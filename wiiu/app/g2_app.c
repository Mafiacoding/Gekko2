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
#include "core/iop_async.h"
#include <stdlib.h>

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
static FILE *say_file; /* verbose log: every status line is also written to the SD session log */
static void say(const g2_platform *p, const char *format, ...)
{
    char line[160];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (p->log)
        p->log(p->user, line);
    if (say_file) {
        fprintf(say_file, "# log %s\n", line);
        fflush(say_file);
    }
}

int gekko2_wiiu_display_sync;
char gekko2_wiiu_disp_info[160];
int g2_read_display(uint32_t *xrgb, unsigned w_max, unsigned h_max, unsigned *w_out, unsigned *h_out)
{
    gs_state_t *g = gs_get_state();
    {
        /* Experiment (display_sync.txt): resolve all GPU-owned GS memory before the scanout read. */
        extern int gekko2_wiiu_display_sync;
        if (gekko2_wiiu_display_sync)
            gs_mem_sync();
    }
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
        {
            extern char gekko2_wiiu_disp_info[160];
            snprintf(gekko2_wiiu_disp_info, sizeof gekko2_wiiu_disp_info,
                     "pmode=%x circ=%u bp=%x bw=%u psm=%u x=%u y=%u w=%u h=%u smode2=%llx display=%llx", (unsigned)g->pmode,
                     c, (unsigned)bp, (unsigned)bw, psm, (unsigned)x, (unsigned)y, (unsigned)w, (unsigned)h,
                     (unsigned long long)g->smode2, (unsigned long long)display);
        }
        return 1;
    }
    return 0;
}

int g2_run(const g2_config *config, const g2_platform *platform, g2_result *result)
{
    gekko2_prof_gs_enabled = 1;
    gekko2_prof_vu_enabled = 1;
    gekko2_wiiu_iw = config->fast_run && !config->use_jit; /* final value below, once the JIT state is known */
    gekko2_wiiu_event_sched = config->event_sched || config->iop_async;
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
                if (!config->vu_jit_off) gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);
                say(platform, "EE dynarec requested (Wii U codegen area)%s", config->vu_jit_off ? "" : ", VU dynarec on");
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
            /* The menu mask (optimization.cfg) keeps native EE blocks off by default, as on the Wii. Without them
             * the dynarec only translates single instructions (tester logs: blocks=0), so the JIT app turns them on. */
            gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
            {
                /* jit_exp.txt: extra optimization bits for measurements (bit 0 ignored) */
                extern int gekko2_wiiu_jit_exp;
                gekko2_optimization_mask |= (uint32_t)gekko2_wiiu_jit_exp & ~1u;
            }
            if (!config->vu_jit_off) gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);
            else gekko2_optimization_mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);
            say(platform, "EE dynarec on (Wii U codegen area)%s", config->vu_jit_off ? "" : ", VU dynarec on");
        } else {
            gekko2_optimization_mask &= ~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS) | GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT));
            say(platform, "EE dynarec NOT available: %s - interpreter only", gekko2_codegen_status());
        }
    }
#endif
#ifndef GEKKO2_WIIU_JIT
    /* The interpreter app cannot run generated code. The default mask (fresh SD card without optimization.cfg) has
     * the EE JIT bit set, which switched the interpreted fast runs off in this app (Cemu: iw=0%). */
    gekko2_optimization_mask &= ~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS) |
                                  GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT));
#endif
    /* Interpreted fast runs execute before the EE dynarec's block path in ee_core_step_n(): with the dynarec on
     * they must be off, otherwise most instructions never reach the JIT (tester logs: 0 native blocks, iw=83%).
     * The JIT app enables the dynarec through the menu mask (use_jit = 0), so decide on the final mask. */
    gekko2_wiiu_iw = config->fast_run && !gekko2_opt_enabled(GEKKO2_OPT_EE_JIT);
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
        fprintf(session, "# gekko2 %s build=%s compiled=%s %s\n", G2_WIIU_VERSION,
#ifdef GEKKO2_WIIU_JIT
                "jit",
#else
                "interpreter",
#endif
                __DATE__, __TIME__);
        fprintf(session, "# config gs_async=%d vu1_async=%d iop_async=%d event_sched=%d fast_run=%d vu_jit_off=%d gx2_gs=%d gx2_split=%d\n",
                config->gs_async, config->vu1_async, config->iop_async, config->event_sched, config->fast_run, config->vu_jit_off,
                config->gx2_gs, config->gx2_split);
        say_file = session;
        fprintf(session, "# session bios=%s disc=%s mask=%08x batch=%u\n# t_s,ee_instr,ips,vblank_total,vblank_per_s(59.94=100%%),frames_presented,pad_mask\n",
                config->bios_path ? config->bios_path : "?", config->iso_path ? config->iso_path : "(none)",
                (unsigned)gekko2_optimization_mask, config->batch_slices ? config->batch_slices : 2000u);
        fflush(session);
    }
    if (config->iso_path && config->iso_path[0]) {
        /* Disc diagnostics for tester logs (after the session file is open so they reach it): a 4 GiB file on
         * FAT32 is a cut-off image (use CHD instead). */
        FILE *df = fopen(config->iso_path, "rb");
        if (df) {
            unsigned long long bytes = 0;
            if (fseeko(df, 0, SEEK_END) == 0) {
                off_t end = ftello(df);
                bytes = end > 0 ? (unsigned long long)end : 0ull;
            }
            fclose(df);
            say(platform, "disc image size=%llu bytes (%llu MiB)", bytes, bytes >> 20);
            if (bytes >= 0xFFFF0000ull && bytes <= 0xFFFFFFFFull)
                say(platform, "WARNING: disc image is exactly at the FAT32 4 GiB limit; it is probably cut off. Convert it to CHD.");
        }
    }
    const uint64_t start = platform->now_ms ? platform->now_ms(platform->user) : 0;
    int gs_async_active = 0, gx2_gs_active = 0;
#ifdef GEKKO2_WIIU_GX2
    {
        /* GX2 may only be used from the core that initialised it: GPU GS drawing keeps GIF/GS on this thread,
         * unless the platform runs the GS worker on the GX2 core (gx2_split). */
        extern int gekko2_gx2_gs_attach(void);
        if (config->gx2_gs) {
            gx2_gs_active = gekko2_gx2_gs_attach();
            if (gx2_gs_active) /* CPU writes outside the GPU-owned buffers must not force a GPU resolve */
                gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_GX_DISJOINT_WRITES);
            say(platform, gx2_gs_active ? "GPU GS renderer on (GX2 draws triangles and sprites)"
                                        : "GPU GS renderer could not start; software GS");
            extern const char *gx2_gs_depth_status(void);
            if (gx2_gs_active)
                say(platform, "GPU GS %s", gx2_gs_depth_status());
        }
    }
#endif
    if (config->gs_async && (!gx2_gs_active || config->gx2_split) && platform->start_worker) {
        gs_async_attach(platform->worker_idle);
        if (platform->start_worker(gs_async_worker_main, platform->user) == 0 && gs_async_enable() == 0) {
            gs_async_active = 1;
            say(platform, "GS worker thread active (GIF/GS work runs on a second core)");
        } else {
            gs_async_detach();
            say(platform, "GS worker thread could not be started; GIF/GS stays on the emulation thread");
        }
    }
    int vu1_async_active = 0;
    if (config->vu1_async && gs_async_active && platform->start_worker_vu) {
        vu1_async_attach(platform->worker_idle);
        if (platform->start_worker_vu(vu1_async_worker_main, platform->user) == 0 && vu1_async_enable() == 0) {
            vu1_async_active = 1;
            say(platform, "VU1 worker thread active (VU1 microprograms run on the third core)");
        } else {
            vu1_async_detach();
            say(platform, "VU1 worker thread could not be started; VU1 stays on the emulation thread");
        }
    }
    int iop_async_active = 0;
    if (config->iop_async && platform->start_worker_stack) {
        static void *iop_stack;
        const unsigned iop_stack_size = 512u * 1024u;
        if (!iop_stack)
            iop_stack = malloc(iop_stack_size);
        if (iop_stack) {
            system_iop_async_attach(platform->worker_idle, iop_stack, (char *)iop_stack + iop_stack_size);
            if (platform->start_worker_stack(system_iop_worker_main, platform->user, iop_stack, iop_stack_size) == 0 &&
                system_iop_async_enable() == 0) {
                iop_async_active = 1;
                say(platform, "IOP worker thread active (IOP catch-up runs on its own core)");
            } else {
                system_iop_async_detach();
                say(platform, "IOP worker thread could not be started; IOP stays on the emulation thread");
            }
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
                {
                    system_profile_t sp;
                    system_profile_get(&sp);
                    const uint64_t tot = sp.ee_ticks + sp.iop_ticks;
                    const uint64_t iop_pm = tot ? sp.iop_ticks * 1000u / tot : 0;
                    say(platform, "PROF iw=%llu%% (runs=%llu) iop=%llu.%llu%% of EE+IOP time (samples=%llu)",
                        (unsigned long long)(e->instructions_executed ? ee_iw_stats[1] * 100u / e->instructions_executed : 0),
                        (unsigned long long)ee_iw_stats[0], (unsigned long long)(iop_pm / 10u), (unsigned long long)(iop_pm % 10u),
                        (unsigned long long)sp.samples);
                    if (session)
                        fprintf(session, "# prof2 t=%llus iw_pct=%llu iop_permille=%llu samples=%llu\n", (unsigned long long)((now - start) / 1000u),
                                (unsigned long long)(e->instructions_executed ? ee_iw_stats[1] * 100u / e->instructions_executed : 0),
                                (unsigned long long)iop_pm, (unsigned long long)sp.samples);
                }
                if (vu1_async_active) {
                    uint64_t vst[3];
                    vu1_async_stats(vst);
                    const uint64_t vw = prof_run_ns ? vst[1] * 1000u / prof_run_ns : 0;
                    say(platform, "PROF vu1 worker busy=%llu.%llu%% of emulation time (runs=%llu, GIF waits=%llu)", (unsigned long long)(vw / 10u),
                        (unsigned long long)(vw % 10u), (unsigned long long)vst[0], (unsigned long long)vst[2]);
                    if (session)
                        fprintf(session, "# prof3 t=%llus vu1w_permille=%llu vu1w_runs=%llu vu1w_gif_waits=%llu\n", (unsigned long long)((now - start) / 1000u),
                                (unsigned long long)vw, (unsigned long long)vst[0], (unsigned long long)vst[2]);
                }
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
#ifdef GEKKO2_WIIU_GX2
            if (gx2_gs_active && session) {
                extern uint64_t gekko2_gx2_gs_stat(unsigned i);
                fprintf(session, "# gpu t=%llus tris=%llu sprites=%llu rejects=%llu uploads=%llu resolves=%llu tex=%llu tex_reused=%llu up_px=%llu rb_px=%llu\n",
                        (unsigned long long)((now - start) / 1000u), (unsigned long long)gekko2_gx2_gs_stat(0),
                        (unsigned long long)gekko2_gx2_gs_stat(1), (unsigned long long)gekko2_gx2_gs_stat(4),
                        (unsigned long long)gekko2_gx2_gs_stat(2), (unsigned long long)gekko2_gx2_gs_stat(3),
                        (unsigned long long)gekko2_gx2_gs_stat(5), (unsigned long long)gekko2_gx2_gs_stat(6),
                        (unsigned long long)gekko2_gx2_gs_stat(8), (unsigned long long)gekko2_gx2_gs_stat(9));
            }
#endif
            {
                /* Disc read health: requests and failed reads (ISO/BIN) and CHD hunk reads/failures. */
                extern uint64_t iso_cache_stat(unsigned index);
                extern uint64_t gekko2_chd_stat(unsigned index);
                static uint64_t reported_fails;
                const uint64_t fails = iso_cache_stat(4) + gekko2_chd_stat(3);
                if (fails > reported_fails) {
                    say(platform, "disc read errors: %llu (image damaged, cut off or unsupported layout?)", (unsigned long long)fails);
                    reported_fails = fails;
                }
                {
                    /* New CDVD commands since the last report (last 64 kept), so a hang shows the command before it. */
                    extern uint32_t iop_cdvd_trace_count(void);
                    extern int iop_cdvd_trace_text(uint32_t seq, char *out, unsigned size);
                    static uint32_t cdvd_seen;
                    const uint32_t n = iop_cdvd_trace_count();
                    if (session && n != cdvd_seen) {
                        uint32_t from = n - cdvd_seen > 64u ? n - 64u : cdvd_seen;
                        fprintf(session, "# cdvd t=%llus total=%u", (unsigned long long)((now - start) / 1000u), (unsigned)n);
                        for (uint32_t q = from; q < n; q++) {
                            char t[48];
                            if (iop_cdvd_trace_text(q, t, sizeof t))
                                fprintf(session, " %s", t);
                        }
                        fputc('\n', session);
                        cdvd_seen = n;
                    }
                }
                if (session && ((now - start) / 1000u) % 10u == 0u)
                    fprintf(session, "# disc t=%llus iso_reads=%llu iso_fail=%llu chd_reads=%llu chd_fail=%llu\n",
                            (unsigned long long)((now - start) / 1000u), (unsigned long long)iso_cache_stat(0),
                            (unsigned long long)iso_cache_stat(4), (unsigned long long)gekko2_chd_stat(0),
                            (unsigned long long)gekko2_chd_stat(3));
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
            {
                extern uint32_t gekko2_gx2_gs_target_info(unsigned i);
                say(platform, "  DISP %s | GPU target fbp=%x fbw=%u h=%u", gekko2_wiiu_disp_info, (unsigned)gekko2_gx2_gs_target_info(0),
                    (unsigned)gekko2_gx2_gs_target_info(1), (unsigned)gekko2_gx2_gs_target_info(2));
            }
            say(platform, "  EE v0=%08x v1=%08x a0=%08x a1=%08x a2=%08x a3=%08x", (unsigned)e->gpr[2].ud0,
                (unsigned)e->gpr[3].ud0, (unsigned)e->gpr[4].ud0, (unsigned)e->gpr[5].ud0, (unsigned)e->gpr[6].ud0,
                (unsigned)e->gpr[7].ud0);
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
            if (session) {
                /* Share of EE instructions retired by native code vs. all, VU dynarec blocks, code switches. */
                extern uint64_t vu_jit_get_block_count(void);
                extern uint64_t vu_jit_get_pair_count(void);
                fprintf(session, "# jit t=%llus native_retired=%llu block_retired=%llu ee_instr=%llu blocks=%llu evictions=%llu "
                                 "compiles=%llu cache_kib=%u codegen_switches=%llu vu_blocks=%llu vu_pairs=%llu\n",
                        (unsigned long long)((now - start) / 1000u), (unsigned long long)ee_jit_get_native_retired_count(),
                        (unsigned long long)ee_jit_get_block_retired(), (unsigned long long)e->instructions_executed,
                        (unsigned long long)ee_jit_get_block_count(), (unsigned long long)ee_jit_get_block_evictions(),
                        (unsigned long long)ee_jit_get_compile_attempt_count(), (unsigned)(ppc_code_cache_used() / 1024u),
                        gekko2_codegen_switch_count(), (unsigned long long)vu_jit_get_block_count(),
                        (unsigned long long)vu_jit_get_pair_count());
            }
            if (session) {
                extern uint64_t gekko2_wiiu_block_diag[16];
                jit_cache_profile cp;
                ee_jit_get_cache_profile(&cp);
                fprintf(session, "# jitb t=%llus", (unsigned long long)((now - start) / 1000u));
                for (unsigned k = 0; k < 16u; k++)
                    fprintf(session, " %u:%llu", k, (unsigned long long)gekko2_wiiu_block_diag[k]);
                {
                    extern void gekko2_codegen_switch_sites(char *out, unsigned size);
                    char sites[320];
                    gekko2_codegen_switch_sites(sites, sizeof sites);
                    fprintf(session, " rw_sites%s", sites);
                }
                fprintf(session, " lookups=%llu hits=%llu misses=%llu stale=%llu installed=%llu failures=%llu\n",
                        (unsigned long long)cp.lookups, (unsigned long long)cp.hits, (unsigned long long)cp.misses,
                        (unsigned long long)cp.stale, (unsigned long long)cp.installed, (unsigned long long)cp.failures);
                {
                    extern char gekko2_wiiu_jit_dump[8192];
                    if (gekko2_wiiu_jit_dump[0]) {
                        fprintf(session, "%s\n", gekko2_wiiu_jit_dump);
                        gekko2_wiiu_jit_dump[0] = 0;
                    }
                }
            }
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
    if (vu1_async_active) {
        uint64_t st[3];
        vu1_async_stats(st);
        say(platform, "VU1 worker: runs=%llu busy=%llu ms GIF waits=%llu", (unsigned long long)st[0], (unsigned long long)(st[1] / 1000000u),
            (unsigned long long)st[2]);
        vu1_async_detach();
    }
    if (iop_async_active) {
        unsigned long long st[4];
        system_iop_async_stats(st);
        say(platform, "IOP worker: jobs=%llu ticks=%llu emulation waits=%llu worker shared waits=%llu", st[0], st[1], st[2], st[3]);
        system_iop_async_detach();
    }
#ifdef GEKKO2_WIIU_GX2
    if (gx2_gs_active) {
        extern uint64_t gekko2_gx2_gs_stat(unsigned i);
        extern void gekko2_gx2_gs_detach(void);
        gekko2_gx2_gs_detach();
        say(platform, "GPU GS: triangles=%llu sprites=%llu software-rejects=%llu uploads=%llu resolves=%llu textures=%llu reused=%llu",
            (unsigned long long)gekko2_gx2_gs_stat(0), (unsigned long long)gekko2_gx2_gs_stat(1), (unsigned long long)gekko2_gx2_gs_stat(4),
            (unsigned long long)gekko2_gx2_gs_stat(2), (unsigned long long)gekko2_gx2_gs_stat(3), (unsigned long long)gekko2_gx2_gs_stat(5),
            (unsigned long long)gekko2_gx2_gs_stat(6));
        extern uint64_t gekko2_gx2_gs_reason(unsigned i);
        char rs[200];
        int n = 0;
        for (unsigned k = 0; k < 14 && n < (int)sizeof rs - 24; k++)
            n += snprintf(rs + n, sizeof rs - (size_t)n, " %u:%llu", k, (unsigned long long)gekko2_gx2_gs_reason(k));
        say(platform, "GPU GS reject reasons:%s", rs);
        extern uint64_t gekko2_gx2_gs_cause(unsigned i);
        say(platform, "GPU GS resolve causes: texture overlap=%llu target switch=%llu other CPU access=%llu",
            (unsigned long long)gekko2_gx2_gs_cause(0), (unsigned long long)gekko2_gx2_gs_cause(1), (unsigned long long)gekko2_gx2_gs_cause(2));
#ifdef G2_HOST_PROF
        extern uint64_t gekko2_gx2_gs_ns(unsigned i);
        say(platform, "GPU GS host time ms: upload=%llu resolve=%llu texdecode=%llu draw=%llu",
            (unsigned long long)(gekko2_gx2_gs_ns(0) / 1000000u), (unsigned long long)(gekko2_gx2_gs_ns(1) / 1000000u),
            (unsigned long long)(gekko2_gx2_gs_ns(2) / 1000000u), (unsigned long long)(gekko2_gx2_gs_ns(3) / 1000000u));
#endif
    }
#endif
    if (gs_async_active) {
        uint64_t st[8];
        gs_async_stats(st);
        say(platform, "GS worker: entries=%llu qwords=%llu drains=%llu fallbacks=%llu ring-full waits=%llu", (unsigned long long)st[0],
            (unsigned long long)st[1], (unsigned long long)st[2], (unsigned long long)st[3], (unsigned long long)st[4]);
        say(platform, "GS worker: busy=%llu ms, emulation thread waited %llu ms in drains", (unsigned long long)(st[6] / 1000000u),
            (unsigned long long)(st[5] / 1000000u));
        gs_async_detach();
    }
    if (session) {
        say_file = NULL;
        fclose(session);
    }
    say(platform, "done: batches=%llu frames=%u EE instr=%llu", (unsigned long long)result->batches,
        result->frames_presented, (unsigned long long)result->ee_instructions);
    return result->status = G2_OK;
}
