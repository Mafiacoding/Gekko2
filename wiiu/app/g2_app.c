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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "core/hw/gs_async.h"
#include "core/hw/vu.h"

extern int gekko2_wiiu_iw;
extern uint64_t ee_iw_stats[4];
extern int gekko2_prof_vu_enabled;
extern uint64_t gekko2_prof_vu1_ns, gekko2_prof_vu1_calls;
extern int gekko2_prof_gs_enabled;
extern uint64_t gekko2_prof_gs_ns, gekko2_prof_gs_calls;
static uint64_t prof_run_ns;
static uint64_t gekko2_vu0_pairs(void) { return ee_core_get_state()->vu0_instructions_executed; }
static uint64_t gekko2_vu1_pairs(void) { return vu1_get_state()->instructions_executed; }
static uint64_t prof_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

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
    gekko2_prof_gs_enabled = 1;
    gekko2_prof_vu_enabled = 1;
    gekko2_wiiu_iw = config->fast_run && !config->use_jit;
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
    if (!config->mask_preset) {
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
        /* Software TLB translation cache ("Fastmem RAM page cache"): host-measured 25% less time to the first
         * BIOS menu with an identical picture; it replaces a 48-entry TLB scan done for every fetch and access. */
        gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_FASTMEM);
    }
#ifdef GEKKO2_WIIU_JIT
    else if (gekko2_optimization_mask & GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)) {
        if (gekko2_codegen_area(NULL)) {
            say(platform, "EE dynarec on (Wii U codegen area)");
        } else {
            gekko2_optimization_mask &= ~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS));
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

    uint64_t last_present = 0, last_report = 0, last_stat_ms = 0, last_stat_instr = 0, last_vb = 0;
    uint16_t last_pad_pressed = 0;
    FILE *session = config->session_log ? fopen(config->session_log, "a") : NULL;
    if (session) {
        fprintf(session, "# session bios=%s disc=%s mask=%08x batch=%u\n# t_s,ee_instr,ips,vblank_total,vblank_per_s(59.94=100%%),frames_presented,pad_mask\n",
                config->bios_path ? config->bios_path : "?", config->iso_path ? config->iso_path : "(none)",
                (unsigned)gekko2_optimization_mask, config->batch_slices ? config->batch_slices : 2000u);
        fflush(session);
    }
    const uint64_t start = platform->now_ms ? platform->now_ms(platform->user) : 0;
    int gs_async_active = 0;
    if (config->gs_async && platform->start_worker) {
        gs_async_attach(platform->worker_idle);
        if (platform->start_worker(gs_async_worker_main, platform->user) == 0 && gs_async_enable() == 0) {
            gs_async_active = 1;
            say(platform, "GS worker thread active (GIF/GS work runs on a second core)");
        } else {
            gs_async_detach();
            say(platform, "GS worker thread could not be started; GIF/GS stays on the emulation thread");
        }
    }
    unsigned batch = config->batch_slices ? config->batch_slices : 2000u;
    while ((!config->max_batches || result->batches < config->max_batches) &&
           (!platform->keep_running || platform->keep_running(platform->user))) {
        if (platform->poll_pad) {
            uint16_t pressed = 0;
            uint8_t axes[4] = { 0x80, 0x80, 0x80, 0x80 };
            if (platform->poll_pad(platform->user, &pressed, axes) == 0) {
                last_pad_pressed = pressed;
                iop_sio2_pad_set_buttons(pressed);
                iop_sio2_pad_set_analog_axes(axes[0], axes[1], axes[2], axes[3]);
            }
        }
        {
            const uint64_t r0 = prof_now();
            system_run_interleaved(batch);
            prof_run_ns += prof_now() - r0;
        }
        result->batches++;
        ee_state_t *e = ee_core_get_state();
        iop_state_t *i = iop_core_get_state();
        result->ee_instructions = e->instructions_executed;
        result->iop_instructions = i->instructions_executed;
        result->ee_pc = e->pc;
        uint64_t now = platform->now_ms ? platform->now_ms(platform->user) : 0;
        if (now - last_report >= 1000u || result->batches == 1) {
            last_report = now;
            const uint64_t dt = now - last_stat_ms;
            const uint64_t ips = dt ? (e->instructions_executed - last_stat_instr) * 1000u / dt : 0;
            last_stat_ms = now;
            last_stat_instr = e->instructions_executed;
            const uint64_t vb = ee_core_get_vblank_events();
            const uint64_t vb_rate = dt ? (vb - last_vb) * 1000000u / dt : 0; /* guest VBlanks per second x1000 (59.94 = full NTSC speed) */
            const uint64_t speed_pct = vb_rate * 100u / 59940u;
            last_vb = vb;
            say(platform, "t=%llus batches=%llu EE pc=%08x instr=%llu IOP instr=%llu ips=%llu vb/s=%llu.%02llu %llu%%",
                (unsigned long long)((now - start) / 1000u), (unsigned long long)result->batches, e->pc,
                (unsigned long long)e->instructions_executed, (unsigned long long)i->instructions_executed,
                (unsigned long long)ips, (unsigned long long)(vb_rate / 1000u), (unsigned long long)(vb_rate % 1000u / 10u),
                (unsigned long long)speed_pct);
            {
                /* Share of emulation wall time spent in GIF/GS work (cumulative). Upper bound for what a GS thread could save. */
                const uint64_t gs_pm = prof_run_ns ? gekko2_prof_gs_ns * 1000u / prof_run_ns : 0;
                const uint64_t vu_pm = prof_run_ns ? gekko2_prof_vu1_ns * 1000u / prof_run_ns : 0;
                say(platform, "PROF iw=%llu%% (runs=%llu) ", (unsigned long long)(e->instructions_executed ? ee_iw_stats[1] * 100u / e->instructions_executed : 0), (unsigned long long)ee_iw_stats[0]);
                say(platform, "PROF gs=%llu.%llu%% vu1=%llu.%llu%% of emulation time (gif calls=%llu, vu1 runs=%llu); VU pairs so far: vu0=%llu vu1=%llu",
                    (unsigned long long)(gs_pm / 10u), (unsigned long long)(gs_pm % 10u), (unsigned long long)(vu_pm / 10u),
                    (unsigned long long)(vu_pm % 10u), (unsigned long long)gekko2_prof_gs_calls, (unsigned long long)gekko2_prof_vu1_calls,
                    (unsigned long long)gekko2_vu0_pairs(), (unsigned long long)gekko2_vu1_pairs());
                if (session)
                    fprintf(session, "# prof t=%llus gs_permille=%llu vu1_permille=%llu gif_calls=%llu vu1_runs=%llu vu0_pairs=%llu vu1_pairs=%llu\n",
                            (unsigned long long)((now - start) / 1000u), (unsigned long long)gs_pm, (unsigned long long)vu_pm,
                            (unsigned long long)gekko2_prof_gs_calls, (unsigned long long)gekko2_prof_vu1_calls,
                            (unsigned long long)gekko2_vu0_pairs(), (unsigned long long)gekko2_vu1_pairs());
            }
            if (session) {
                fprintf(session, "%llu,%llu,%llu,%llu,%llu.%02llu,%u,%04x\n", (unsigned long long)((now - start) / 1000u),
                        (unsigned long long)e->instructions_executed, (unsigned long long)ips, (unsigned long long)vb,
                        (unsigned long long)(vb_rate / 1000u), (unsigned long long)(vb_rate % 1000u / 10u),
                        result->frames_presented, (unsigned)last_pad_pressed);
                fflush(session);
                fsync(fileno(session));
            }
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
            say(platform, "  JIT %s exec=%llu comp=%llu blk=%llu cache=%uKiB sw=%llu", gekko2_codegen_status(),
                (unsigned long long)ee_jit_get_executed_count(), (unsigned long long)ee_jit_get_compile_attempt_count(),
                (unsigned long long)ee_jit_get_block_count(), (unsigned)(ppc_code_cache_used() / 1024u), gekko2_codegen_switch_count());
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
    if (gs_async_active) {
        uint64_t st[8];
        gs_async_stats(st);
        say(platform, "GS worker: entries=%llu qwords=%llu drains=%llu fallbacks=%llu ring-full waits=%llu", (unsigned long long)st[0],
            (unsigned long long)st[1], (unsigned long long)st[2], (unsigned long long)st[3], (unsigned long long)st[4]);
        say(platform, "GS worker: busy=%llu ms, emulation thread waited %llu ms in drains", (unsigned long long)(st[6] / 1000000u),
            (unsigned long long)(st[5] / 1000000u));
        gs_async_detach();
    }
    if (session)
        fclose(session);
    say(platform, "done: batches=%llu frames=%u EE instr=%llu", (unsigned long long)result->batches,
        result->frames_presented, (unsigned long long)result->ee_instructions);
    return result->status = G2_OK;
}
