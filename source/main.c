#include "core/hw/game_trace.h"
#include "core/chd_reader.h"
#include "build_version.h"
#include "core/recompiler/experimental_options.h"
#include "core/hw/iop_dma.h"
#include "core/hw/audio_output.h"
#include "core/hw/controller_input.h"
#include "core/hw/spu2_mixer.h"
#include "core/hw/iop_spu2.h"
#include "core/hw/iop_service_hle.h"
#include "core/hw/rspu2_stream.h"
#include "core/hw/arm_ram.h"
#include "core/hw/gs_gx_dump.h"
#include "core/hw/guest_error_probe.h"
#include "core/hw/gs_gx_additive.h"
#include "core/runtime_profile.h"
#include "core/recompiler/dynarec_config.h"
#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/iop_lightrec.h"
#include "core/recompiler/dsi_fastmem.h"
#ifdef GEKKO2_IOP_LIGHTREC_PILOT
#ifdef GEKKO2_GX_NATIVE_ADDITIVE
#ifdef GEKKO2_EE_AOT
#ifdef GEKKO2_GX_NATIVE_BIOS_TRI
#define GEKKO2_BUILD_NAME "" GEKKO2_RELEASE "-GPU early-reject / STQ beta"
#else
#define GEKKO2_BUILD_NAME "" GEKKO2_RELEASE "-CPU triangle-cache control beta"
#endif
#else
#define GEKKO2_BUILD_NAME "" GEKKO2_RELEASE "-B GX destination-alpha correction graphics beta"
#endif
#else
#define GEKKO2_BUILD_NAME "" GEKKO2_RELEASE "-A GX destination-alpha correction control beta"
#endif
#define GEKKO2_BUILD_LIGHTREC 1
#else
#define GEKKO2_BUILD_NAME "" GEKKO2_RELEASE "-A"
#define GEKKO2_BUILD_LIGHTREC 0
#endif
#ifdef GEKKO2_DSI_FASTMEM
#define GEKKO2_BUILD_DSI 1
#else
#define GEKKO2_BUILD_DSI 0
#endif
#include "core/recompiler/ppc_dynarec.h"
#include "core/hw/frontend_text.h"
#include "core/hw/frontend_logo.h"
#include "core/hw/frontend_runtime.h"
#include "core/hw/frontend_settings.h"
#include "core/hw/frontend_log.h"
#include "core/recompiler/iop_jit.h"
#include "core/recompiler/vu_jit.h"
/* R1308: native Wii launcher followed by real EE/IOP boot. */
#include <gccore.h>
#include <ogc/conf.h>
#include <wiiuse/wpad.h>
#include "frontend.h"
#include <fat.h>
#include <stdio.h>
#include "core/hw/iop_hle_thread.h"
#include <stdlib.h>
#include <string.h>
#include <ogc/lwp_watchdog.h>

#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/recompiler/ee_jit.h"
#ifdef GEKKO2_EE_AOT
#include "core/recompiler/ee_aot.h"
#endif
#include "core/iop/iop_core.h"
#include "core/system.h"
#include "core/bios_loader.h"
#include "core/hw/cdvd_config.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include "core/hw/gs.h"
#include "core/hw/vif.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
#include "core/hw/gs_gx.h"
#include "core/hw/dma.h"
#include "core/hw/ee_intc.h"
#include "core/hw/ee_timers.h"
#include "core/hw/gif.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/ipu.h"
#include "core/hw/arm_worker.h"
#include "core/hw/arm_loader.h"
#include "core/hw/wii_arm_ios.h"
#include "core/hw/iop_hle_bios.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static int g_present_widescreen;

/* All menu buttons retain libogc PAD bit compatibility. */
static uint16_t g_input_held,g_input_down;
static int g_input_home,g_input_exit;
static controller_focus g_controller;
static void wii_input_scan(void)
{
 controller_host_scan(&g_controller);
 g_input_held=g_controller.pad.menu;g_input_down=g_controller.down;
 g_input_home=g_controller.home_down;
 uint16_t ignored;
 g_input_exit=controller_session(&g_controller,&ignored)==3;
}

static void wii_console_setup(void)
{
    VIDEO_Init();
    controller_focus_init(&g_controller);
    controller_host_init();

    rmode = VIDEO_GetPreferredMode(NULL);
    g_present_widescreen=CONF_GetAspectRatio()==CONF_ASPECT_16_9;
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    CON_Init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
             rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
}

/* ====================================================================
 * Native Wii test-menu drawing helpers (direct XFB pixel writes, YUV
 * packed 2-px-per-word format, via gs_wii_output.c's already-tested
 * gs_rgb8_pair_to_ycbcr() - see that file's header for the citation).
 * Not part of PS2 GS emulation - this writes straight into the real
 * Wii framebuffer, independent of the emulated GS local memory.
 * ==================================================================== */

static inline void put_pixel_pair(uint32_t x_even, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (y >= rmode->xfbHeight || x_even >= rmode->fbWidth) return;
    uint32_t *row = (uint32_t *)((uint8_t *)xfb + (size_t)y * rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    row[x_even / 2] = gs_rgb8_pair_to_ycbcr(r, g, b, r, g, b);
}

static void fill_rect(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b)
{
    x0 &= ~1u;
    if (w & 1u) w++;
    uint32_t y_end = y0 + h;
    uint32_t x_end = x0 + w;
    for (uint32_t y = y0; y < y_end && y < rmode->xfbHeight; y++)
        for (uint32_t x = x0; x < x_end && x < rmode->fbWidth; x += 2)
            put_pixel_pair(x, y, r, g, b);
}

static void draw_gradient_background(uint8_t top_r, uint8_t top_g, uint8_t top_b,
                                      uint8_t bot_r, uint8_t bot_g, uint8_t bot_b)
{
    uint32_t h = rmode->xfbHeight;
    for (uint32_t y = 0; y < h; y++) {
        uint8_t r = (uint8_t)((int)top_r + ((int)bot_r - (int)top_r) * (int)y / (int)h);
        uint8_t g = (uint8_t)((int)top_g + ((int)bot_g - (int)top_g) * (int)y / (int)h);
        uint8_t b = (uint8_t)((int)top_b + ((int)bot_b - (int)top_b) * (int)y / (int)h);
        fill_rect(0, y, rmode->fbWidth, 1, r, g, b);
    }
}

static void flush_screen(void)
{
    DCFlushRange(xfb, rmode->fbWidth * rmode->xfbHeight * VI_DISPLAY_PIX_SZ);
}

/* Approximate character-cell size of libogc's default console font
 * (8x16), used only to line printf-based text labels up next to the
 * pixel-drawn boxes below - not pixel-exact, just close enough for a
 * readable test screen. */
#define CHAR_W 8
#define CHAR_H 16
static void goto_rc(uint32_t px, uint32_t py) { printf("\x1b[%u;%uH", (unsigned)(py / CHAR_H) + 1, (unsigned)(px / CHAR_W) + 1); }

/* The native launcher is independent of the emulated GS framebuffer. */
static int g_boot_disc = 0;
static int g_hud_default = 0;
static int g_hud_active;
static int g_fps_default = 1;
static uint64_t g_gx_attempts,g_gx_fallbacks;
static uint64_t g_sw_attempts,g_sw_copies,g_sw_sync_failures,g_sw_invalid;
static uint64_t g_xfb_updates,g_xfb_guest_copies,g_xfb_hud_updates;
static int g_gx_present = 0; /* One experimental option enables output and supported primitive drawing. */
static int g_gx_requested = 0;
static uint32_t g_fps_milli,g_present_milli;
static frontend_fps_window g_fps_window;
static system_profile_t g_profile_previous;
static uint32_t g_slice_budget=512;
static void wii_exit_to_loader(void);
static int g_throughput = 1;
static void launcher_rect(int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b)
{
    uint32_t sw=rmode->fbWidth,sh=rmode->xfbHeight;
    int left=x*sw/640,top=y*sh/480;
    int right=(x+w)*sw/640,bottom=(y+h)*sh/480;
    /* XFB stores chroma in pairs; ensure thin strokes remain visible. */
    if(right<=left)right=left+2;
    if(bottom<=top)bottom=top+1;
    fill_rect(left,top,right-left,bottom-top,r,g,b);
}
static void launcher_logo(int x,int y)
{frontend_logo_draw(xfb,rmode->fbWidth,rmode->xfbHeight,x,y);}
static void launcher_text(int x,int y,int scale,const char *text,int r,int g,int b)
{
    frontend_text_draw(xfb,rmode->fbWidth,rmode->xfbHeight,x,y,scale,text,r,g,b);
}
static void wait_for_button(uint16_t mask)
{
    for (;;) { VIDEO_WaitVSync(); wii_input_scan(); if(g_input_exit)wii_exit_to_loader(); if((g_input_down&mask)||g_input_home)return; }
}
static int   g_fat_mounted = 0;
static int   g_bios_ok = 0;
static int   g_system_started = 0;
static bios_image_t g_bios;

/* Round 209 (task 366/371): real disc-image mounting for the ACTUAL
 * persistent boot flow, not just throwaway host-native diagnostics.
 * Rounds 170/207 already proved iso_loader.c/iop_cdvd_mount_iso()/
 * iop_cdrom_legacy_mount_iso() correctly parse a real PS2 disc image
 * and serve real sector data - but until this round, main.c never
 * called either mount function at all, so the actual Wii build had
 * no disc to read even if real BIOS/EELOAD code tried to read one.
 * Mirrors the existing BIOS search-path convention: sd:/pcsx2/games/,
 * checked once. A missing disc is NOT a hard error - real PS2
 * hardware also boots fine with no disc inserted (it shows the
 * browser/opening screen instead of a disc-boot fast path); only a
 * real BIOS is mandatory to proceed at all. */
static char g_disc_path[FRONTEND_BROWSER_PATH];
static char g_disc_notice[72];
static frontend_browser g_browser;
static int   g_disc_checked = 0;
static int   g_disc_ok = 0;

/* Keep progress visible until the emulated display contains real RGB pixels. */
static int g_first_picture;
static uint64_t g_log_ticks,g_log_previous;
static gekko2_profile g_runtime_previous,g_front_previous;
static uint64_t g_image_probe_attempts;
static uint64_t g_boot_started,g_boot_log_next;
static frontend_log g_session_log;
static int g_guest_error_reported;
static uint64_t g_game_trace_cursor;
static int g_game_context_reported;
static uint64_t g_fileio_trace_previous_total;

static const char *boot_log_path(void)
{
    return g_session_log.path;
}
static void save_boot_progress(const char *event)
{
    uint64_t log_begin=gettime();
    if(!g_fat_mounted)return;
    ee_state_t *e=ee_core_get_state();iop_state_t *i=iop_core_get_state();gs_state_t *g=gs_get_state();
    FILE *f=frontend_log_open(&g_session_log);if(!f)return;
#ifdef GEKKO2_GX_DUMP
    gs_gx_dump_record("CPU_CHECKPOINT",e->pc,i->pc,(uint32_t)e->instructions_executed,(uint32_t)i->instructions_executed);
    int dump_ok=gs_gx_dump_flush(NULL);
    fprintf(f,"GX_DIAGNOSTIC resident_blend=0 dump_write_ok=%d path=sd:/pcsx2/logs/" GEKKO2_RELEASE "-gx-dump.txt\n",dump_ok);
#endif
    fprintf(f,"EVENT session=%lu kind=%s active_mask=%08lx next_boot_mask=%08lx GX=%d next_GX=%d log_errors=%lu\n",
      (unsigned long)g_session_log.sequence,event,(unsigned long)gekko2_optimization_mask,
      (unsigned long)gekko2_opt_requested(),g_gx_present,g_gx_requested,(unsigned long)g_session_log.errors);
    fprintf(f,"%s ms=%llu BIOS=%s ROMVER=%s size=%u disc=%d EE=%llu PC=%08x IOP=%llu PC=%08x halted=%d/%d STATUS=%08x CAUSE=%08x EPC=%08x PMODE=%llx DISPFB1=%llx DISPFB2=%llx FIRST_IMAGE=%d\n",
        event,(unsigned long long)ticks_to_millisecs(gettime()-g_boot_started),g_bios.name,g_bios.version_string,(unsigned)g_bios.size,g_disc_ok,
        (unsigned long long)e->instructions_executed,e->pc,(unsigned long long)i->instructions_executed,i->pc,e->halted,i->halted,e->cop0[12],e->cop0[13],e->cop0[14],
        (unsigned long long)g->pmode,(unsigned long long)g->dispfb1,(unsigned long long)g->dispfb2,g_first_picture);
    uint32_t sx,sy,sw,sh;int circuit=(g->pmode&1u)?1:2;
    gs_decode_display_region(circuit==1?g->dispfb1:g->dispfb2,
                             circuit==1?g->display1:g->display2,g->smode2,&sx,&sy,&sw,&sh);
    fprintf(f,"VIDEO circuit=%d source=%ux%u origin=%u,%u output=%ux%u SMODE2=%llx DISPLAY=%llx launcher_font=coverage\n",
        circuit,sw,sh,sx,sy,(unsigned)rmode->fbWidth,(unsigned)rmode->xfbHeight,
        (unsigned long long)g->smode2,(unsigned long long)(circuit==1?g->display1:g->display2));
    fprintf(f,"DSI_PROGRESS build=" GEKKO2_BUILD_NAME " status=%d emitted=%lu faults=%lu patched=%lu block_sites=%lu block_prepares=%lu\n",
      dsi_fastmem_status(),(unsigned long)dsi_fastmem_stat(5),(unsigned long)dsi_fastmem_stat(0),(unsigned long)dsi_fastmem_stat(1),(unsigned long)dsi_fastmem_stat(6),(unsigned long)dsi_fastmem_stat(7));
    game_trace_dump(f,&g_game_trace_cursor);
    frontend_log_close(&g_session_log,f);g_log_ticks+=gettime()-log_begin;
}

/* HBC supplies the loader return stub used by standard exit(0).
 * SYS_RETURNTOMENU would go to the Wii System Menu instead. */
static void wii_exit_to_loader(void)
{
    if(g_system_started){
        FILE *f=frontend_log_open(&g_session_log);
        if(f){fprintf(f,"CONTROL_ACTION action=exit-loader held=%04x down=%04x home=%u home_down=%u controller=%s slot=%d connected=%u gx_timeouts=%llu\n",
            g_controller.pad.menu,g_controller.down,g_controller.pad.home,g_controller.home_down,
            controller_kind_name(g_controller.pad.kind),g_controller.active,g_controller.pad.connected,
            (unsigned long long)gs_gx_watchdog_count(0));frontend_log_close(&g_session_log,f);}
        save_boot_progress("EXIT_HBC");
    }
    audio_output_stop();
    controller_host_shutdown();
    gs_gx_shutdown();
    frontend_browser_release(&g_browser);
    iop_cdvd_unmount_iso();iop_cdrom_legacy_unmount_iso();
    if(g_system_started){ee_core_shutdown();iop_core_shutdown();}
    if(g_bios_ok)bios_free(&g_bios);
    fflush(NULL);
    exit(0);
}
static void draw_fps_overlay(void)
{
    char text[64];
    snprintf(text,sizeof(text),"FPS %u.%02u  OUTPUT %u.%02u",
             (unsigned)(g_fps_milli/1000),(unsigned)((g_fps_milli%1000)/10),
             (unsigned)(g_present_milli/1000),(unsigned)((g_present_milli%1000)/10));
    launcher_rect(12,12,304,24,4,9,22);
    launcher_text(20,17,1,text,220,240,255);
}

/* Measure host throughput separately from emulated vertical-sync events.
 * Presenting the same framebuffer again is not a new guest frame. */
static void save_performance(uint64_t ms, uint64_t presents, uint64_t events,
                             uint64_t ee_ins, uint64_t core_ticks, uint64_t blit_ticks)
{
    uint64_t log_begin=gettime();
    if (!g_fat_mounted || !ms) return;
    FILE *f=frontend_log_open(&g_session_log);if(!f)return;
    fprintf(f,"LOG_STATUS session=%lu ms=%llu errors=%lu last_errno=%d path=%s\n",
      (unsigned long)g_session_log.sequence,(unsigned long long)ticks_to_millisecs(gettime()-g_boot_started),
      (unsigned long)g_session_log.errors,g_session_log.last_error,boot_log_path());
    fprintf(f,"PERF interval_ms=%llu presents_mHz=%llu guest_vblank_mHz=%llu EE_per_s=%llu core_ms=%llu blit_ms=%llu JIT=%s\n",
        (unsigned long long)ms,(unsigned long long)(presents*1000000ull/ms),
        (unsigned long long)(events*1000000ull/ms),(unsigned long long)(ee_ins*1000ull/ms),
        (unsigned long long)ticks_to_millisecs(core_ticks),(unsigned long long)ticks_to_millisecs(blit_ticks),
#ifdef PCSX2WII_JIT_DISABLE
        "off"
#else
        "on"
#endif
    );
    if(!g_game_context_reported){
        const char *base=strrchr(g_disc_path,'/');base=base?base+1:g_disc_path;
        char label[96];unsigned n=0;while(base[n]&&n+1<sizeof label){unsigned char c=base[n];label[n]=(c>=32u&&c<127u&&c!='"')?(char)c:'?';n++;}label[n]=0;
        fprintf(f,"GAME_CONTEXT schema=1 image=\"%s\" identity=selected-filename-not-verified-title trace_capacity=64 raw_memory_dump=0\n",label);
        g_game_context_reported=1;
    }
    game_trace_dump(f,&g_game_trace_cursor);
    fprintf(f,"GAME_WAIT ee_pc=%08lx iop_pc=%08lx vu_pc=%08lx signal_stalls=%llu path=%u vblank_events=%llu draw_qwords=%llu interpretation=not-a-gameplay-detector\n",
        (unsigned long)ee_core_get_state()->pc,(unsigned long)iop_core_get_state()->pc,
        (unsigned long)vu1_get_state()->tpc,(unsigned long long)gif_signal_stall_count(),
        gif_get_state()->arb_owner,(unsigned long long)events,(unsigned long long)gif_get_state()->quadwords_seen);
    const ee_state_t *probe_cpu=ee_core_get_state();
    fprintf(f,"GAME_CPU pc=%08lx next=%08lx epc=%08lx status=%08lx cause=%08lx sp=%08lx ra=%08lx v0=%08lx v1=%08lx a0=%08lx a1=%08lx\n",
        (unsigned long)probe_cpu->pc,(unsigned long)probe_cpu->next_pc,(unsigned long)probe_cpu->cop0[14],
        (unsigned long)probe_cpu->cop0[12],(unsigned long)probe_cpu->cop0[13],
        (unsigned long)probe_cpu->gpr[29].ud0,(unsigned long)probe_cpu->gpr[31].ud0,
        (unsigned long)probe_cpu->gpr[2].ud0,(unsigned long)probe_cpu->gpr[3].ud0,
        (unsigned long)probe_cpu->gpr[4].ud0,(unsigned long)probe_cpu->gpr[5].ud0);
    /* BETA12.4: single, read-only Crash E3 NuError capture; no guest mutation.
     * The error routine has already formatted both strings on its stack. */
    if (!g_guest_error_reported) {
        gekko2_guest_error_t error;
        if (gekko2_guest_error_probe(ee_core_get_state(), &error)) {
            fprintf(f,"CRASH_E3_NUERROR pc=%08lx sp=%08lx caller=%08lx provider=%08lx heading=\"%s\" detail=\"%s\"\n",
                (unsigned long)error.pc,(unsigned long)error.sp,(unsigned long)error.caller,
                (unsigned long)ee_core_get_state()->fileio_rpc_version,error.heading,error.detail);
            ee_fileio_trace_t tr; ee_core_fileio_trace(&tr);
            fprintf(f,"CRASH_E3_FILEIO version_calls=%llu opens=%llu open_ok=%llu open_fail=%llu reads=%llu read_bytes=%llu closes=%llu font_opens=%llu font_open_ok=%llu font_open_fail=%llu font_reads=%llu font_bytes=%llu last_open_result=%ld open_offset=%lu last_open=\"%s\" last_font=\"%s\" last_read_fd=%lu last_read_requested=%lu last_read_returned=%lu\n",
                (unsigned long long)tr.version_calls,(unsigned long long)tr.open_calls,
                (unsigned long long)tr.open_success,(unsigned long long)tr.open_fail,
                (unsigned long long)tr.read_calls,(unsigned long long)tr.read_bytes,
                (unsigned long long)tr.close_calls,(unsigned long long)tr.font_open_calls,
                (unsigned long long)tr.font_open_success,(unsigned long long)tr.font_open_fail,
                (unsigned long long)tr.font_read_calls,(unsigned long long)tr.font_read_bytes,
                (long)tr.last_open_result,(unsigned long)tr.last_open_offset,tr.last_open_path,tr.last_font_path,
                (unsigned long)tr.last_read_fd,(unsigned long)tr.last_read_requested,
                (unsigned long)tr.last_read_returned);
            g_guest_error_reported=1;
        }
    }
    /* Print only if FILEIO activity changed; never flood the session log. */
    {
        ee_fileio_trace_t tr;ee_core_fileio_trace(&tr);
        uint64_t total=tr.version_calls+tr.open_calls+tr.read_calls+tr.close_calls;
        if (total && total!=g_fileio_trace_previous_total) {
            fprintf(f,"FILEIO_TRACE version=%08lx versions=%llu opens=%llu ok=%llu fail=%llu reads=%llu bytes=%llu closes=%llu font_opens=%llu font_ok=%llu font_fail=%llu font_reads=%llu font_bytes=%llu last_reply=%ld open_offset=%lu last_open=\"%s\" last_font=\"%s\" last_read=%lu/%lu fd=%lu\n",
                (unsigned long)ee_core_get_state()->fileio_rpc_version,
                (unsigned long long)tr.version_calls,(unsigned long long)tr.open_calls,
                (unsigned long long)tr.open_success,(unsigned long long)tr.open_fail,
                (unsigned long long)tr.read_calls,(unsigned long long)tr.read_bytes,
                (unsigned long long)tr.close_calls,(unsigned long long)tr.font_open_calls,
                (unsigned long long)tr.font_open_success,(unsigned long long)tr.font_open_fail,
                (unsigned long long)tr.font_read_calls,(unsigned long long)tr.font_read_bytes,
                (long)tr.last_open_result,(unsigned long)tr.last_open_offset,tr.last_open_path,tr.last_font_path,
                (unsigned long)tr.last_read_returned,(unsigned long)tr.last_read_requested,(unsigned long)tr.last_read_fd);
            g_fileio_trace_previous_total=total;
        }
    }
    gekko2_profile runtime;gekko2_profile_get(&runtime);
    uint64_t sampled_total=0;for(unsigned n=0;n<GP_COUNT;n++)sampled_total+=runtime.ticks[n]-g_runtime_previous.ticks[n];
    fprintf(f,"TIME_SAMPLE scope=exclusive_random_CPU TB_total=%llu samples=%llu EE=%llu IOP=%llu scheduler=%llu idle=%llu compile=%llu GS_raster=%llu GX_upload=%llu GX_readback=%llu GX_wait=%llu IPU=%llu overflows=%llu\n",
      (unsigned long long)sampled_total,(unsigned long long)(runtime.samples-g_runtime_previous.samples),
      (unsigned long long)(runtime.ticks[GP_EE]-g_runtime_previous.ticks[GP_EE]),
      (unsigned long long)(runtime.ticks[GP_IOP]-g_runtime_previous.ticks[GP_IOP]),
      (unsigned long long)(runtime.ticks[GP_SCHEDULER]-g_runtime_previous.ticks[GP_SCHEDULER]),
      (unsigned long long)(runtime.ticks[GP_IDLE]-g_runtime_previous.ticks[GP_IDLE]),
      (unsigned long long)(runtime.ticks[GP_COMPILE]-g_runtime_previous.ticks[GP_COMPILE]),
      (unsigned long long)(runtime.ticks[GP_GS_RASTER]-g_runtime_previous.ticks[GP_GS_RASTER]),
      (unsigned long long)(runtime.ticks[GP_GX_UPLOAD]-g_runtime_previous.ticks[GP_GX_UPLOAD]),
      (unsigned long long)(runtime.ticks[GP_GX_READBACK]-g_runtime_previous.ticks[GP_GX_READBACK]),
      (unsigned long long)(runtime.ticks[GP_GX_WAIT]-g_runtime_previous.ticks[GP_GX_WAIT]),
      (unsigned long long)(runtime.ticks[GP_IPU]-g_runtime_previous.ticks[GP_IPU]),
      (unsigned long long)runtime.overflows);
    g_runtime_previous=runtime;
    gekko2_profile front;gekko2_profile_get_host(&front);
    uint64_t front_total=0;for(unsigned n=0;n<GP_COUNT;n++)front_total+=front.ticks[n]-g_front_previous.ticks[n];
    fprintf(f,"TIME_PRESENT scope=exact_host TB_total=%llu calls=%llu frontend=%llu GX_upload=%llu GX_readback=%llu GX_wait=%llu overflows=%llu\n",
      (unsigned long long)front_total,(unsigned long long)(front.samples-g_front_previous.samples),
      (unsigned long long)(front.ticks[GP_PRESENT]-g_front_previous.ticks[GP_PRESENT]),
      (unsigned long long)(front.ticks[GP_GX_UPLOAD]-g_front_previous.ticks[GP_GX_UPLOAD]),
      (unsigned long long)(front.ticks[GP_GX_READBACK]-g_front_previous.ticks[GP_GX_READBACK]),
      (unsigned long long)(front.ticks[GP_GX_WAIT]-g_front_previous.ticks[GP_GX_WAIT]),
      (unsigned long long)front.overflows);
    g_front_previous=front;
    fprintf(f,"HOST_TIME core_tb=%llu presentation_tb=%llu log_tb=%llu log_ms=%llu\n",
      (unsigned long long)core_ticks,(unsigned long long)blit_ticks,
      (unsigned long long)(g_log_ticks-g_log_previous),(unsigned long long)ticks_to_millisecs(g_log_ticks-g_log_previous));
    g_log_previous=g_log_ticks;
    jit_cache_profile caches[2];ee_jit_get_cache_profile(&caches[0]);iop_jit_get_cache_profile(&caches[1]);
    fprintf(f,"JIT_BUDGET_RANGES interval_hits=%llu way_scans=%llu\n",(unsigned long long)ee_jit_get_budget_cache_stat(6),(unsigned long long)ee_jit_get_budget_cache_stat(7));
    fprintf(f,"GX_SPARSE_SEED uploaded_bytes=%llu tile_runs=%llu reused_tiles=%llu\n",
        (unsigned long long)gs_gx_surface_count(5),(unsigned long long)gs_gx_surface_count(9),(unsigned long long)gs_gx_surface_count(10));
    fprintf(f,"GX_BOUNDED_READBACK avoided_bytes=%llu seed_only_releases=%llu resident_fence=%u\n",
        (unsigned long long)gs_gx_surface_count(7),(unsigned long long)gs_gx_surface_count(8),
#ifdef GEKKO2_GX_RESIDENT_FENCE
        1u
#else
        0u
#endif
    );
    fprintf(f,"GX_NATIVE_ADDITIVE draws=%llu source_pixels=%llu avoided_destination_copy_bytes=%llu\n",(unsigned long long)gs_gx_additive_stat(0),(unsigned long long)gs_gx_additive_stat(1),(unsigned long long)gs_gx_additive_stat(2));
    for(unsigned n=0;n<2;n++)fprintf(f,"BLOCK_CACHE cpu=%s ways=%u lookups=%llu hits=%llu misses=%llu collisions=%llu stale=%llu attempts=%llu installed=%llu failures=%llu compile_sample_tb=%llu compile_samples=%llu sample_stride=64\n",
      n?"IOP":"EE",gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)?4u:1u,
      (unsigned long long)caches[n].lookups,(unsigned long long)caches[n].hits,(unsigned long long)caches[n].misses,
      (unsigned long long)caches[n].collisions,(unsigned long long)caches[n].stale,(unsigned long long)caches[n].attempts,
      (unsigned long long)caches[n].installed,(unsigned long long)caches[n].failures,
      (unsigned long long)caches[n].compile_tb,(unsigned long long)caches[n].compile_samples);
    fprintf(f,"EE_CACHE_PRESSURE sweeps=%llu released=%llu deferred=%llu scanned=%llu blocked=%llu reclaimed_bytes=%llu\n",
      (unsigned long long)ee_jit_get_pressure_stat(0),(unsigned long long)ee_jit_get_pressure_stat(1),
      (unsigned long long)ee_jit_get_pressure_stat(2),(unsigned long long)ee_jit_get_pressure_stat(3),
      (unsigned long long)ee_jit_get_pressure_stat(4),(unsigned long long)ee_jit_get_pressure_stat(5));
    fprintf(f,"EE_CACHE_LAYOUT entries=%u sets=%u ways=%u\n",ee_jit_get_cache_entries(),ee_jit_get_cache_entries()/(gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)?4u:1u),gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)?4u:1u);
    fprintf(f,"EE_CACHE_BUDGET fit_hits=%llu budget_misses=%llu variant_installs=%llu deferred_variants=%llu refusal_hits=%llu lookup_hits=%llu\n",
      (unsigned long long)ee_jit_get_budget_cache_stat(0),(unsigned long long)ee_jit_get_budget_cache_stat(1),
      (unsigned long long)ee_jit_get_budget_cache_stat(2),(unsigned long long)ee_jit_get_budget_cache_stat(3),(unsigned long long)ee_jit_get_budget_cache_stat(4),(unsigned long long)ee_jit_get_budget_cache_stat(5));
    fprintf(f,"IOP_ROUTES native_instructions=%llu interpreter_instructions=%llu recovery_ticks=%llu RAM_helper_fast_reads=%llu RAM_helper_fast_writes=%llu\n",
      (unsigned long long)iop_core_route_stat(0),(unsigned long long)iop_core_route_stat(1),
      (unsigned long long)iop_core_route_stat(2),(unsigned long long)iop_core_route_stat(3),(unsigned long long)iop_core_route_stat(4));
    ipu_profile_t ipu;ipu_get_profile(&ipu);
    fprintf(f,"IPU_STATUS scope=fifo_idec_bdec_idct_vdec_csc_pack unimplemented=%llu input_qwc=%llu accepted_qwc=%llu discarded_qwc=%llu fifo=%lu last=%08lx output_available=%lu busy=%lu output_qwc=%llu csc=%llu completed=%llu\n",
      (unsigned long long)ipu.unimplemented_commands,(unsigned long long)ipu.input_qwc,
      (unsigned long long)ipu.accepted_qwc,(unsigned long long)ipu.discarded_qwc,
      (unsigned long)ipu.fifo_count,(unsigned long)ipu.last_command,
      (unsigned long)ipu.output_available,(unsigned long)ipu.busy,
      (unsigned long long)ipu.output_qwc,(unsigned long long)ipu.csc_macroblocks,(unsigned long long)ipu.completed_commands);
    fprintf(f,"IPU_COMMANDS BCLR=%llu IDEC=%llu BDEC=%llu VDEC=%llu FDEC=%llu SETIQ=%llu SETVQ=%llu CSC=%llu PACK=%llu SETTH=%llu unknown=%llu\n",
      (unsigned long long)ipu.commands[0],(unsigned long long)ipu.commands[1],
      (unsigned long long)ipu.commands[2],(unsigned long long)ipu.commands[3],
      (unsigned long long)ipu.commands[4],(unsigned long long)ipu.commands[5],
      (unsigned long long)ipu.commands[6],(unsigned long long)ipu.commands[7],
      (unsigned long long)ipu.commands[8],(unsigned long long)ipu.commands[9],
      (unsigned long long)(ipu.commands[10]+ipu.commands[11]+ipu.commands[12]+ipu.commands[13]+ipu.commands[14]+ipu.commands[15]));
    fprintf(f,"HLE_ROUTES ram_copy_bytes=%llu ram_set_bytes=%llu unknown_legacy=%llu unknown_guest=%llu strict=%u\n",
      (unsigned long long)iop_hle_bios_route_stat(0),(unsigned long long)iop_hle_bios_route_stat(1),
      (unsigned long long)iop_hle_bios_route_stat(2),(unsigned long long)iop_hle_bios_route_stat(3),gekko2_opt_enabled(GEKKO2_OPT_STRICT_HLE));
    fprintf(f,"ARM_EE_BOOT verified=%llu selected=%llu PPC_ticks=%llu ARM_ticks=%llu scope=cold-boot-RAM-clear\n",(unsigned long long)ee_core_arm_boot_stat(0),(unsigned long long)ee_core_arm_boot_stat(1),(unsigned long long)ee_core_arm_boot_stat(2),(unsigned long long)ee_core_arm_boot_stat(3));
    fprintf(f,"ARM_WORKER probes=%llu submitted=%llu completed=%llu errors=%llu stale=%llu enabled=%u available=%d\n",
      (unsigned long long)arm_worker_stat(0),(unsigned long long)arm_worker_stat(1),
      (unsigned long long)arm_worker_stat(2),(unsigned long long)arm_worker_stat(3),(unsigned long long)arm_worker_stat(4),gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER),arm_worker_available());
    fprintf(f,"ARM_RAM probes=%llu chunks=%llu completed=%llu bytes=%llu errors=%llu declined=%llu enabled=%d available=%d synchronous=1 verified=1\n",
      (unsigned long long)arm_ram_stat(0),(unsigned long long)arm_ram_stat(1),
      (unsigned long long)arm_ram_stat(2),(unsigned long long)arm_ram_stat(3),
      (unsigned long long)arm_ram_stat(4),(unsigned long long)arm_ram_stat(5),
      gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM),arm_ram_available());
    fprintf(f,"ARM_WORKER_CONNECT open=%d caps=%d attempts=%d\n",arm_worker_diagnostic(0),arm_worker_diagnostic(1),arm_worker_diagnostic(2));
    fprintf(f,"ARM_WORKER_CAPS decline=%d raw=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
      arm_worker_diagnostic(3),arm_worker_diagnostic(4),arm_worker_diagnostic(5),arm_worker_diagnostic(6),arm_worker_diagnostic(7),
      arm_worker_diagnostic(8),arm_worker_diagnostic(9),arm_worker_diagnostic(10),arm_worker_diagnostic(11));
    fprintf(f,"ARM_LOADER status=%d path=sd:/pcsx2/arm/Gekko2-ARM-Worker.elf\n",arm_loader_status());
    fprintf(f,"ARM_LOAD_DETAIL bytes=%lu base=%08lx capacity=%08lx entry=%08lx crc32=%08lx validation=%lu\n",(unsigned long)arm_loader_stat(0),(unsigned long)arm_loader_stat(1),(unsigned long)arm_loader_stat(2),(unsigned long)arm_loader_stat(3),(unsigned long)arm_loader_stat(4),(unsigned long)arm_loader_stat(5));
    fprintf(f,"ARM_TRANSFER operation=%lu result=%ld address=%08lx length=%lu\n",(unsigned long)arm_loader_stat(6),(long)(int32_t)arm_loader_stat(7),(unsigned long)arm_loader_stat(8),(unsigned long)arm_loader_stat(9));
    fprintf(f,"PAD_INPUT host_held=%04x guest_pressed=%04x samples=%lu serial_commands=%lu controller=%s slot=%d connected=%u drc_available=%d usb_status=%d axes=%u,%u,%u,%u\n",g_input_held,iop_sio2_pad_get_buttons(),(unsigned long)iop_sio2_pad_sample_count(),(unsigned long)iop_sio2_get_pad_command_count(),controller_kind_name(g_controller.pad.kind),g_controller.active,g_controller.pad.connected,controller_host_drc_available(),controller_host_usb_status(),g_controller.pad.rx,g_controller.pad.ry,g_controller.pad.lx,g_controller.pad.ly);
    {const rspu2_stream_state_t *stream=rspu2_stream_get_state();
     audio_output_info ai;audio_output_get_info(&ai);
     fprintf(f,"SPU2_AUDIO enabled=%d rate=48000 frames=%lu kon=%llu dma=%lu irq=%lu adma=%lu unsupported=%lu queued=%lu submitted=%lu played=%lu dropped=%lu underrun=%lu nonzero=%lu running=%lu\n",
       gekko2_opt_enabled(GEKKO2_OPT_SOUND),(unsigned long)iop_spu2_audio_stat(0),(unsigned long long)spu2_mixer_get_total_kon_count(),
       (unsigned long)iop_dma_get_spu2_transfer_count(),(unsigned long)iop_spu2_audio_stat(1),(unsigned long)iop_spu2_audio_stat(2),(unsigned long)iop_spu2_audio_stat(3),
       (unsigned long)ai.queued,(unsigned long)ai.submitted,(unsigned long)ai.played,(unsigned long)ai.dropped,(unsigned long)ai.underrun,(unsigned long)ai.nonzero,(unsigned long)ai.running);
     fprintf(f,"RSPU2_STREAM init=%lu cmd=%04lx status=%02lx reads=%lu sectors=%lu remaining=%lu failures=%lu audio_sectors=%lu\n",
       (unsigned long)stream->initialized,(unsigned long)stream->last_command,(unsigned long)rspu2_stream_status(),
       (unsigned long)stream->reads,(unsigned long)stream->transferred_sectors,(unsigned long)stream->remaining_sectors,(unsigned long)stream->failures,(unsigned long)stream->audio_sectors_read);}
    fprintf(f,"ARM_IOS status=%d original=%u active=%u\n",wii_arm_ios_status(),wii_arm_ios_original(),wii_arm_ios_active());
    fprintf(f,"ARM_STARTUP requested_ios=%u active_ios=%u restart_required=%d\n",
      gekko2_opt_enabled(GEKKO2_OPT_ARM_IOS222)?222u:0u,wii_arm_ios_active(),
      gekko2_opt_enabled(GEKKO2_OPT_ARM_IOS222)&&wii_arm_ios_active()!=222u);

    system_profile_t profile;system_profile_get(&profile);
    uint64_t ee_sample=profile.ee_ticks-g_profile_previous.ee_ticks;
    uint64_t iop_sample=profile.iop_ticks-g_profile_previous.iop_ticks;
    uint64_t total_sample=ee_sample+iop_sample;
    fprintf(f,"CPU_SAMPLE pairs=%llu EE_tb=%llu IOP_tb=%llu EE_percent=%llu IOP_percent=%llu\n",
            (unsigned long long)(profile.samples-g_profile_previous.samples),
            (unsigned long long)ee_sample,(unsigned long long)iop_sample,
            (unsigned long long)(total_sample?ee_sample*100/total_sample:0),
            (unsigned long long)(total_sample?iop_sample*100/total_sample:0));
    g_profile_previous=profile;
    ee_timers_state_t timer_snapshot;ee_timers_snapshot(&timer_snapshot);
    ee_timers_state_t *timers=&timer_snapshot;
    fprintf(f,"EVENT_TIMER deferred=%llu boundaries=%llu\n",(unsigned long long)ee_timers_get_batched_ticks(),(unsigned long long)ee_timers_get_boundary_ticks());
    fprintf(f,"EE_TIMERS mode0=%lx mode1=%lx mode2=%lx mode3=%lx\n",
            (unsigned long)timers->t[0].mode,(unsigned long)timers->t[1].mode,
            (unsigned long)timers->t[2].mode,(unsigned long)timers->t[3].mode);
    fprintf(f,"HOST budget=%lu controller_connected=%d fps_overlay=%d gx_output=%d EE_PC=%08lx IOP_PC=%08lx\n",(unsigned long)g_slice_budget,g_controller.pad.connected,g_fps_default,g_gx_present,(unsigned long)ee_core_get_state()->pc,(unsigned long)iop_core_get_state()->pc);
    ee_state_t *ee_runtime=ee_core_get_state();iop_state_t *iop_runtime=iop_core_get_state();
    fprintf(f,"CORE_WAIT EE_idle=%u EE_halted=%u EE_Count=%08lx EE_retired=%llu IOP_idle=%u IOP_halted=%u IOP_ticks=%llu\n",
        (unsigned)ee_runtime->idle,(unsigned)ee_runtime->halted,(unsigned long)ee_runtime->cop0[9],
        (unsigned long long)ee_runtime->instructions_executed,(unsigned)iop_runtime->idle,
        (unsigned)iop_runtime->halted,(unsigned long long)iop_runtime->sched_ticks);
    const gs_state_t *scanout=gs_get_state();const gif_state_t *draw_target=gif_get_state();
    fprintf(f,"GS_PRESENT_LAYOUT smode1=%llx cmod=%u display1_xy=%u,%u mag1=%u,%u display2_xy=%u,%u mag2=%u,%u host=%ux%u policy=full-output wii_widescreen=%d\n",
        (unsigned long long)scanout->smode1,(unsigned)((scanout->smode1>>13)&3u),
        (unsigned)(scanout->display1&4095u),(unsigned)((scanout->display1>>12)&2047u),
        (unsigned)(((scanout->display1>>23)&15u)+1u),(unsigned)(((scanout->display1>>27)&3u)+1u),
        (unsigned)(scanout->display2&4095u),(unsigned)((scanout->display2>>12)&2047u),
        (unsigned)(((scanout->display2>>23)&15u)+1u),(unsigned)(((scanout->display2>>27)&3u)+1u),
        (unsigned)rmode->fbWidth,(unsigned)rmode->xfbHeight,g_present_widescreen);
    fprintf(f,"GS_DISPLAY_REGS schema=1 PMODE=%llx SMODE2=%llx DISPFB1=%llx DISPLAY1=%llx DISPFB2=%llx DISPLAY2=%llx BGCOLOR=%llx\n",
        (unsigned long long)scanout->pmode,(unsigned long long)scanout->smode2,
        (unsigned long long)scanout->dispfb1,(unsigned long long)scanout->display1,
        (unsigned long long)scanout->dispfb2,(unsigned long long)scanout->display2,
        (unsigned long long)scanout->bgcolor);
    uint64_t scanout_fb=(scanout->pmode&1u)?scanout->dispfb1:scanout->dispfb2;
    uint64_t scanout_display=(scanout->pmode&1u)?scanout->display1:scanout->display2;
    uint32_t scan_bp,scan_bw,scan_x,scan_y,scan_w,scan_h;
    gs_decode_dispfb(scanout_fb,&scan_bp,&scan_bw);
    gs_decode_display_region(scanout_fb,scanout_display,scanout->smode2,&scan_x,&scan_y,&scan_w,&scan_h);
    uint32_t color_probe[6]={0};
    gs_scanout_probe_rgb_psm(scan_bp,scan_bw,scan_x,scan_y,scan_w,scan_h,
                              (uint32_t)((scanout_fb>>15)&31u),color_probe);
    fprintf(f,"GS_SOURCE_RGB samples=%lu avg_r=%lu avg_g=%lu avg_b=%lu max_luma=%lu nonblack=%lu scope=CPU-visible-scanout-grid\n",
        (unsigned long)color_probe[0],(unsigned long)color_probe[1],(unsigned long)color_probe[2],(unsigned long)color_probe[3],(unsigned long)color_probe[4],(unsigned long)color_probe[5]);
    /* Compare the guest-selected front buffer with the active draw target.
     * Never substitute the other buffer: a legitimate black fade remains black. */
    if (g_first_picture && draw_target->fbp != scan_bp && draw_target->fbw) {
        uint32_t back_probe[6]={0};
        gs_scanout_probe_rgb_psm(draw_target->fbp,draw_target->fbw,0,0,
                             scan_w<=640u?scan_w:640u,scan_h<=448u?scan_h:448u,
                             draw_target->frame_psm,back_probe);
        fprintf(f,"GS_BUFFER_COMPARE display_bp=%lu display_nonblack=%lu draw_bp=%lu draw_nonblack=%lu disp_size=%lux%lu\n",
          (unsigned long)scan_bp,(unsigned long)color_probe[5],
          (unsigned long)draw_target->fbp,(unsigned long)back_probe[5],
          (unsigned long)scan_w,(unsigned long)scan_h);
    }
    uint32_t display_hash=0,draw_hash=0;
    int display_valid=gs_scanout_grid_hash(scan_bp,scan_bw,scan_x,scan_y,scan_w,scan_h,
                                           (uint32_t)((scanout_fb>>15)&31u),&display_hash);
    int draw_valid=gs_scanout_grid_hash(draw_target->fbp,draw_target->fbw,0,0,
                                       scan_w<=640u?scan_w:640u,scan_h<=448u?scan_h:448u,
                                       draw_target->frame_psm,&draw_hash);
    fprintf(f,"GS_GRID_HASH schema=1 display_valid=%d display_rgb=%08lx draw_valid=%d draw_rgb=%08lx draw_psm=%lu scope=8x8-sampled-RGB-not-full-frame\n",
        display_valid,(unsigned long)display_hash,draw_valid,(unsigned long)draw_hash,
        (unsigned long)draw_target->frame_psm);
    fprintf(f,"GS_PSM_CONVERT converted_pixels=%llu rejected_formats=%llu extended=%d\n",
        (unsigned long long)gs_mem_local_converted_pixels(),
        (unsigned long long)gs_mem_local_rejected_formats(),
        gs_mem_get_local_extended_conversion());
    fprintf(f,"GS_LOCAL_HOST fifo_calls=%llu bytes=%llu failed_reads=%llu busdir=%llu vif1_qwc=%lu\n",
        (unsigned long long)gif_local_host_stat(0),
        (unsigned long long)gif_local_host_stat(1),
        (unsigned long long)gif_local_host_stat(2),
        (unsigned long long)gs_get_state()->busdir,
        (unsigned long)dma_get_state()->chan[DMA_CHANNEL_VIF1].qwc);
    fprintf(f,"GS_ALIAS_SYNC queries=%llu resolves=%llu disjoint=%llu failures=%llu cross_layout_snapshots=%llu snapshot_pixels=%llu size_declines=%llu\n",
        (unsigned long long)gs_mem_alias_stat(0),(unsigned long long)gs_mem_alias_stat(1),
        (unsigned long long)gs_mem_alias_stat(2),(unsigned long long)gs_mem_alias_stat(3),
        (unsigned long long)gs_mem_local_alias_stat(0),(unsigned long long)gs_mem_local_alias_stat(1),
        (unsigned long long)gs_mem_local_alias_stat(2));
    fprintf(f,"GS_LOCAL_COPY transfers=%llu declines=%llu pixels=%llu\n",
        (unsigned long long)gif_local_copy_stat(0),
        (unsigned long long)gif_local_copy_stat(1),
        (unsigned long long)gif_local_copy_stat(2));
    fprintf(f,"GS_SCANOUT_FORMAT psm=%lu supported=%u path=%s\n",
       (unsigned long)((scanout_fb>>15)&31u),
       (unsigned)((((scanout_fb>>15)&31u)==0u)||(((scanout_fb>>15)&31u)==1u)||
                  (((scanout_fb>>15)&31u)==2u)||(((scanout_fb>>15)&31u)==10u)),
       frontend_allow_gx_output(1,1,(unsigned)((scanout_fb>>15)&31u))?"GX-or-software":"unsupported");
    fprintf(f,"GS_SCANOUT PMODE=%llx DISPFB=%llx DISPLAY=%llx bp=%lu bw=%lu origin=%lu,%lu size=%lux%lu image_probes=%llu draw_bp=%lu draw_bw=%lu draw_psm=%lu\n",
        (unsigned long long)scanout->pmode,(unsigned long long)scanout_fb,(unsigned long long)scanout_display,
        (unsigned long)scan_bp,(unsigned long)scan_bw,(unsigned long)scan_x,(unsigned long)scan_y,
        (unsigned long)scan_w,(unsigned long)scan_h,(unsigned long long)g_image_probe_attempts,
        (unsigned long)draw_target->fbp,(unsigned long)draw_target->fbw,(unsigned long)draw_target->frame_psm);
    ee_intc_poll_trace_t pt;ee_intc_poll_trace_snapshot(&pt);
    fprintf(f,"EE_VBLANK_POLL irq_raises=%lu irq_stat=%08lx irq_mask=%08lx w1c=%llu reads=%llu observed_set=%llu observed_clear=%llu last_pc=%08lx last_value=%08lx fast_skips=%llu fast_slots=%llu last_fast_site=%08llx\n",
      (unsigned long)ee_intc_get_raise_count(2),(unsigned long)ee_intc_get_state()->stat,
      (unsigned long)ee_intc_get_state()->mask,(unsigned long long)pt.vblank_w1c,
      (unsigned long long)pt.stat_reads,(unsigned long long)pt.vblank_seen_reads,
      (unsigned long long)pt.vblank_clear_reads,(unsigned long)pt.last_guest_pc,
      (unsigned long)pt.last_guest_value,
      (unsigned long long)ee_core_vblank_fast_wait_stat(0),
      (unsigned long long)ee_core_vblank_fast_wait_stat(1),
      (unsigned long long)ee_core_vblank_fast_wait_stat(2));
    fprintf(f,"GX requested=%d ready=%d primitives_requested=%d primitives_active=%d first=%d hud=%d attempts=%llu fallbacks=%llu pending=%lu sync_errors=%lu log=%s\n",
       g_gx_present,gs_gx_ready(),g_gx_present,
       frontend_allow_gx_primitives(g_gx_present,g_first_picture)&&gs_gx_ready(),
       g_first_picture,g_hud_active,(unsigned long long)g_gx_attempts,(unsigned long long)g_gx_fallbacks,
       (unsigned long)gs_mem_gpu_pending(),(unsigned long)gs_mem_sync_failures(),boot_log_path());
    fprintf(f,"GX_WORK draws=%llu quads=%llu readback_bytes=%llu resolve_waits=%llu attempts=%llu\n",
            (unsigned long long)gs_gx_work_count(0),(unsigned long long)gs_gx_work_count(1),
            (unsigned long long)gs_gx_work_count(2),(unsigned long long)gs_gx_work_count(3),
            (unsigned long long)gs_gx_work_count(4));
    fprintf(f,"GS_PRESENT schema=1 xfb_updates=%llu guest_copies=%llu hud_updates=%llu gx_copies=%llu sw_attempts=%llu sw_copies=%llu sw_sync_failures=%llu sw_invalid=%llu scope=submitted-XFB-not-hardware-display-proof\n",
        (unsigned long long)g_xfb_updates,(unsigned long long)g_xfb_guest_copies,
        (unsigned long long)g_xfb_hud_updates,(unsigned long long)(g_gx_attempts-g_gx_fallbacks),
        (unsigned long long)g_sw_attempts,(unsigned long long)g_sw_copies,
        (unsigned long long)g_sw_sync_failures,(unsigned long long)g_sw_invalid);
        fprintf(f,"GX_PIPELINE depth_hw=%llu depth_hybrid=%llu blend_hw=%llu blend_hybrid=%llu z32_split=%llu pabe_split=%llu\n",
        (unsigned long long)gs_gx_pipeline_count(0),(unsigned long long)gs_gx_pipeline_count(1),
        (unsigned long long)gs_gx_pipeline_count(2),(unsigned long long)gs_gx_pipeline_count(3),
        (unsigned long long)gs_gx_pipeline_count(4),(unsigned long long)gs_gx_pipeline_count(5));
    fprintf(f,"GX_SURFACE opens=%llu draws=%llu resolves=%llu snapshots=%llu presents=%llu seed_bytes=%llu gouraud_resident=%llu\n",
        (unsigned long long)gs_gx_surface_count(0),(unsigned long long)gs_gx_surface_count(1),
        (unsigned long long)gs_gx_surface_count(2),(unsigned long long)gs_gx_surface_count(3),
        (unsigned long long)gs_gx_surface_count(4),(unsigned long long)gs_gx_surface_count(5),
        (unsigned long long)gs_gx_surface_count(6));
    fprintf(f,"GS_HASH_CACHE hits=%llu avoided_bytes=%llu\n",
        (unsigned long long)gs_mem_hash_cache_hits(),(unsigned long long)gs_mem_hash_cache_bytes_avoided());
    fprintf(f,"GX_SOURCE_CACHE hits=%llu misses=%llu decoded_bytes=%llu avoided_cpu_flush_bytes=%llu\n",
        (unsigned long long)gs_gx_source_cache_count(0),(unsigned long long)gs_gx_source_cache_count(1),
        (unsigned long long)gs_gx_source_cache_count(2),(unsigned long long)gs_gx_source_cache_count(3));
    fprintf(f,"GX_TEXCACHE_INVALIDATE_SKIPS=%llu scope=unchanged-source-buffer\n",
       (unsigned long long)gs_gx_source_cache_count(4));
    fprintf(f,"GX_RESIDENT_PIPELINE attempts=%llu accepted=%llu depth_cpu_tests=%llu depth_failed=%llu depth_cpu_writes=%llu blend_gx=%llu avoided_compact_bytes=%llu depth_alias_rejects=%llu blend_rejects=%llu layout_rejects=%llu blend_snapshot_gpu_bytes=%llu\n",
        (unsigned long long)gs_gx_resident_pipeline_count(0),(unsigned long long)gs_gx_resident_pipeline_count(1),
        (unsigned long long)gs_gx_resident_pipeline_count(2),(unsigned long long)gs_gx_resident_pipeline_count(3),
        (unsigned long long)gs_gx_resident_pipeline_count(4),(unsigned long long)gs_gx_resident_pipeline_count(5),
        (unsigned long long)gs_gx_resident_pipeline_count(6),(unsigned long long)gs_gx_resident_pipeline_count(7),
        (unsigned long long)gs_gx_resident_pipeline_count(8),(unsigned long long)gs_gx_resident_pipeline_count(9),(unsigned long long)gs_gx_resident_pipeline_count(10));
    fprintf(f,"GS_BLEND a=%u b=%u c=%u d=%u fix=%u colclamp=%u pabe=%u\n",
        gif_get_state()->alpha_a,gif_get_state()->alpha_b,gif_get_state()->alpha_c,gif_get_state()->alpha_d,gif_get_state()->alpha_fix,gif_get_state()->colclamp,gif_get_state()->pabe);
    fprintf(f,"GX_TEXTURE candidates=%llu draws=%llu upload_bytes=%llu layout_rejects=%llu\n",
        (unsigned long long)gs_gx_texture_count(0),(unsigned long long)gs_gx_texture_count(1),
        (unsigned long long)gs_gx_texture_count(2),(unsigned long long)gs_gx_texture_count(3));
    fprintf(f,"JIT_CACHE compiled=%u attempts=%llu rejected_hits=%llu executed=%llu L0hit=%llu L0miss=%llu\n",
            (unsigned)ee_jit_get_cache_size(),
            (unsigned long long)ee_jit_get_compile_attempt_count(),
            (unsigned long long)ee_jit_get_rejected_hit_count(),
            (unsigned long long)ee_jit_get_executed_count(),
            (unsigned long long)ee_jit_get_pc_l0_hit_count(),
            (unsigned long long)ee_jit_get_pc_l0_miss_count());
    fprintf(f,"EE_CHAIN_RESIDENCY enabled=%llu runs=%llu retired=%llu successors=%llu abi=experimental-clean-write-through\n",
        (unsigned long long)ee_jit_get_chain_residency_stat(0),
        (unsigned long long)ee_jit_get_chain_residency_stat(1),
        (unsigned long long)ee_jit_get_chain_residency_stat(2),
        (unsigned long long)ee_jit_get_chain_residency_stat(3));
    fprintf(f,"EXPERIMENTAL_OPTIONS chain_compiled=%u chain_requested=%d chain_active=%d psm_requested=%d psm_active=%d apply=cold-boot\n",
        (gekko2_experimental_available()>>GEKKO2_EXP_CHAIN)&1u,
        gekko2_experimental_requested(GEKKO2_EXP_CHAIN),gekko2_experimental_enabled(GEKKO2_EXP_CHAIN),
        gekko2_experimental_requested(GEKKO2_EXP_PSM),gs_mem_get_local_extended_conversion());
    fprintf(f,"GX_FENCE completion=ordered-PE-token waits=%llu callbacks=%llu polled=%llu timeouts=%llu aborts=%llu last_reason=%llu\n",
        (unsigned long long)gs_gx_watchdog_count(4),(unsigned long long)gs_gx_watchdog_count(5),
        (unsigned long long)gs_gx_watchdog_count(6),(unsigned long long)gs_gx_watchdog_count(0),
        (unsigned long long)gs_gx_watchdog_count(7),(unsigned long long)gs_gx_watchdog_count(1));
    /* R1358: capture the drain/producer pair in the existing log snapshot.
     * No new per-instruction polling or logging. */
    const dma_state_t *d=dma_get_state();
    const dma_channel_t *gc=&d->chan[DMA_CHANNEL_GIF], *sp=&d->chan[DMA_CHANNEL_FROMSPR];
    fprintf(f,"TEKKEN_DMA ctrl=%08lx rbor=%08lx rbsr=%08lx gif_chcr=%08lx gif_tadr=%08lx gif_madr=%08lx gif_qwc=%lu gif_error=%lu spr_madr=%08lx spr_error=%lu\n",
        (unsigned long)d->d_ctrl,(unsigned long)d->d_rbor,(unsigned long)d->d_rbsr,
        (unsigned long)gc->chcr,(unsigned long)gc->tadr,(unsigned long)gc->madr,
        (unsigned long)gc->qwc,(unsigned long)gc->last_error,(unsigned long)sp->madr,(unsigned long)sp->last_error);
    const gif_state_t *gif=gif_get_state();
    const vu1_state_t *vu1=vu1_get_state();
    fprintf(f,"GIF_SIGNAL_STALL calls=%llu scope=unaccepted-producer-qword\n",
        (unsigned long long)gif_signal_stall_count());
    const gif_register_carry_t *blocked=&gif->register_carry_path[GIF_PATH_1];
    fprintf(f,"GIF_BLOCKED_STATE csr=%08lx imr=%08lx intc_stat=%08lx intc_mask=%08lx tag=%08lx,%08lx,%08lx,%08lx mode=%lu nreg=%lu index=%lu remaining=%lu eop=%lu regs=%08lx,%08lx vif1_stat=%08lx vif1_held_words=%lu uv_fractional=1\n",
        (unsigned long)gs_get_state()->csr,(unsigned long)gs_get_state()->imr,
        (unsigned long)ee_intc_get_state()->stat,(unsigned long)ee_intc_get_state()->mask,
        (unsigned long)gif->gif_tag0,(unsigned long)gif->gif_tag1,
        (unsigned long)gif->gif_tag2,(unsigned long)gif->gif_tag3,
        (unsigned long)blocked->mode,(unsigned long)blocked->nreg,
        (unsigned long)blocked->index,(unsigned long)blocked->remaining,(unsigned long)blocked->eop,
        (unsigned long)blocked->regs_lo,(unsigned long)blocked->regs_hi,
        (unsigned long)vif1_get_state()->stat,(unsigned long)vif1_get_state()->stall_words);
    fprintf(f,"GS_TRANSPORT schema=1 qwords=%llu path1_calls=%llu owner=%u pending=%u fifo=%u image_remaining=%lu,%lu,%lu vu1_running=%u vu1_tpc=%08lx vu1_pairs=%llu vu1_unknown=%llu kick_pending=%u kick_addr=%08lx kick_qwords=%lu kick_busy=%llu kick_deferred=%u kick_deferred_stalls=%llu\n",
        (unsigned long long)gif->quadwords_seen,(unsigned long long)gif->gif_path1_transfers,
        (unsigned)gif->arb_owner,(unsigned)gif->arb_pending,(unsigned)gif->fifo_count,
        (unsigned long)gif->image_carry_path[0],(unsigned long)gif->image_carry_path[1],
        (unsigned long)gif->image_carry_path[2],(unsigned)vu1->running,(unsigned long)vu1->tpc,
        (unsigned long long)vu1->instructions_executed,(unsigned long long)vu1->unimplemented_opcodes_seen,
        (unsigned)vu1->xgkick_pending,(unsigned long)vu1->xgkick_addr,
        (unsigned long)vu1->xgkick_packet_qwords,(unsigned long long)vu1->xgkick_busy_stalls,
        (unsigned)vu1->xgkick_deferred,(unsigned long long)vu1->xgkick_deferred_stalls);
    fprintf(f,"GS_STATE frame_psm=%lu prim=%lx prmode=%lx ac=%lu zcfg=%lu ate=%lu fbmask=%lx triangles=%llu sprites=%llu\n",
        (unsigned long)gif->frame_psm,(unsigned long)gif->prim,(unsigned long)gif->prmode,
        (unsigned long)gif->prmodecont_ac,(unsigned long)gif->zbuf_configured,(unsigned long)gif->ate,
        (unsigned long)gif->fbmask,(unsigned long long)gif->triangles_drawn,(unsigned long long)gif->sprites_drawn);
    fprintf(f,"GS_DATE enabled=%d datm=%d tested=%llu rejected=%llu scope=destination-alpha-color-and-depth-rejection\n",gif->date,gif->datm,(unsigned long long)gif_date_stat(0),(unsigned long long)gif_date_stat(1));
    fprintf(f,"GS_PALETTE cpsm=%u csm=%u csa=%u cbp=%u cld=%u cbw=%u cou=%u cov=%u\n",
        gif_get_state()->tex_cpsm,gif_get_state()->tex_csm,gif_get_state()->tex_csa,
        gif_get_state()->tex_cbp,gif_get_state()->tex_cld,gif_get_state()->texclut_cbw,
        gif_get_state()->texclut_cou,gif_get_state()->texclut_cov);
    const vu1_state_t *kick=vu1_get_state();
    fprintf(f,"VU1_XGKICK pending=%u addr=%u packet_qwc=%u ring_qwc=%u eop=%u credit=%llu cycle=%llu stalls=%llu owner=%u pending_paths=%u\n",
        kick->xgkick_pending,kick->xgkick_addr,kick->xgkick_packet_qwords,kick->xgkick_ring_qwords,
        kick->xgkick_eop,(unsigned long long)kick->xgkick_cycle_credit,
        (unsigned long long)kick->pipeline.cycle,(unsigned long long)kick->xgkick_busy_stalls,
        gif_get_state()->arb_owner,gif_get_state()->arb_pending);
    fprintf(f,"GS_DETAIL zmsk=%lu zte=%lu ztst=%lu tex_psm=%lu tw=%lu th=%lu tcc=%lu tfx=%lu mmag=%lu mmin=%lu dthe=%lu fba=%lu\n",
        (unsigned long)gif->zmsk,(unsigned long)gif->zte,(unsigned long)gif->ztst,(unsigned long)gif->tex_psm,
        (unsigned long)gif->tex_tw,(unsigned long)gif->tex_th,(unsigned long)gif->tex_tcc,(unsigned long)gif->tex_tfx,
        (unsigned long)gif->tex1_mmag,(unsigned long)gif->tex1_mmin,(unsigned long)gif->dthe,(unsigned long)gif->fba);
    fprintf(f,"GS_ROUTE draws=%llu texture=%llu blend=%llu zwrite=%llu ztest=%llu alpha=%llu mask=%llu dither_fba=%llu fog=%llu gouraud_tri=%llu cached_sprite=%llu fast_rows=%llu\n",
        (unsigned long long)gif_get_render_work(0),(unsigned long long)gif_get_render_work(1),
        (unsigned long long)gif_get_render_work(2),(unsigned long long)gif_get_render_work(3),
        (unsigned long long)gif_get_render_work(4),(unsigned long long)gif_get_render_work(5),
        (unsigned long long)gif_get_render_work(6),(unsigned long long)gif_get_render_work(7),
        (unsigned long long)gif_get_render_work(8),(unsigned long long)gif_get_render_work(9),
        (unsigned long long)gif_get_render_work(10),(unsigned long long)gif_get_render_work(11));
#ifdef GEKKO2_GX_NATIVE_BIOS_TRI
    fprintf(f,"GX_NATIVE_MODE enabled=1 force_gouraud_resident=1 precision=experimental-GX\n");
#else
    fprintf(f,"GX_NATIVE_MODE enabled=0 force_gouraud_resident=0 precision=GS-software\n");
#endif
    fprintf(f,"GX_NATIVE_TEXTURE attempts=%llu accepted=%llu screen_bbox_pixels=%llu CPU_texels=%llu alpha_rejects=%llu GPU_vertices=%llu STQ=%llu Gouraud=%llu varying_alpha=%llu CPU_alpha_pixels=%llu additive_source_alpha=%llu scope=experimental-GX-RGB-exact-alpha-shadow\n",
      (unsigned long long)gs_gx_native_affine_stat(0),(unsigned long long)gs_gx_native_affine_stat(1),
      (unsigned long long)gs_gx_native_affine_stat(2),(unsigned long long)gs_gx_native_affine_stat(3),
      (unsigned long long)gs_gx_native_affine_stat(4),(unsigned long long)gs_gx_native_affine_stat(5),
      (unsigned long long)gs_gx_native_affine_stat(6),(unsigned long long)gs_gx_native_affine_stat(7),
      (unsigned long long)gs_gx_native_affine_stat(8),(unsigned long long)gs_gx_native_affine_stat(9),
      (unsigned long long)gs_gx_native_affine_stat(10));
#ifdef GEKKO2_GX_EXTENDED_TRIANGLES
    fprintf(f,"GX_EXTENDED_MODE compiled=1 gate=GX_GOURAUD+GX_RESIDENT precision=experimental-GX\n");
#else
    fprintf(f,"GX_EXTENDED_MODE compiled=0 gate=disabled precision=legacy-path\n");
#endif
    fprintf(f,"GX_RISK_MODE available=%u requested=%d enabled=%d candidates=%llu fast_draws=%llu fallback=%llu vertices_saved=%llu apply=cold-boot coverage=experimental-GX-edges\n",
      (gekko2_experimental_available()>>GEKKO2_EXP_GX_RISK)&1u,
      gekko2_experimental_requested(GEKKO2_EXP_GX_RISK),gekko2_experimental_enabled(GEKKO2_EXP_GX_RISK),
      (unsigned long long)gs_gx_extended_stat(12),(unsigned long long)gs_gx_extended_stat(13),
      (unsigned long long)gs_gx_extended_stat(14),(unsigned long long)gs_gx_extended_stat(15));
#ifdef GEKKO2_GX_GENERIC_SHADER
    fprintf(f,"GX_COMMON_SHADER compiled=1 untextured=%llu highlight=%llu fog=%llu varying_vertex_alpha=%llu\n",
      (unsigned long long)gs_gx_extended_stat(8),(unsigned long long)gs_gx_extended_stat(9),
      (unsigned long long)gs_gx_extended_stat(10),(unsigned long long)gs_gx_extended_stat(11));
#else
    fprintf(f,"GX_COMMON_SHADER compiled=0\n");
#endif
    fprintf(f,"GX_EXTENDED_TRIANGLES accepted=%llu bbox_pixels=%llu fragment_checks=%llu color_rejected=%llu z_permissions=%llu source_cache_hits=%llu attempts=%llu GPU_vertices=%llu scope=experimental-GX-RGB-CPU-GS-tests\n",
      (unsigned long long)gs_gx_extended_stat(0),(unsigned long long)gs_gx_extended_stat(1),
      (unsigned long long)gs_gx_extended_stat(2),(unsigned long long)gs_gx_extended_stat(3),
      (unsigned long long)gs_gx_extended_stat(4),(unsigned long long)gs_gx_extended_stat(5),
      (unsigned long long)gs_gx_extended_stat(6),(unsigned long long)gs_gx_extended_stat(7));
    fprintf(f,"GS_TRIANGLE_TEXTURE_CACHE draws=%llu scope=disjoint-draw-local\n",(unsigned long long)gif_get_triangle_texture_cache_draws());
    /* R1330-K: Gouraud/triangle accounting.  submitted = degenerate + offscreen + visible;
     * visible = gx + software.  These are NOT the legacy gouraud_tri counter above. */
    fprintf(f,"GS_GOURAUD submitted=%llu degenerate=%llu offscreen=%llu visible=%llu gx=%llu software=%llu deg_identical3=%llu deg_two_same=%llu deg_collinear=%llu deg_origin=%llu flat_submitted=%llu flat_degenerate=%llu sw_bbox_px=%llu gx_bbox_px=%llu gx_hybrid=%llu gx_deferred=%llu gx_cpu_px=%llu\n",
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_SUBMITTED),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEGENERATE),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_OFFSCREEN),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_VISIBLE),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_GX),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_SOFTWARE),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEG_IDENTICAL3),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEG_TWO_SAME),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEG_COLLINEAR),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEG_ORIGIN),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_FLAT_SUBMITTED),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_FLAT_DEGENERATE),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_SW_BBOX_PIXELS),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_GX_BBOX_PIXELS),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_GX_HYBRID),(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_GX_DEFERRED),
        (unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_GX_CPU_PIXELS));
    fprintf(f,"GX_SHADED_TRIANGLES attempts=%llu accepted=%llu bbox_pixels=%llu mode=exact-CPU-shading-GX-resident-blend\n",(unsigned long long)gs_gx_shaded_triangle_stat(0),(unsigned long long)gs_gx_shaded_triangle_stat(1),(unsigned long long)gs_gx_shaded_triangle_stat(2));
    fprintf(f,"GS_GOURAUD_SW_REASON");
    for(unsigned r=1;r<GIF_GFB_COUNT;r++)fprintf(f," %s=%llu",gif_gouraud_fallback_name(r),(unsigned long long)gif_get_gouraud_fallback(r));
    fprintf(f,"\n");
    {
        uint32_t key[64];uint64_t cnt[64];unsigned used=0;
        while(used<64u&&gif_get_gouraud_state(used,&key[used],&cnt[used]))used++;
        for(unsigned rank=0;rank<8u&&rank<used;rank++) { /* partial selection sort */
            unsigned best=rank;
            for(unsigned c=rank+1u;c<used;c++)if(cnt[c]>cnt[best])best=c;
            uint64_t tc=cnt[rank];uint32_t tk=key[rank];cnt[rank]=cnt[best];key[rank]=key[best];cnt[best]=tc;key[best]=tk;
            fprintf(f,"GS_GOURAUD_STATE rank=%u key=0x%04lx count=%llu\n",rank+1u,(unsigned long)key[rank],(unsigned long long)cnt[rank]);
        }
        fprintf(f,"GS_GOURAUD_STATE_OVERFLOW %llu\n",(unsigned long long)gif_get_gouraud_state_overflow());
    }
    {
        int32_t xy[6];uint32_t prim;
        for(unsigned i=0;i<4u&&gif_get_degenerate_sample(i,xy,&prim);i++)
            fprintf(f,"GS_GOURAUD_DEGEN_SAMPLE i=%u prim=%lx xy=%ld,%ld %ld,%ld %ld,%ld\n",i,(unsigned long)prim,
                (long)xy[0],(long)xy[1],(long)xy[2],(long)xy[3],(long)xy[4],(long)xy[5]);
    }
    {uint32_t v[10];for(unsigned i=0;i<4u&&gif_get_vertex_source(i,v);i++)
      fprintf(f,"GS_VERTEX_SOURCE i=%u path=%lu encoding=%lu EE_PC=%08lx prim=%lx raw=%08lx,%08lx,%08lx offset=%lu,%lu kick=%lu\n",
       i,(unsigned long)v[0],(unsigned long)v[1],(unsigned long)v[2],(unsigned long)v[3],
       (unsigned long)v[4],(unsigned long)v[5],(unsigned long)v[6],(unsigned long)v[7],(unsigned long)v[8],(unsigned long)v[9]);}
    {uint32_t q;memcpy(&q,&gif_get_state()->cur_q,4);
     fprintf(f,"GS_Q gif_temporary=%08lx gs_rgbaq=%08lx independent=1\n",
       (unsigned long)(uint32_t)gif_get_state()->gif_q_bits,(unsigned long)q);}
    fprintf(f,"BUILD checkpoint=" GEKKO2_BUILD_NAME " scope=tekken-xpad-vu-ior-cdrom-state mask=%08lx next_boot_mask=%08lx resident_pipeline=%d EE_blocks=%d scheduler_quanta=%d\n",
        (unsigned long)gekko2_optimization_mask,(unsigned long)gekko2_opt_requested(),
        gekko2_opt_enabled(GEKKO2_OPT_GX_RESIDENT),
        gekko2_opt_enabled(GEKKO2_OPT_EE_JIT)&&gekko2_opt_enabled(GEKKO2_OPT_EE_BLOCKS),
        GEKKO2_SCHEDULER_QUANTA_ENABLED&&gekko2_opt_enabled(GEKKO2_OPT_EE_JIT)&&gekko2_opt_enabled(GEKKO2_OPT_EE_BLOCKS));
#ifdef GEKKO2_SPEED_STUB
    { extern uint64_t ee_quiet_hits,ee_quiet_arms,ee_quiet_arm_zero,ee_quiet_tlb_fills;
      fprintf(f,"SPEED_STUB beta=SPEED4 stub_hits=%llu arms=%llu arm_zero=%llu tlb_fills=%llu (nicht auf Wii getestet)\n",(unsigned long long)ee_quiet_hits,(unsigned long long)ee_quiet_arms,(unsigned long long)ee_quiet_arm_zero,(unsigned long long)ee_quiet_tlb_fills); }
#endif
#ifdef GEKKO2_IDLE_SKIP
    { extern uint64_t ee_idle_skip_stats[16];
      fprintf(f,"IDLE_SKIP skips=%llu slots=%llu pc_seen=%llu no_vu_dma=%llu no_rpc_sif=%llu no_irq=%llu no_timer=%llu (nicht auf Wii getestet)\n",(unsigned long long)ee_idle_skip_stats[0],(unsigned long long)ee_idle_skip_stats[1],(unsigned long long)ee_idle_skip_stats[2],(unsigned long long)ee_idle_skip_stats[3],(unsigned long long)ee_idle_skip_stats[4],(unsigned long long)ee_idle_skip_stats[5],(unsigned long long)ee_idle_skip_stats[6]);  extern uint64_t iop_spin_skip_stats[4]; fprintf(f,"IOP_SPIN_SKIP runs=%llu ticks=%llu (nicht auf Wii getestet)\n",(unsigned long long)iop_spin_skip_stats[0],(unsigned long long)iop_spin_skip_stats[1]);
      { extern uint32_t ee_dma_diag[64]; extern uint64_t ee_dma_diag_hits; unsigned i;
        fprintf(f,"DMA_DIAG hits=%llu pc=%08x (nicht auf Wii getestet)\n",(unsigned long long)ee_dma_diag_hits,ee_dma_diag[50]);
        for(i=0;i<10;i++)fprintf(f,"DMA_CH%u chcr=%08x madr=%08x qwc=%08x tadr=%08x\n",i,ee_dma_diag[i*4],ee_dma_diag[i*4+1],ee_dma_diag[i*4+2],ee_dma_diag[i*4+3]);
        {extern uint64_t ee_spin_stats[4];fprintf(f,"SPIN_SKIP hashes=%llu confirmed=%llu skips=%llu slots=%llu (nicht auf Wii getestet)\n",(unsigned long long)ee_spin_stats[0],(unsigned long long)ee_spin_stats[1],(unsigned long long)ee_spin_stats[2],(unsigned long long)ee_spin_stats[3]);}
        fprintf(f,"DMA_GLOBAL ctrl=%08x stat=%08x rbsr=%08x rbor=%08x stadr=%08x giffifo=%u vu1=%u giftagpend=%u viftagpend1=%u vifend1=%u\n",ee_dma_diag[40],ee_dma_diag[41],ee_dma_diag[42],ee_dma_diag[43],ee_dma_diag[44],ee_dma_diag[45],ee_dma_diag[46],ee_dma_diag[47],ee_dma_diag[48],ee_dma_diag[49]); } }
#endif
#ifdef GEKKO2_SPEED_STUB
    { extern unsigned ee_jit_hotpc_top(uint32_t*,uint32_t*,uint64_t*,unsigned);
      uint32_t hp[16],hr[16];uint64_t ht[16];unsigned hn=ee_jit_hotpc_top(hp,hr,ht,16);
      fprintf(f,"EE_HOTPC n=%u",hn);
      for(unsigned i=0;i<hn;i++)fprintf(f," %08x:%u:%llu",hp[i],hr[i],(unsigned long long)ht[i]);
      fprintf(f,"\n"); }
#endif
#ifdef GEKKO2_SPEED_STUB
    { extern uint64_t ee_core_scalar_hot_top(uint32_t *,uint32_t *,unsigned);
      uint32_t pcs[8], hits[8];
      uint64_t total=ee_core_scalar_hot_top(pcs,hits,8u);
      fprintf(f,"EE_SCALAR_HOT samples=%llu n=8",(unsigned long long)total);
      for(unsigned k=0;k<8u;k++)fprintf(f," %08x:%lu",pcs[k],(unsigned long)hits[k]);
      fprintf(f,"\n"); }
#endif
#ifdef GEKKO2_SPEED_RETIRE
    { extern uint64_t ee_speed_retire_stats[2];
      fprintf(f,"SPEED_RETIRE beta=SPEED1 lite=%llu full=%llu (nicht auf Wii getestet)\n",(unsigned long long)ee_speed_retire_stats[0],(unsigned long long)ee_speed_retire_stats[1]); }
#endif
    fprintf(f,"EVENT_SCHEDULER enabled=%d batches=%llu iop_ticks=%llu shared_barriers=%llu catchups=%llu width_sum=%llu max_ee_slots=64 ratio=8\n",
      gekko2_opt_enabled(GEKKO2_OPT_EVENT_SCHEDULER),(unsigned long long)system_event_stat(0),
      (unsigned long long)system_event_stat(1),(unsigned long long)system_event_stat(2),
      (unsigned long long)system_event_stat(3),(unsigned long long)system_event_stat(4));
    fprintf(f,"SYNC_REASONS scope=active_EE_barrier_calls SIF_read=");
    for(unsigned r=0;r<7u;r++)fprintf(f,"%s%llu",r?",":"",(unsigned long long)system_sync_reason_stat(r));
    fprintf(f," SIF_write=");
    for(unsigned r=7u;r<14u;r++)fprintf(f,"%s%llu",r>7u?",":"",(unsigned long long)system_sync_reason_stat(r));
    fprintf(f," DMA_read=%llu DMA_write=%llu IOP_RAM_read=%llu IOP_RAM_write=%llu other=%llu order=MSCOM,SMCOM,MSFLAG,SMFLAG,CTRL,F250,F260\n",
      (unsigned long long)system_sync_reason_stat(14u),(unsigned long long)system_sync_reason_stat(15u),
      (unsigned long long)system_sync_reason_stat(16u),(unsigned long long)system_sync_reason_stat(17u),
      (unsigned long long)system_sync_reason_stat(18u));
    fprintf(f,"IOP_SERVICE_READ calls=%llu bytes=%llu declines=%llu scope=registered-sysclib-readonly\n",
      (unsigned long long)iop_service_hle_stat(7),(unsigned long long)iop_service_hle_stat(8),
      (unsigned long long)iop_service_hle_stat(9));
    fprintf(f,"IOP_SERVICE_HLE enabled=%d registered=%llu calls=%llu stale=%llu invalid_argument=%llu scope=registered-sio2-sysclib\n",
      gekko2_opt_enabled(GEKKO2_OPT_IOP_SERVICE_HLE),(unsigned long long)iop_service_hle_stat(0),
      (unsigned long long)iop_service_hle_stat(1),(unsigned long long)iop_service_hle_stat(2),(unsigned long long)iop_service_hle_stat(3));
    fprintf(f,"IOP_CATCHUP_BLOCKS enabled=%d adaptive=%d growths=%llu maximum=8\n",
      gekko2_opt_enabled(GEKKO2_OPT_IOP_CATCHUP_BLOCKS),gekko2_opt_enabled(GEKKO2_OPT_IOP_BLOCK_GROWTH),
      (unsigned long long)iop_jit_get_block_growths());
    fprintf(f,"IOP_RAM_SERVICE calls=%llu bytes=%llu declines=%llu\n",
      (unsigned long long)iop_service_hle_stat(4),(unsigned long long)iop_service_hle_stat(5),
      (unsigned long long)iop_service_hle_stat(6));
    fprintf(f,"IOP_IDLE_BATCH enabled=%d ticks=%llu runs=%llu\n",gekko2_opt_enabled(GEKKO2_OPT_IOP_IDLE_BATCH),
      (unsigned long long)iop_core_idle_batch_stat(0),(unsigned long long)iop_core_idle_batch_stat(1));
    fprintf(f,"EE_LONG_BLOCKS enabled=%d maximum=32\n",gekko2_opt_enabled(GEKKO2_OPT_LONG_EE_BLOCKS));
    fprintf(f,"UNIT_PLAN host_quanta=%u native_horizon=64 event_window_max=64 ee_iop_slot_ratio=8 vu_reentry=%d vu_execution=synchronous ee_entry_admission=%d\n",
        gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES)?64u:8u,
        gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES)&&gekko2_opt_enabled(GEKKO2_OPT_VU_JIT),
        gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES));
    fprintf(f,"EE_EVENT_CHAIN enabled=%d runs=%llu retired=%llu selected_successors=%llu\n",
      gekko2_opt_enabled(GEKKO2_OPT_NATIVE_LINKS)&&gekko2_opt_enabled(GEKKO2_OPT_NATIVE_EVENTS)&&gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES),(unsigned long long)ee_jit_get_event_chain_stat(0),
      (unsigned long long)ee_jit_get_event_chain_stat(1),(unsigned long long)ee_jit_get_event_chain_stat(2));
    fprintf(f,"EE_NATIVE_WINDOW enabled=%d runs=%llu retired=%llu probes=%llu dependent_runs=%llu\n",
      gekko2_opt_enabled(GEKKO2_OPT_NATIVE_EVENTS),(unsigned long long)ee_core_native_window_stat(0),
      (unsigned long long)ee_core_native_window_stat(1),(unsigned long long)ee_core_native_window_stat(2),(unsigned long long)ee_core_native_window_stat(3));
    fprintf(f,"VU0_CONSTANT vf00=%08lx,%08lx,%08lx,%08lx jit_reads=hardwired\n",
      (unsigned long)ee_runtime->vu0_vf[0][0],(unsigned long)ee_runtime->vu0_vf[0][1],
      (unsigned long)ee_runtime->vu0_vf[0][2],(unsigned long)ee_runtime->vu0_vf[0][3]);
    fprintf(f,"VU_EE_WINDOWS micro0=%llu data0=%llu micro1=%llu data1=%llu\n",
       (unsigned long long)ee_core_vu_window_stat(0),(unsigned long long)ee_core_vu_window_stat(1),
       (unsigned long long)ee_core_vu_window_stat(2),(unsigned long long)ee_core_vu_window_stat(3));
    fprintf(f,"EE_CPU_FIFO VIF0=%llu VIF1=%llu GIF=%llu\n",
       (unsigned long long)ee_core_cpu_fifo_stat(0),(unsigned long long)ee_core_cpu_fifo_stat(1),(unsigned long long)ee_core_cpu_fifo_stat(2));
    fprintf(f,"CODE_ARENA enabled=%d capacity=%lu used=%lu high_water=%lu failures=%llu\n",
        gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA),(unsigned long)ppc_code_cache_capacity(),(unsigned long)ppc_code_cache_used(),
        (unsigned long)ppc_code_cache_high_water(),(unsigned long long)ppc_code_cache_failures());
    ppc_code_cache_info arena_info;ppc_code_cache_get_info(&arena_info);
    fprintf(f,"CODE_ARENA_LAYOUT begin=%08lx end=%08lx memory=%s free=%lu largest_free=%lu holes=%lu owners=%lu alloc=%llu release=%llu scans=%llu admission_reuses=%llu policy=bounded-clock lookup=exact32-to1KiB-4way\n",
      (unsigned long)arena_info.begin,(unsigned long)arena_info.end,
      arena_info.begin>=0x80000000u&&arena_info.end<=0x81800000u?"MEM1":"other",
      (unsigned long)arena_info.free_bytes,(unsigned long)arena_info.largest_free_payload,
      (unsigned long)arena_info.free_spans,(unsigned long)arena_info.live_spans,
      (unsigned long long)arena_info.allocations,(unsigned long long)arena_info.releases,
      (unsigned long long)arena_info.scan_steps,(unsigned long long)arena_info.admission_reuses);
    fprintf(f,"CHD_READER requests=%llu hunk_decodes=%llu hunk_reuses=%llu decode_errors=%llu scope=read-only-DVD-single-data-track-CD\n",
      (unsigned long long)gekko2_chd_stat(0),(unsigned long long)gekko2_chd_stat(1),(unsigned long long)gekko2_chd_stat(2),(unsigned long long)gekko2_chd_stat(3));
    fprintf(f,"ISO_CACHE requests=%llu hits=%llu fills=%llu physical_bytes=%llu failures=%llu capacity_sectors=16\n",
      (unsigned long long)iso_cache_stat(0),(unsigned long long)iso_cache_stat(1),
      (unsigned long long)iso_cache_stat(2),(unsigned long long)iso_cache_stat(3),(unsigned long long)iso_cache_stat(4));
    fprintf(f,"LIGHTREC_IOP compiled=%d enabled=%d ready=%llu bodies=%llu program_misses=%llu failures=%llu code_pool=%llu scope=MULT-MULTU-DIV-DIVU-pilot\n",
      GEKKO2_BUILD_LIGHTREC,gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT),
      (unsigned long long)iop_lightrec_stat(0),(unsigned long long)iop_lightrec_stat(1),
      (unsigned long long)iop_lightrec_stat(2),(unsigned long long)iop_lightrec_stat(3),(unsigned long long)iop_lightrec_stat(4));
    fprintf(f,"LIGHTREC_ROUTE eligible=%llu fallbacks=%llu native_block_hook=%d\n",
        (unsigned long long)iop_lightrec_stat(5),(unsigned long long)iop_lightrec_stat(6),GEKKO2_BUILD_LIGHTREC);
    fprintf(f,"FASTMEM enabled=%d ram_page_hits=%llu slow_translations=%llu invalidations=%llu hardware_MMU=%d\n",
        gekko2_opt_enabled(GEKKO2_OPT_FASTMEM),(unsigned long long)ee_fastmem_stat(0),
        (unsigned long long)ee_fastmem_stat(1),(unsigned long long)ee_fastmem_stat(2),dsi_fastmem_status()==1);
    fprintf(f,"TOOLCHAIN gcc=%s libogc=3.1.0 DSI_build=%d\n",__VERSION__,GEKKO2_BUILD_DSI);
    fprintf(f,"DSI_FASTMEM compiled=%d status=%d enabled=%d scope=EE-LW-LWU-precise-blocks faults=%lu patched=%lu last_pc=%08lx last_dar=%08lx probes=%lu emitted=%lu block_sites=%lu block_prepares=%lu\n",
      GEKKO2_BUILD_DSI,dsi_fastmem_status(),dsi_fastmem_enabled(),
      (unsigned long)dsi_fastmem_stat(0),(unsigned long)dsi_fastmem_stat(1),
      (unsigned long)dsi_fastmem_stat(2),(unsigned long)dsi_fastmem_stat(3),(unsigned long)dsi_fastmem_stat(4),(unsigned long)dsi_fastmem_stat(5),(unsigned long)dsi_fastmem_stat(6),(unsigned long)dsi_fastmem_stat(7));
    fprintf(f,"OPT_ROUTES EE=%d IOP=%d VU=%d links=%d residency=%d word_alloc=%d GX_source_cache=%d GX_Gouraud=%d GX_disjoint_writes=%d\n",
        gekko2_opt_enabled(GEKKO2_OPT_EE_JIT),gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT),gekko2_opt_enabled(GEKKO2_OPT_VU_JIT),
        gekko2_opt_enabled(GEKKO2_OPT_NATIVE_LINKS),gekko2_opt_enabled(GEKKO2_OPT_RESIDENCY),gekko2_opt_enabled(GEKKO2_OPT_WORD_ALLOCATION),
        gekko2_opt_enabled(GEKKO2_OPT_GX_SOURCE_CACHE),gekko2_opt_enabled(GEKKO2_OPT_GX_GOURAUD),gekko2_opt_enabled(GEKKO2_OPT_GX_DISJOINT_WRITES));
    fprintf(f,"GX_SYNC reuse=%llu readback=%llu present=%llu reuse_tb=%llu readback_tb=%llu present_tb=%llu\n",
      (unsigned long long)gs_gx_sync_count(0),(unsigned long long)gs_gx_sync_count(1),(unsigned long long)gs_gx_sync_count(2),
      (unsigned long long)gs_gx_sync_count(4),(unsigned long long)gs_gx_sync_count(5),(unsigned long long)gs_gx_sync_count(6));
    fprintf(f,"EE_BLOCK runs=%llu retired=%llu native_successors=%llu dispatch_hits=%llu\n",(unsigned long long)ee_jit_get_block_count(),(unsigned long long)ee_jit_get_block_retired(),(unsigned long long)ee_jit_get_native_successors(),(unsigned long long)ee_jit_get_dispatch_hits());
#ifdef GEKKO2_EE_AOT
    fprintf(f,"EE_AOT blocks=%u retired=%llu\n",ee_aot_block_count(),(unsigned long long)ee_aot_retired_count());
    fprintf(f,"IOP_VU_AOT IOP=%llu VU_upper=%llu VU_lower=%llu\n",(unsigned long long)iop_aot_executed_count(),(unsigned long long)vu_aot_executed_count(0),(unsigned long long)vu_aot_executed_count(1));
#endif
#ifdef GEKKO2_COUNT_BOUNDARIES
    fprintf(f,"EE_BOUNDARY fused_mod32=%llu\n",(unsigned long long)ee_core_get_fused_boundaries());
#endif
    fprintf(f,"EE_PREFIX runs=%llu retired=%llu enabled=%d services=%d\n",
            (unsigned long long)ee_jit_get_budget_cache_stat(6),
            (unsigned long long)ee_jit_get_budget_cache_stat(7),
            gekko2_opt_enabled(GEKKO2_OPT_EE_PREFIX),gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES));
    fprintf(f,"IOP_DEADLINES skipped=%llu scans=%llu exposed=%llu enabled=%d\n",
            (unsigned long long)iop_hle_thread_deadline_stat(0),
            (unsigned long long)iop_hle_thread_deadline_stat(1),
            (unsigned long long)iop_hle_thread_deadline_stat(2),gekko2_opt_enabled(GEKKO2_OPT_IOP_DEADLINES));
    fprintf(f,"IOP_JIT compiled=%u executed=%llu rejected_hits=%llu\n",
            (unsigned)iop_jit_get_cache_size(),
            (unsigned long long)iop_jit_get_executed_count(),
            (unsigned long long)iop_jit_get_rejected_hit_count());
    fprintf(f,"WORD_ALLOC bodies=%llu eliminated=%llu spills=%llu reused_loads=%llu\n",
            (unsigned long long)ppc_dynarec_get_allocated_bodies(),
            (unsigned long long)ppc_dynarec_get_eliminated_word_ops(),
            (unsigned long long)ppc_dynarec_get_word_spills(),
            (unsigned long long)ppc_dynarec_get_reused_word_loads());
    fprintf(f,"RESIDENT blocks=%llu removed_loads=%llu refresh_edges=%llu\n",
        (unsigned long long)ppc_dynarec_get_resident_blocks(),
        (unsigned long long)ppc_dynarec_get_resident_loads(),
        (unsigned long long)ppc_dynarec_get_resident_refresh_edges());
    fprintf(f,"IOP_BLOCK compiled=%u runs=%llu ticks=%llu stale=%llu\n",
            (unsigned)iop_jit_get_block_cache_size(),
            (unsigned long long)iop_jit_get_block_runs(),
            (unsigned long long)iop_jit_get_block_ticks(),
            (unsigned long long)iop_core_block_stale_count());
    fprintf(f,"VU_JIT compiled=%u upper=%llu lower=%llu rejected_hits=%llu pairs=%llu blocks=%llu\n",
            (unsigned)vu_jit_get_cache_size(),
            (unsigned long long)vu_jit_get_upper_count(),
            (unsigned long long)vu_jit_get_lower_count(),
            (unsigned long long)vu_jit_get_rejected_hit_count(),
            (unsigned long long)vu_jit_get_pair_count(),
            (unsigned long long)vu_jit_get_block_count());frontend_log_close(&g_session_log,f);g_log_ticks+=gettime()-log_begin;
}

/* Round 29 continued (task #126): real BIOS boot as the PRIMARY,
 * automatic action - see the top-of-file header comment for the full
 * rationale. Runs the actual EE/IOP interleaved scheduler in bounded
 * chunks (so the UI keeps redrawing/responding every ~200k IOP
 * instructions instead of blocking for tens of millions at once),
 * checks the REAL GS privileged registers each chunk, and blits the
 * REAL GS local memory content the instant a display gets configured
 * - not a canned test pattern. Bounded by a generous but finite
 * safety cap (not "a real limit" - this project's own diagnostics
 * have run well past it - just a guard against a truly-never-ending
 * loop with no way out other than power-cycling); the user can also
 * hold B at any time to stop early and drop into the secondary test
 * menu below. */
#define BOOT_CHUNK_SLICES 50000ull  /* IOP-instruction budget per redraw, keeps the UI responsive */
#define BOOT_TOTAL_CAP    2000000000ull /* generous overall safety cap, not a real limit - see comment above */

/* Round 119 (task #172/#274): this used to be a static function
 * defined right here. It was moved into gs_wii_output.c/.h
 * (gs_decode_dispfb) so it could actually be unit-tested host-
 * natively - everything in this file depends on <gccore.h> (the real
 * Wii SDK), which isn't available in this project's host-native test
 * environment, so nothing defined in main.c has ever been directly
 * testable. The logic is byte-for-byte unchanged, only the name/
 * location changed - see gs_wii_output.h's doc comment for the real
 * PS2 GS DISPFB1/DISPFB2 register field layout this implements. */
#define decode_dispfb gs_decode_dispfb

/* Persist first fault evidence as text, once; no guest-memory writes. */
static int r1252_evidence_written;
static void r1252_save_fault_evidence(void)
{
    uint32_t f[112]={0}; ee_core_get_r1252_fault(f);
    if (!f[0] || r1252_evidence_written) return;
    FILE *fp=fopen("sd:/pcsx2/Gekko2-" GEKKO2_RELEASE "-first-fault.txt", "w");
    if (!fp) return;
    fprintf(fp,"Gekko2 " GEKKO2_RELEASE " JIT=%s disc=%d BIOS=%s version=%s size=%u\n",
#ifdef PCSX2WII_JIT_DISABLE
        "OFF",
#else
        "ON",
#endif
        g_disc_ok,g_bios.name,g_bios.version_string,(unsigned)g_bios.size);
    for(unsigned n=0;n<112;n++)fprintf(fp,"F[%03u]=%08lx\n",n,(unsigned long)f[n]);
    uint32_t hit=0,w[16]={0};ee_core_get_r1246_writer(&hit,w);
    fprintf(fp,"WRITER hit=%lu\n",(unsigned long)hit);
    for(unsigned n=0;n<16;n++)fprintf(fp,"W[%02u]=%08lx\n",n,(unsigned long)w[n]);
    ee_state_t *ee=ee_core_get_state();
    const uint32_t starts[]={0x00014280u,0x0100ec00u,0x010459c0u};
    for(unsigned i=0;i<3;i++)for(unsigned n=0;n<64;n++){
        uint32_t a=starts[i]+n*4;
        if(a+4<=ee->ram_size){uint8_t *b=ee->ram+a;
            uint32_t v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
            fprintf(fp,"RAM[%08lx]=%08lx\n",(unsigned long)a,(unsigned long)v);}
    }
    int failed=ferror(fp); int closed=fclose(fp);
    if(!failed && closed==0)r1252_evidence_written=1;
}

/* Live progress HUD for the automatic real boot flow - redrawn every
 * chunk so the user can see real instruction counts advancing (proof
 * the emulator core is genuinely executing, not frozen), whether each
 * core has halted (and why), and whether the BIOS/game has configured
 * a real GS display yet. */
static void draw_boot_progress_hud(uint64_t ee_instr, uint64_t iop_instr,
                                    int ee_halted, int iop_halted,
                                    const char *ee_reason, const char *iop_reason,
                                    int display_active)
{
    goto_rc(16, 40);
    printf("Gekko2 - Real BIOS Boot                                        \n");
    printf("===================================                              \n\n");
    printf("EE  instructions executed: %-16llu halted=%d              \n",
           (unsigned long long)ee_instr, ee_halted);
    printf("    %-64s\n", ee_halted ? ee_reason : "(still executing real instructions)");
    printf("JIT: %-12llu / %-12llu EE (%3llu%%) cache=%u L0=%llu/%llu\n",
           (unsigned long long)ee_jit_get_native_retired_count(),
           (unsigned long long)ee_instr,
           (unsigned long long)(ee_instr ? (ee_jit_get_native_retired_count() * 100u / ee_instr) : 0u),
           (unsigned)ee_jit_get_cache_size(),
           (unsigned long long)ee_jit_get_pc_l0_hit_count(),
           (unsigned long long)ee_jit_get_pc_l0_miss_count());
    printf("HELP: sqrt=%llu cvt=%llu mmiMD=%llu pmfhl=%llu                    \n",
           (unsigned long long)g_r1175_jit_sqrt_calls,
           (unsigned long long)g_r1175_jit_cvt_calls,
           (unsigned long long)g_r1175_jit_mmi_muldiv_calls,
           (unsigned long long)g_r1175_jit_pmfhl_calls);
    {
        unsigned long long n = (unsigned long long)g_r1176_prof_samples;
        printf("TB: n=%llu fetch=%llu jit=%llu B=%llu C=%llu H=%llu I=%llu ticks       \n",
               n,
               n ? (unsigned long long)g_r1176_prof_fetch_tb / n : 0ULL,
               n ? (unsigned long long)g_r1176_prof_jit_tb / n : 0ULL,
               n ? (unsigned long long)g_r1177_prof_base_tb / n : 0ULL,
               n ? (unsigned long long)g_r1177_prof_clock_tb / n : 0ULL,
               n ? (unsigned long long)g_r1177_prof_house_tb / n : 0ULL,
               n ? (unsigned long long)g_r1177_prof_irq_tb / n : 0ULL);
        printf("Csplit: TL=%llu VB=%llu BC=%llu GS=%llu TM=%llu ticks              \n",
               n ? (unsigned long long)g_r1178_prof_latch_tb / n : 0ULL,
               n ? (unsigned long long)g_r1178_prof_vblank_tb / n : 0ULL,
               n ? (unsigned long long)(g_r1179_prof_selfloop_tb + g_r1179_prof_eeload_tb + g_r1179_prof_escalate_tb + g_r1179_prof_carousel_tb + g_r1179_prof_sbus_tb) / n : 0ULL,
               n ? (unsigned long long)g_r1178_prof_gsvsync_tb / n : 0ULL,
               n ? (unsigned long long)g_r1178_prof_timers_tb / n : 0ULL);
        printf("BCsplit: SL=%llu EL=%llu ES=%llu CA=%llu SB=%llu ticks             \n",
               n ? (unsigned long long)g_r1179_prof_selfloop_tb / n : 0ULL,
               n ? (unsigned long long)g_r1179_prof_eeload_tb / n : 0ULL,
               n ? (unsigned long long)g_r1179_prof_escalate_tb / n : 0ULL,
               n ? (unsigned long long)g_r1179_prof_carousel_tb / n : 0ULL,
               n ? (unsigned long long)g_r1179_prof_sbus_tb / n : 0ULL);
        printf("SBdetail: G=%llu H=%llu match=%llu call=%llu unblock=%llu             \n",
               n ? (unsigned long long)g_r1181_prof_sbus_guard_tb / n : 0ULL,
               n ? (unsigned long long)g_r1181_prof_sbus_helper_tb / n : 0ULL,
               (unsigned long long)g_r1181_sbus_guard_match,
               (unsigned long long)g_r1181_sbus_helper_calls,
               (unsigned long long)g_r1181_sbus_unblocks);
        /* R1245: compact passive slot watcher; no guest-memory reads. */
        {
            int su=0; int32_t sm=0,sc=0,sw=0; uint64_t it=0,stc=0,hh=0,re=0; uint32_t cd=0,se=0,hit=0,sn[12];
            ee_hle_thread_get_sema_state(5,&su,&sm,&sc,&sw); ee_core_get_r1190_sif_diag(&it,&stc,&hh,&re,&cd,&se); ee_core_get_r1245_slottruth(&hit,sn);
            uint32_t wh=0,wsn[16]={0}; ee_core_get_r1246_writer(&wh,wsn);
#ifdef PCSX2WII_JIT_DISABLE
            const char *jm="OFF";
#else
            const char *jm="ON";
#endif
            printf("" GEKKO2_RELEASE " MENU / RPC-GS JIT=%s REND=%llu hh=%llu S5=%ld/%ld w=%ld | slot2=%lu writer=%lu\033[K\n",jm,(unsigned long long)re,(unsigned long long)hh,(long)sc,(long)sm,(long)sw,(unsigned long)hit,(unsigned long)wh);
            uint64_t rc[4]={0}; uint32_t rl[6]={0};
            ee_core_get_r1249_repair(rc,rl);
            printf("REPAIR=%llu T%lu CONT=%llu LD=%llu JR=%llu\033[K\n",
                (unsigned long long)rc[0],(unsigned long)rl[0],
                (unsigned long long)rc[1],(unsigned long long)rc[2],(unsigned long long)rc[3]);
            uint32_t ff[112]={0}; ee_core_get_r1252_fault(ff);
            printf("FAULT=%lu PC=%08lx VA=%08lx T%lu dump=%d DISC=%d\033[K\n",
                (unsigned long)ff[0],(unsigned long)ff[1],(unsigned long)ff[2],
                (unsigned long)ff[7],r1252_evidence_written,g_disc_ok);
            ee_state_t *live = ee_core_get_state();
            int tid = ee_hle_thread_get_current_thread_id();
            printf("CP SR=%08lx CA=%08lx EPC=%08lx BV=%08lx\033[K\n",
                (unsigned long)live->cop0[12], (unsigned long)live->cop0[13],
                (unsigned long)live->cop0[14], (unsigned long)live->cop0[8]);
            printf("T%d st=%lu pr=%lu wait=%lu/%lu SP=%08lx RA=%08lx\033[K\n",
                tid, (unsigned long)ee_hle_thread_get_status(tid),
                (unsigned long)ee_hle_thread_get_priority(tid),
                (unsigned long)ee_hle_thread_get_wait_type(tid),
                (unsigned long)ee_hle_thread_get_wait_id(tid),
                (unsigned long)live->gpr[29].ud0, (unsigned long)live->gpr[31].ud0);

        }
        }
    printf("IOP instructions executed: %-16llu halted=%d              \n",
           (unsigned long long)iop_instr, iop_halted);
    printf("    %-64s\n", iop_halted ? iop_reason : "(still executing real instructions)");
    printf("\n");
    printf("GS display: %-58s\n",
           display_active ? "configured by BIOS/game - showing real GS memory below"
                           : "not configured yet (see docs/STATUS.md's Round 29 notes)");
    printf("\nClassic/Pro/GamePad: PLUS=PS2 START, HOME=pause, HOME+PLUS=HUD\n");
    printf("LOG session=%lu errors=%lu errno=%d\033[K\n",(unsigned long)g_session_log.sequence,
      (unsigned long)g_session_log.errors,g_session_log.last_error);
}

/* The real boot flow itself - see the top-of-file header comment and
 * the doc comment above draw_boot_progress_hud() for the full design.
 * Runs automatically once at startup (called from main()) and is also
 * reachable again from the launcher ("Re-run Boot Flow"). */
static void run_real_boot_flow(void)
{
    if(gs_gx_faulted())return; /* Lost GPU work cannot be resumed safely. */
    draw_gradient_background(6, 6, 22, 0, 0, 6);
    flush_screen();
    goto_rc(16, 40);
    printf("Booting real PS2 BIOS...\n");

    if (!g_fat_mounted)
        g_fat_mounted = fatInitDefault() ? 1 : 0;
    if (!g_fat_mounted) {
        printf("\n[!] fatInitDefault() failed - no SD/USB storage found.\n");
        printf("    Insert an SD card with /pcsx2/bios/*.bin and restart to\n");
        printf("    boot a real BIOS. Press A or B to open the launcher.\n");
        wait_for_button(PAD_BUTTON_A | PAD_BUTTON_B);
        return;
    }

    if (!g_bios_ok) {
        memset(&g_bios, 0, sizeof(g_bios));
        /* Round 951 (task #447/#536/#887): SCPH50004.bin tried FIRST -
         * per the user's Round 950 pivot, this is now the project's
         * primary/focus BIOS (SCPH-10000's OSDSYS was never fully in
         * ROM to begin with - see docs/STATUS.md Round 950). Place the
         * uploaded SCPH-50004_BIOS_V9_EUR_190.BIN at
         * sd:/pcsx2/bios/SCPH50004.bin (Dolphin: inside the emulated
         * SD card image) for this build to pick it up. */
        g_bios_ok = (bios_load("sd:/pcsx2/bios/SCPH50004.bin", &g_bios) == 0 ||
                     bios_load("sd:/pcsx2/bios/SCPH39001.bin", &g_bios) == 0 ||
                     bios_load("sd:/pcsx2/bios/SCPH10000.bin", &g_bios) == 0 ||
                     bios_load("sd:/pcsx2/bios/bios.bin", &g_bios) == 0);
    }
    if (!g_bios_ok) {
        printf("\n[!] Could not load a PS2 BIOS image from sd:/pcsx2/bios/\n");
        printf("    Place a legally-dumped PS2 BIOS there (e.g. bios.bin) and\n");
        printf("    restart. Press A or B to open the launcher.\n");
        wait_for_button(PAD_BUTTON_A | PAD_BUTTON_B);
        return;
    }

    printf("[+] BIOS loaded: %s  size=%u  rom_ver=%s\n",
           g_bios.name, (unsigned)g_bios.size, g_bios.version_string);

    if (!g_system_started) {
        /* Cold boot changes code generation. Release every old code owner
         * before applying the next options; resumed sessions retain theirs. */
        ee_jit_reset_stats_for_test();
        vu_jit_reset_for_test();
        ppc_dynarec_reset_translation_stats();
        gekko2_opt_apply();
        gekko2_experimental_apply_cold_boot();
        g_gx_present=g_gx_requested;
        game_trace_reset();g_game_trace_cursor=0;g_game_context_reported=0;
        g_boot_started=gettime();g_boot_log_next=0;g_guest_error_reported=0;g_fileio_trace_previous_total=0;
        if(frontend_log_begin(&g_session_log,"sd:/pcsx2/logs",g_boot_disc,g_gx_present)<0)
            printf("Could not create session log (errno=%d).\n",g_session_log.last_error);
        if (system_init(&g_bios, &g_bios) != 0) {
            save_boot_progress("BOOT_FAILED");ee_core_shutdown();iop_core_shutdown();
            printf("Core initialization failed. A/B: launcher.\n");
            wait_for_button(PAD_BUTTON_A | PAD_BUTTON_B);return;
        }
        if(cdvd_config_bind_file("sd:/pcsx2/bios-config.bin")<0)printf("BIOS configuration file could not be loaded; saving disabled.\n");
        g_system_started = 1;
        g_first_picture=0;
        g_image_probe_attempts=0;
        g_gx_attempts=g_gx_fallbacks=0;
        g_sw_attempts=g_sw_copies=g_sw_sync_failures=g_sw_invalid=0;
        g_xfb_updates=g_xfb_guest_copies=g_xfb_hud_updates=0;
        gs_init();
        gs_mem_init();
        gs_mem_set_local_extended_conversion(gekko2_experimental_enabled(GEKKO2_EXP_PSM));
        gs_mem_reset_local_copy_stats();
        save_boot_progress("BOOT");
    } else {
        save_boot_progress("RESUMED");
    }

    /* Round 209: real disc mount, once, best-effort. Mounted on BOTH
     * real register interfaces since it is not yet established which
     * one (if either) real BIOS/kernel code actually uses to read
     * SYSTEM.CNF (Round 205-207) - giving the real boot every real
     * chance to succeed regardless of which path it tries. Missing
     * disc is logged, not fatal - matches real PS2 boot-with-no-disc
     * behavior.
     *
     * Round 610 (task #536 continuation): moved to AFTER system_init()
     * above - it used to run BEFORE, but system_init() -> iop_core_
     * init() -> iop_cdvd_init() unconditionally resets the CDVD
     * disc-type register back to IOP_CDVD_TYPE_NODISC (see iop_cdvd.c's
     * init()), which silently undid this exact mount/set_disc_present
     * call on every single real disc-mounted boot. This was a
     * pre-existing bug (not introduced this round) discovered while
     * host-native-verifying ee_check_browser_menu_escalation_
     * heuristic()'s new diskless-only gate: a disc-mounted boot trace
     * and a genuinely diskless boot trace were found to be byte-
     * identical because iop_cdvd_get_disc_type() read back NODISC in
     * both cases. Moving the mount to run after system_init() fixes
     * this for every caller of iop_cdvd_get_disc_type(), not just the
     * new Round 610 gate. */
    if (!g_disc_checked) {
        g_disc_checked = 1;
        if (!g_boot_disc) {
        g_disc_ok = 0;
        printf("[" GEKKO2_RELEASE "] BIOS mode: no disc mounted.\n");
        } else {
        int cdvd_rc = iop_cdvd_mount_iso(g_disc_path);
        int legacy_rc = iop_cdrom_legacy_mount_iso(g_disc_path);
        g_disc_ok = (cdvd_rc == 0) || (legacy_rc == 0);
        if (g_disc_ok) {
            if (cdvd_rc == 0) iop_cdvd_set_disc_present(0x12);
            printf("[+] Disc image mounted: %s\n",g_disc_path);
        } else {
            g_disc_checked=0;
            printf("[!] Selected image could not be mounted: %s\nA/B: launcher.\n",g_disc_path);
            wait_for_button(PAD_BUTTON_A|PAD_BUTTON_B);return;
        }
        }
    }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();
    gs_state_t  *gs  = gs_get_state();

    uint32_t frame = 0;
    uint64_t last_present_ms=0,last_present_events=ee_core_get_vblank_events();
    uint64_t last_present_quadwords=UINT64_MAX;
    uint64_t last_dispfb=UINT64_MAX,last_display=UINT64_MAX,last_pmode=UINT64_MAX,last_smode=UINT64_MAX;
    gs_gx_set_render_enabled(frontend_allow_gx_primitives(g_gx_present,g_first_picture));
    g_slice_budget=512;g_fps_milli=g_present_milli=0;
    memset(&g_fps_window,0,sizeof(g_fps_window));
    system_profile_reset();g_profile_previous=(system_profile_t){0};
    g_runtime_previous=g_front_previous=(gekko2_profile){0};g_log_ticks=g_log_previous=0;
    uint64_t total_slices = 0;
    int stopped_by_user = 0,exit_requested=0,gx_fault_paused=0;
    int show_hud = g_hud_default;g_hud_active=show_hud;

    if(gekko2_opt_enabled(GEKKO2_OPT_SOUND))audio_output_start();
    iop_sio2_pad_connect();
    frontend_pad_latch pad_latch={iop_sio2_pad_sample_count(),0,0};
    uint64_t perf_start=gettime(),perf_presents=0,perf_events=ee_core_get_vblank_events();
    uint64_t perf_ee=ee->instructions_executed,perf_core=0,perf_blit=0;

    for (;;) {
        wii_input_scan();
        if(g_input_exit){exit_requested=1;stopped_by_user=1;break;}
        uint16_t guest_pressed=0;unsigned controls=controller_session(&g_controller,&guest_pressed);
        if(controls==1u){stopped_by_user=1;break;}
        if(controls==2u){show_hud=!show_hud;g_hud_active=show_hud;last_present_ms=0;}
        if(g_controller.changed||!g_controller.pad.connected){
            pad_latch=(frontend_pad_latch){iop_sio2_pad_sample_count(),0,0};
        }
        iop_sio2_pad_set_buttons(frontend_pad_latch_update(&pad_latch,guest_pressed,iop_sio2_pad_sample_count()));
        iop_sio2_pad_set_analog_axes(g_controller.pad.rx,g_controller.pad.ry,g_controller.pad.lx,g_controller.pad.ly);

        uint64_t stage_started=gettime();
        if (!(ee->halted && iop->halted)) {
            system_run_interleaved(g_slice_budget);
            total_slices += g_slice_budget;
        }

        if(gs_gx_faulted()){gx_fault_paused=1;break;}
        uint64_t chunk_ticks=gettime()-stage_started;
        perf_core+=chunk_ticks;
        g_slice_budget=frontend_next_budget(g_slice_budget,(uint32_t)ticks_to_millisecs(chunk_ticks),g_throughput?50:20);
        stage_started=gettime();
        gekko2_profile_start_host();
        uint64_t now_display_ms=ticks_to_millisecs(stage_started);
        uint64_t current_events=ee_core_get_vblank_events();
        int present_due=frontend_present_due(now_display_ms,last_present_ms,current_events,last_present_events);

        /* PMODE bits 0/1 = EN1/EN2 (circuit 1/2 enabled) - real,
         * documented GS register semantics, not a guess.
         *
         * Round 212 fix (task #366/#172, 252nd finding): this used to
         * always call decode_dispfb(gs->dispfb1, ...) regardless of
         * which circuit PMODE actually enabled. A real PCSX2 debugger
         * session at the real BIOS splash screen (user-provided
         * screenshots) showed PMODE=0x66 - EN1=0, EN2=1 - with
         * DISPFB2/DISPLAY2 populated with real, structured values
         * (DISPFB2's FBW field decodes to 640px, a genuine PS2
         * resolution) while DISPFB1/DISPLAY1 stayed exactly zero, the
         * same "DISPLAY1 never written" symptom this project has
         * chased since the 94th/126th/223rd findings. That symptom is
         * consistent with Circuit 1 legitimately never being used by
         * this BIOS at all - Circuit 2 is what actually drives the
         * picture. The old code's `display_active` check already
         * correctly went true on EN2 alone (mask 0x3 covers both
         * bits), but the blit itself was hardcoded to Circuit 1's
         * dispfb, so even a real, hardware-accurate GS setup writing
         * only Circuit 2 would have silently produced no picture here.
         * Fixed by choosing the circuit to blit from based on which
         * EN bit is actually set (EN1 preferred if both are somehow
         * set, matching real hardware's Circuit-1-is-primary
         * convention; EN2 used otherwise) instead of assuming
         * Circuit 1. gs_state_t/gs.c already modeled dispfb2/display2
         * correctly at their real addresses (0x12000090/0x120000A0) -
         * only this call site needed the fix. */
        int en1 = (gs->pmode & 0x1u) != 0;
        int en2 = (gs->pmode & 0x2u) != 0;
        int display_active = en1 || en2;
        uint64_t current_quadwords=gif_get_state()->quadwords_seen;
        uint64_t current_dispfb=en1?gs->dispfb1:gs->dispfb2;
        uint64_t current_display=en1?gs->display1:gs->display2;
        /* Frequent input polling must not multiply expensive full-screen
         * blits for an unchanged BIOS framebuffer. GIF activity and display
         * changes refresh within 500 ms; real guest VBlank refreshes at once.
         * A five-second fallback covers writes outside normal GIF paths. */
        if(g_first_picture && !show_hud && current_events==last_present_events &&
           current_quadwords==last_present_quadwords && current_dispfb==last_dispfb &&
           current_display==last_display && gs->pmode==last_pmode && gs->smode2==last_smode &&
           now_display_ms>=last_present_ms && now_display_ms-last_present_ms<5000)present_due=0;
        if(frontend_probe_first_image(present_due,display_active,g_first_picture)) {
            uint32_t probe_bp,probe_bw,sx,sy,sw,sh;
            decode_dispfb(current_dispfb,&probe_bp,&probe_bw);
            gs_decode_display_region(current_dispfb,current_display,gs->smode2,&sx,&sy,&sw,&sh);
            g_image_probe_attempts++;
            if(probe_bw && gs_display_has_rgb_psm(probe_bp,probe_bw,sx,sy,sw,sh,
                         (uint32_t)((current_dispfb>>15)&31u))) {
                g_first_picture=1;save_boot_progress("FIRST_IMAGE");
            }
        }
        gs_gx_set_render_enabled(frontend_allow_gx_primitives(g_gx_present,g_first_picture));
        if(present_due)VIDEO_WaitVSync();
        int guest_copied=0;
        if (present_due && display_active && !show_hud) {
            uint64_t active_dispfb = en1 ? gs->dispfb1 : gs->dispfb2;
            uint32_t bp_words, bw_pixels;
            decode_dispfb(active_dispfb, &bp_words, &bw_pixels);
            if (bw_pixels > 0) {
                uint32_t blit_h = rmode->xfbHeight;
                uint32_t sx, sy, sw, sh;
                gs_decode_display_region(active_dispfb, en1 ? gs->display1 : gs->display2,
                                          gs->smode2, &sx, &sy, &sw, &sh);
                int gx_ok=0;
                if(frontend_allow_gx_output(g_gx_present,g_first_picture,(unsigned)((active_dispfb>>15)&31u))) {
                    g_gx_attempts++;
                    gx_ok=gs_gx_present_psm(xfb,rmode,bp_words,bw_pixels,sx,sy,sw,sh,(unsigned)((active_dispfb>>15)&31u));
                    if(gs_gx_faulted()){gekko2_profile_stop();gx_fault_paused=1;break;}
                    if(!gx_ok)g_gx_fallbacks++;
                    if(gx_ok)guest_copied=1;
                }
                if(g_first_picture && !gx_ok) {
                    g_sw_attempts++;
                    int copied=gs_blit_scaled_psm_to_xfb_checked(xfb, rmode->fbWidth, blit_h,
                                          bp_words, bw_pixels, sx, sy, sw, sh,
                                          (uint32_t)((active_dispfb>>15)&31u));
                    if(copied>0){g_sw_copies++;guest_copied=1;}
                    else if(copied<0)g_sw_sync_failures++;
                    else g_sw_invalid++;
                }

            }
        }

        if(gs_gx_faulted()){gekko2_profile_stop();gx_fault_paused=1;break;}
        if(present_due){
        if (show_hud || !display_active || !g_first_picture) {
        g_xfb_hud_updates++;
        draw_boot_progress_hud(ee->instructions_executed, iop->instructions_executed,
                                ee->halted, iop->halted, ee->halt_reason, iop->halt_reason,
                                display_active);
        if(!g_first_picture)printf("Waiting for BIOS pixels in VRAM.\033[K\n");
        else printf("BIOS pixels ready. HUD: HOME+PLUS (full pad), MINUS+PLUS / Z+START.\033[K\n");
        printf("GX output=%d ready=%d flat=%d first=%d pending=%lu errors=%lu\033[K\n",
          g_gx_present,gs_gx_ready(),g_gx_present,g_first_picture,
          (unsigned long)gs_mem_gpu_pending(),(unsigned long)gs_mem_sync_failures());
        frame++;
        } else {
            VIDEO_SetNextFramebuffer(xfb); VIDEO_Flush();
            frame++;
        }

        if(g_fps_default && g_first_picture && !show_hud)draw_fps_overlay();
        flush_screen();
        g_xfb_updates++;
        if(guest_copied)g_xfb_guest_copies++;
        perf_presents++;last_present_ms=now_display_ms;last_present_events=current_events;
        last_present_quadwords=current_quadwords;last_dispfb=current_dispfb;last_display=current_display;
        last_pmode=gs->pmode;last_smode=gs->smode2;
        }
        gekko2_profile_stop();
        perf_blit+=gettime()-stage_started;
        r1252_save_fault_evidence();
        if(ee->instructions_executed>=g_boot_log_next){save_boot_progress("PROGRESS");g_boot_log_next=ee->instructions_executed+50000000ull;}
        uint64_t perf_now=gettime(),perf_ms=ticks_to_millisecs(perf_now-perf_start);
        if(perf_ms>=5000ull){
            uint64_t now_events=ee_core_get_vblank_events();
            frontend_fps_push(&g_fps_window,perf_ms,now_events-perf_events,perf_presents);
            g_fps_milli=frontend_fps_guest(&g_fps_window);
            g_present_milli=frontend_fps_output(&g_fps_window);
            save_performance(perf_ms,perf_presents,now_events-perf_events,
                             ee->instructions_executed-perf_ee,perf_core,perf_blit);
            perf_start=perf_now;perf_presents=0;perf_events=now_events;
            perf_ee=ee->instructions_executed;perf_core=0;perf_blit=0;
        }
        if (!g_controller.pad.full_pad && (g_input_held & (PAD_BUTTON_B | PAD_TRIGGER_Z)) == (PAD_BUTTON_B | PAD_TRIGGER_Z)) { stopped_by_user = 1; break; }
        if (ee->halted && iop->halted) break;
        if (total_slices >= BOOT_TOTAL_CAP) break;
    }

    uint64_t final_ms=ticks_to_millisecs(gettime()-perf_start);
    if(final_ms)save_performance(final_ms,perf_presents,ee_core_get_vblank_events()-perf_events,
      ee->instructions_executed-perf_ee,perf_core,perf_blit);
    audio_output_stop();
    save_boot_progress(gx_fault_paused?"GX_FAULT_PAUSED":exit_requested?"EXIT_REQUEST":stopped_by_user?"PAUSED":"STOPPED");
    if(exit_requested)wii_exit_to_loader();
    if (stopped_by_user||gx_fault_paused) return;
    goto_rc(16, 400);
    if (stopped_by_user)
        printf("Stopped by user (B held).                                          \n");
    else if (ee->halted && iop->halted)
        printf("Both cores halted on their own - see reasons above.                \n");
    else
        printf("Reached the safety instruction cap - still executing real code.    \n");
    printf("\nPress A or B to open the launcher.\n");
    wait_for_button(PAD_BUTTON_A | PAD_BUTTON_B);
}

/* Not a hardware accuracy mode: explicitly opt-in, separate from 32-bit
 * CPU/GX option mask.  Saved to SD for use at the next cold boot. */

static void gs_compat_load(void)
{
    gekko2_experimental_load(GEKKO2_EXP_PSM,"sd:/pcsx2/gs-compat.cfg");
    gekko2_experimental_load(GEKKO2_EXP_CHAIN,"sd:/pcsx2/jit-chain.cfg");
    gekko2_experimental_load(GEKKO2_EXP_GX_RISK,"sd:/pcsx2/gx-risk.cfg");
}
static const char *experiment_toggle_save(unsigned n)
{
 static const char *paths[GEKKO2_EXP_COUNT]={"sd:/pcsx2/jit-chain.cfg","sd:/pcsx2/gs-compat.cfg","sd:/pcsx2/gx-risk.cfg"};
 if(n>=GEKKO2_EXP_COUNT||!gekko2_experimental_request(n,!gekko2_experimental_requested(n)))return "Experiment is not compiled into this build.";
 return gekko2_experimental_save(n,paths[n])==0?"Saved. Applies at next cold boot.":"Not saved. Check SD space / directory.";
}
static void frontend_preferences_load(void)
{
 frontend_settings s;frontend_settings_defaults(&s);
 if(g_fat_mounted)frontend_settings_load(&s,"sd:/pcsx2/frontend.cfg");
 g_hud_default=s.hud;g_throughput=s.throughput;g_fps_default=s.fps;g_gx_requested=s.gx;
}
static int frontend_preferences_save(void)
{
 frontend_settings s={(unsigned)g_hud_default,(unsigned)g_throughput,(unsigned)g_fps_default,(unsigned)g_gx_requested};
 return frontend_settings_save(&s,"sd:/pcsx2/frontend.cfg");
}
static int cold_boot_session(int disc)
{
#ifdef GEKKO2_ARM_BETA
 /* Launcher changes precede the first BIOS/disc handles and ARM requests.
  * A paused session requires an app restart; never reload its live IOS. */
 if(wii_arm_ios_prepare_first_boot(gekko2_opt_requested(),g_system_started,&g_fat_mounted)==-4)return -1;
#endif
 audio_output_stop();
 if(g_system_started)save_boot_progress("COLD_BOOT_REQUEST");
 /* Resolve old GX ownership before disc/RAM/core owners are replaced.
  * Failed resolution leaves the paused session available for inspection. */
 if(!gs_gx_reset_boot_state())return -1;
 if(g_system_started) {
  save_boot_progress("COLD_BOOT_END");
  iop_cdvd_unmount_iso();iop_cdrom_legacy_unmount_iso();
  ee_core_shutdown();iop_core_shutdown();
 }
 g_system_started=0;g_disc_checked=0;g_disc_ok=0;g_first_picture=0;
 r1252_evidence_written=0;g_boot_disc=disc;
 run_real_boot_flow();return 0;
}

/* A explicitly starts a new session; START only resumes existing cores. */
int main(int argc, char **argv)
{
    (void)argc;(void)argv;
    wii_console_setup();
    if (!g_fat_mounted) g_fat_mounted = fatInitDefault() ? 1 : 0;
    int opt_load_status=g_fat_mounted?gekko2_opt_load("sd:/pcsx2/optimization.cfg"):-1;
    if(g_fat_mounted)gs_compat_load();
    frontend_preferences_load();
    gekko2_opt_apply(); /* Startup must use loaded settings before IOS selection. */
#ifdef GEKKO2_ARM_BETA
    /* Both saved ARM and explicit IOS222 selection are required for reload.
     * Restart after changing ARM: never reload IOS inside a guest session. */
    int ios_result=wii_arm_ios_start((gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER)||gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM))&&gekko2_opt_enabled(GEKKO2_OPT_ARM_IOS222)?222:0,&g_fat_mounted);
    if(ios_result==-4){printf("IOS recovery failed. Restart Wii before using Gekko2.\n");return 1;}
#endif
    /* Persist a fresh identity before the first launcher draw. Previous
     * BIOS logs can survive a failed startup and must not identify this run. */
    {
        FILE *boot=fopen("sd:/pcsx2/Gekko2-startup.log","w");
        if(boot) {
            fprintf(boot,"BUILD checkpoint=" GEKKO2_BUILD_NAME " stage=launcher-ready resident_pipeline=%d\n",
#ifdef GEKKO2_GX_RESIDENT_PIPELINE_DISABLE
                0
#else
                1
#endif
            );
            fprintf(boot,"FRONTEND_OPTIONS hud=%d throughput=%d fps=%d gx_requested=%d gx_session=%d\n",g_hud_default,g_throughput,g_fps_default,g_gx_requested,g_gx_present);
            fprintf(boot,"OPTIONS load_status=%d requested=%08lx active=%08lx ios222=%d\n",opt_load_status,(unsigned long)gekko2_opt_requested(),(unsigned long)gekko2_optimization_mask,gekko2_opt_enabled(GEKKO2_OPT_ARM_IOS222));
            fprintf(boot,"EXPERIMENTAL_OPTIONS chain_compiled=%u chain_requested=%d chain_active=%d psm_requested=%d psm_active=%d apply=next-cold-boot\n",
                (gekko2_experimental_available()>>GEKKO2_EXP_CHAIN)&1u,
                gekko2_experimental_requested(GEKKO2_EXP_CHAIN),gekko2_experimental_enabled(GEKKO2_EXP_CHAIN),
                gekko2_experimental_requested(GEKKO2_EXP_PSM),gekko2_experimental_enabled(GEKKO2_EXP_PSM));
            fprintf(boot,"GX_RISK_MODE available=%u requested=%d enabled=%d apply=next-cold-boot\n",
                (gekko2_experimental_available()>>GEKKO2_EXP_GX_RISK)&1u,
                gekko2_experimental_requested(GEKKO2_EXP_GX_RISK),gekko2_experimental_enabled(GEKKO2_EXP_GX_RISK));
            fprintf(boot,"ARM_IOS status=%d original=%u active=%u\n",wii_arm_ios_status(),wii_arm_ios_original(),wii_arm_ios_active());
            fclose(boot);
        }
    }
    int selected=0,page=0,redraw=1,opt_selected=0,settings_selected=0,experiment_selected=0;
    const char *notice="SD: pcsx2/bios/ and pcsx2/games/";
#ifdef PCSX2WII_JIT_DISABLE
    const char *engine="INTERPRETER";
#else
    const char *engine="PPC JIT";
#endif
    for (;;) {
        if(redraw){ui_text_renderer=launcher_text;ui_logo_renderer=launcher_logo;
            if(page==4){
                ui_optimization_draw(launcher_rect,opt_selected,gekko2_opt_requested(),gekko2_opt_available(),notice);
            }
            else if(page==5)ui_experiments_draw(launcher_rect,experiment_selected,notice);
            else if(page==1)ui_settings_draw(launcher_rect,settings_selected,g_hud_default,g_throughput,g_fps_default,g_gx_requested,notice);
            else if(page==3)ui_browser_draw(launcher_rect,&g_browser,notice);
            else ui_draw(launcher_rect,selected,page,g_system_started,g_hud_default,g_throughput,g_fps_default,g_gx_requested,engine,notice);
            flush_screen();redraw=0;}
        VIDEO_WaitVSync();wii_input_scan();uint16_t down=g_input_down;
        if(g_input_exit || (g_input_home && page==0))wii_exit_to_loader();
        if(g_input_home){page=0;redraw=1;continue;}
        if(!down)continue;
        redraw=1;
        if(down&PAD_BUTTON_START) {
            if(g_system_started){page=0;run_real_boot_flow();notice=gs_gx_faulted()?"GX fence failed. Restart app from HBC; resume blocked.":"Paused. START resumes; COLD BOOT starts a new log.";}
            else notice="No paused session. Choose BIOS or DISC.";
            continue;
        }
        if(page==3){
            if(down&PAD_BUTTON_B){
                if(!strcmp(g_browser.path,g_browser.root)){page=0;notice=g_disc_path[0]?g_disc_notice:"No disc selected.";}
                else if(frontend_browser_up(&g_browser))notice="Could not open parent folder.";
            }else if((down&PAD_TRIGGER_L)&&g_browser.count)g_browser.selected=g_browser.selected>=8?g_browser.selected-8:0;
            else if((down&PAD_TRIGGER_R)&&g_browser.count)g_browser.selected=g_browser.selected+8<g_browser.count?g_browser.selected+8:g_browser.count-1;
            else if((down&PAD_BUTTON_UP)&&g_browser.count)g_browser.selected=(g_browser.selected+g_browser.count-1)%g_browser.count;
            else if((down&PAD_BUTTON_DOWN)&&g_browser.count)g_browser.selected=(g_browser.selected+1)%g_browser.count;
            else if(down&PAD_BUTTON_A){
                int rc=frontend_browser_activate(&g_browser,g_disc_path,sizeof(g_disc_path));
                if(rc==1){const char *name=strrchr(g_disc_path,'/');
                    snprintf(g_disc_notice,sizeof(g_disc_notice),"DISC: %.62s",name?name+1:g_disc_path);
                    notice=g_disc_notice;page=0;selected=UI_DISC;
                }else notice=rc<0?"Could not open selection.":"Choose an ISO / BIN / CHD file.";
            }
            continue;
        }
        if(page){
            if(page==4){
                if(down&PAD_BUTTON_B)page=1;
                else if(down&PAD_BUTTON_X)notice=experiment_toggle_save(GEKKO2_EXP_PSM);
                else if(down&PAD_BUTTON_Y)notice=experiment_toggle_save(GEKKO2_EXP_CHAIN);
                else if(down&PAD_BUTTON_UP)opt_selected=(opt_selected+GEKKO2_OPT_COUNT-1)%GEKKO2_OPT_COUNT;
                else if(down&PAD_BUTTON_DOWN)opt_selected=(opt_selected+1)%GEKKO2_OPT_COUNT;
                else if(down&(PAD_BUTTON_A|PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT)){
                    if(gekko2_opt_toggle((unsigned)opt_selected))
                        notice=gekko2_opt_save("sd:/pcsx2/optimization.cfg")==0?
                          (opt_selected==GEKKO2_OPT_ARM_WORKER||opt_selected==GEKKO2_OPT_ARM_IOS222||opt_selected==GEKKO2_OPT_ARM_CPU_RAM?"Saved. ARM / IOS needs app restart.":"Saved. Applies at next cold boot."):
                          "Not saved. Check SD space / directory.";
                    else notice="Backend is not compiled into this build.";
                }
            }else if(page==5){
                if(down&PAD_BUTTON_B)page=1;
                else if(down&PAD_BUTTON_UP)experiment_selected=(experiment_selected+GEKKO2_EXP_COUNT-1)%GEKKO2_EXP_COUNT;
                else if(down&PAD_BUTTON_DOWN)experiment_selected=(experiment_selected+1)%GEKKO2_EXP_COUNT;
                else if(down&(PAD_BUTTON_A|PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT))notice=experiment_toggle_save((unsigned)experiment_selected);
            }else if(page==1){
                if(down&PAD_BUTTON_B)page=0;
                else if(down&PAD_BUTTON_UP)settings_selected=(settings_selected+UI_SET_COUNT-1)%UI_SET_COUNT;
                else if(down&PAD_BUTTON_DOWN)settings_selected=(settings_selected+1)%UI_SET_COUNT;
                else if(down&(PAD_BUTTON_A|PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT)){
                    int rc=0;
                    switch(settings_selected){
                    case UI_SET_RENDERER:g_gx_requested=!g_gx_requested;rc=frontend_preferences_save();break;
                    case UI_SET_HUD:g_hud_default=!g_hud_default;rc=frontend_preferences_save();break;
                    case UI_SET_UPDATE:g_throughput=!g_throughput;rc=frontend_preferences_save();break;
                    case UI_SET_FPS:g_fps_default=!g_fps_default;rc=frontend_preferences_save();break;
                    case UI_SET_SOUND:gekko2_opt_toggle(GEKKO2_OPT_SOUND);rc=gekko2_opt_save("sd:/pcsx2/optimization.cfg");break;
                    case UI_SET_PROFILE:
                        gekko2_opt_request_profile(!strcmp(gekko2_opt_profile_name(),"WII BASE")?GEKKO2_PROFILE_INTERPRETER:GEKKO2_PROFILE_BASE);
                        rc=gekko2_opt_save("sd:/pcsx2/optimization.cfg");break;
                    case UI_SET_EXPERIMENTS:page=5;break;
                    case UI_SET_DEVELOPER:page=4;break;
                    case UI_SET_DEFAULTS:{
                        frontend_settings defaults;frontend_settings_defaults(&defaults);
                        g_hud_default=defaults.hud;g_throughput=defaults.throughput;g_fps_default=defaults.fps;g_gx_requested=defaults.gx;
                        gekko2_opt_reset_requested();rc=frontend_preferences_save();
                        if(gekko2_opt_save("sd:/pcsx2/optimization.cfg"))rc=-1;
                        const char *paths[GEKKO2_EXP_COUNT]={"sd:/pcsx2/jit-chain.cfg","sd:/pcsx2/gs-compat.cfg","sd:/pcsx2/gx-risk.cfg"};
                        for(unsigned n=0;n<GEKKO2_EXP_COUNT;n++){
                            gekko2_experimental_request(n,0);
                            if(gekko2_experimental_save(n,paths[n]))rc=-1;
                        }
                        break;}
                    }
                    notice=rc?"Some settings were not saved. Check SD.":"Saved. CPU / GX: cold boot; ARM / IOS: restart.";
                }
            }else if(down&PAD_BUTTON_B)page=0;

            continue;
        }
        if(down&(PAD_BUTTON_UP|PAD_BUTTON_LEFT))selected=(selected+UI_MENU_COUNT-1)%UI_MENU_COUNT;
        else if(down&(PAD_BUTTON_DOWN|PAD_BUTTON_RIGHT))selected=(selected+1)%UI_MENU_COUNT;
        else if(down&PAD_BUTTON_A){
            if(selected==UI_EXIT)wii_exit_to_loader();
            if(selected==UI_SETTINGS||selected==UI_ABOUT){page=selected==UI_SETTINGS?1:2;continue;}
            int disc=selected==UI_COLD_BOOT?g_boot_disc:selected==UI_DISC;
            if(selected==UI_SELECT_DISC||(disc&&!g_disc_path[0])) {
                if(!g_fat_mounted)g_fat_mounted=fatInitDefault()?1:0;
                if(!g_fat_mounted){notice="SD card unavailable.";continue;}
                frontend_browser_release(&g_browser);
                if(frontend_browser_init(&g_browser,"sd:/","sd:/pcsx2/games/")<0){notice="Could not open SD card.";continue;}
                page=3;notice="Choose an ISO / BIN / CHD file.";continue;
            }
            if(cold_boot_session(disc)<0){notice=gs_gx_faulted()?"GX fence failed. Restart app from HBC; resume blocked.":"Cold boot blocked: GX ownership could not resolve.";continue;}
            notice=g_session_log.errors?"Log write failed. Check SD space / access.":
              (disc&&g_system_started&&!g_disc_ok)?"Selected disc failed to mount. SELECT DISC to retry.":
              gs_gx_faulted()?"GX fence failed. Restart app from HBC; resume blocked.":g_system_started?"Paused. START resumes; COLD BOOT starts a new log.":"Boot failed. Check BIOS paths on SD.";
        }
    }
    return 0;
}
