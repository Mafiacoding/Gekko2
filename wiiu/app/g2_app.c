/* GPL-3.0-or-later. See g2_app.h. */
#include "g2_app.h"
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/gs.h"
#include "core/hw/ee_intc.h"
#include "core/hw/iop_intc.h"
#include "core/hw/sif.h"
#include "core/hw/iop_sio2.h"
#include "core/recompiler/native_ppc.h"
#include "core/recompiler/optimization.h"
#ifdef GEKKO2_WIIU_JIT
#include "core/recompiler/ee_jit.h"
#include "core/recompiler/ppc_code_cache.h"
#endif
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include <stdio.h>
#include <string.h>

static void say(const g2_platform *p, const char *format, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void say(const g2_platform *p, const char *format, ...)
{
    char line[160];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (p->log)
        p->log(p->user, line);
}

int g2_read_display(uint32_t *xrgb, unsigned w_max, unsigned h_max, unsigned *w_out, unsigned *h_out)
{
    gs_state_t *g = gs_get_state();
    for (unsigned c = 0; c < 2; c++) {
        if (!(g->pmode & (1u << c)))
            continue;
        uint64_t fb = c ? g->dispfb2 : g->dispfb1, display = c ? g->display2 : g->display1;
        uint32_t bp, bw, x, y, w, h;
        gs_decode_dispfb(fb, &bp, &bw);
        gs_decode_display_region(fb, display, g->smode2, &x, &y, &w, &h);
        if (!w || !h || w > w_max || h > h_max || !bw)
            continue;
        unsigned psm = (fb >> 15) & 31u;
        if (psm != 0 && psm != 1 && psm != 2 && psm != 10)
            continue;
        for (unsigned row = 0; row < h; row++) {
            for (unsigned col = 0; col < w; col++) {
                uint32_t pixel;
                if (psm == 2 || psm == 10) {
                    uint16_t p = psm == 10 ? gs_mem_read_psmct16s(bp, bw, x + col, y + row)
                                           : gs_mem_read_psmct16(bp, bw, x + col, y + row);
                    pixel = ((p & 31u) << 3) | (((p >> 5) & 31u) << 11) | (((p >> 10) & 31u) << 19);
                } else {
                    pixel = gs_mem_read_psmct32(bp, bw, x + col, y + row);
                }
                /* GS stores 0xAABBGGRR; present() wants 0x00RRGGBB. */
                xrgb[(size_t)row * w + col] = ((pixel & 0xffu) << 16) | (pixel & 0xff00u) | ((pixel >> 16) & 0xffu);
            }
        }
        *w_out = w;
        *h_out = h;
        return 1;
    }
    return 0;
}

int g2_run(const g2_config *config, const g2_platform *platform, g2_result *result)
{
    static uint32_t frame[1024u * 1024u];            /* largest frame accepted by g2_read_display() */
    g2_result local;
    memset(&local, 0, sizeof local);
    if (!result)
        result = &local;
    memset(result, 0, sizeof *result);

    bios_image_t bios;
    memset(&bios, 0, sizeof bios);
    say(platform, "loading BIOS: %s", config->bios_path);
    if (bios_load(config->bios_path, &bios) != 0) {
        say(platform, "BIOS could not be loaded (missing, wrong size or no ROMVER)");
        return result->status = G2_ERR_BIOS;
    }
    say(platform, "BIOS ok: %s (%u bytes)", bios.version_string, (unsigned)bios.size);
#ifdef GEKKO2_WIIU_JIT
    if (config->use_jit) {
        /* The codegen area is only usable from the codegen core; ask now, on the emulator thread. */
        if (gekko2_codegen_area(NULL)) {
            gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
            say(platform, "EE dynarec requested (Wii U codegen area)");
        } else {
            say(platform, "EE dynarec NOT available: %s - interpreter only", gekko2_codegen_status());
        }
    }
#endif
    if (system_init(&bios, &bios) != 0) {
        say(platform, "system_init failed");
        return result->status = G2_ERR_INIT;
    }
    if (config->iso_path && config->iso_path[0]) {
        int a = iop_cdvd_mount_iso(config->iso_path), b = iop_cdrom_legacy_mount_iso(config->iso_path);
        if (a && b) {
            say(platform, "disc image could not be mounted: %s", config->iso_path);
            return result->status = G2_ERR_DISC;
        }
        if (!a)
            iop_cdvd_set_disc_present(0x12);
        say(platform, "disc mounted");
    }

    uint64_t last_present = 0, last_report = 0;
    const uint64_t start = platform->now_ms ? platform->now_ms(platform->user) : 0;
    unsigned batch = config->batch_slices ? config->batch_slices : 2000u;
    while ((!config->max_batches || result->batches < config->max_batches) &&
           (!platform->keep_running || platform->keep_running(platform->user))) {
        if (platform->poll_pad) {
            uint16_t pressed = 0;
            uint8_t axes[4] = { 0x80, 0x80, 0x80, 0x80 };
            if (platform->poll_pad(platform->user, &pressed, axes) == 0) {
                iop_sio2_pad_set_buttons(pressed);
                iop_sio2_pad_set_analog_axes(axes[0], axes[1], axes[2], axes[3]);
            }
        }
        system_run_interleaved(batch);
        result->batches++;
        ee_state_t *e = ee_core_get_state();
        iop_state_t *i = iop_core_get_state();
        result->ee_instructions = e->instructions_executed;
        result->iop_instructions = i->instructions_executed;
        result->ee_pc = e->pc;
        uint64_t now = platform->now_ms ? platform->now_ms(platform->user) : 0;
        if (now - last_report >= 1000u || result->batches == 1) {
            last_report = now;
            say(platform, "t=%llus batches=%llu EE pc=%08x instr=%llu IOP instr=%llu",
                (unsigned long long)((now - start) / 1000u), (unsigned long long)result->batches, e->pc,
                (unsigned long long)e->instructions_executed, (unsigned long long)i->instructions_executed);
            say(platform, "  EE halted=%u sr=%08x cause=%08x ra=%08x | IOP pc=%08x halted=%u",
                (unsigned)e->halted, e->cop0[12], e->cop0[13], (unsigned)e->gpr[31].ud0, i->pc,
                (unsigned)i->halted);
            if ((now - start) / 1000u % 5u == 0u) {
                const ee_intc_state_t *ei = ee_intc_get_state();
                const iop_intc_state_t *ii = iop_intc_get_state();
                const sif_state_t *sf = sif_get_state();
                say(platform, "  cnt=%08x cmp=%08x EEINTC %04x/%04x", e->cop0[9], e->cop0[11], ei->stat, ei->mask);
                say(platform, "  IOPINTC %08x/%08x", ii->istat, ii->imask);
                say(platform, "  SIF ms=%08x sm=%08x msflag=%08x smflag=%08x ctrl=%08x", sf->mscom, sf->smcom,
                    sf->msflag, sf->smflag, sf->ctrl);
            }
#ifdef GEKKO2_WIIU_JIT
            say(platform, "  JIT %s exec=%llu compiles=%llu blocks=%llu cache=%uKiB", gekko2_codegen_status(),
                (unsigned long long)ee_jit_get_executed_count(), (unsigned long long)ee_jit_get_compile_attempt_count(),
                (unsigned long long)ee_jit_get_block_count(), (unsigned)(ppc_code_cache_used() / 1024u));
#endif
            if (e->halted)
                say(platform, "  EE halt: %.100s", e->halt_reason);
            if (i->halted)
                say(platform, "  IOP halt: %.100s", i->halt_reason);
        }
        if (platform->present && now - last_present >= config->present_interval_ms) {
            unsigned w = 0, h = 0;
            if (g2_read_display(frame, 1024u, 1024u, &w, &h)) {
                last_present = now;
                if (platform->present(platform->user, frame, w, h) == 0) {
                    result->frames_presented++;
                    result->last_w = w;
                    result->last_h = h;
                }
            }
        }
        if (e->halted || i->halted) {
            result->halted = 1;
            say(platform, "HALT %s | %s", e->halt_reason, i->halt_reason);
            break;
        }
    }
    say(platform, "done: batches=%llu frames=%u EE instr=%llu", (unsigned long long)result->batches,
        result->frames_presented, (unsigned long long)result->ee_instructions);
    return result->status = G2_OK;
}
