#include "core/hw/rspu2_stream.h"
#include "core/runtime_profile.h"
#include "core/recompiler/dynarec_config.h"
#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/ppc_dynarec.h"
#include "core/hw/frontend_text.h"
#include "core/hw/frontend_logo.h"
#include "core/hw/frontend_runtime.h"
#include "core/hw/frontend_log.h"
#include "core/recompiler/iop_jit.h"
#include "core/recompiler/vu_jit.h"
/* R1308: native Wii launcher followed by real EE/IOP boot. */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "frontend.h"
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogc/lwp_watchdog.h>

#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/recompiler/ee_jit.h"
#include "core/iop/iop_core.h"
#include "core/system.h"
#include "core/bios_loader.h"
#include "core/hw/cdvd_config.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include "core/hw/gs.h"
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

/* Round 272 (task #423, 313th finding): translate the real Wii
 * GameCube-style controller's held-button mask (libogc's real
 * PAD_BUTTON_ / PAD_TRIGGER_ bits, from PAD_ButtonsHeld()) into a
 * real PS2 digital-pad pressed-mask (IOP_PAD_BTN_*, psx-spx-cited,
 * already defined in iop_sio2.h) and feed it to this project's own
 * already-implemented, real host-side pad API
 * (iop_sio2_pad_set_buttons(), Round 184/195).
 *
 * This is a port-level control-mapping DESIGN CHOICE, not a claim
 * about real PS2 hardware behavior - there is no "real" Wii-to-PS2
 * button mapping to cite, since the Wii never shipped a PS2 pad
 * pass-through. The mapping below follows the common-sense
 * face-button correspondence used by most emulators/ports that
 * support both pads (Wii A/B/X/Y sit in the same physical ring
 * position as PS2 Cross/Circle/Square/Triangle when both pads are
 * held the same way), and is documented here explicitly as a design
 * choice so it is never confused with a cited hardware fact:
 *   Wii A      -> PS2 Cross    (both are the primary "confirm" button)
 *   Wii B      -> PS2 Circle
 *   Wii X      -> PS2 Square
 *   Wii Y      -> PS2 Triangle
 *   Wii Start  -> PS2 Start
 *   Wii D-pad  -> PS2 D-pad (direct correspondence)
 *   Wii L/R    -> PS2 L1/R1
 *   Wii Z      -> PS2 Select
 *
 * Round 272 also tested (host-native scratch diagnostic, not shipped
 * here) whether simply holding Cross from boot start unblocks
 * OSDSYS's idle loop or triggers any CD-ROM auto-boot activity - it
 * does not (0 SIO2 register writes either way, identical trace to the
 * no-press baseline - see docs/STATUS.md's 313th finding for the
 * full account). This wiring is shipped anyway because it is a real,
 * correct, useful feature for actual interactive use on real Wii
 * hardware once further boot progress is made - not because it was
 * found to unblock anything on its own. */
static uint16_t wii_pad_to_ps2_pad(uint16_t wii_held)
{
    uint16_t ps2 = 0;
    if (wii_held & PAD_BUTTON_A)     ps2 |= IOP_PAD_BTN_CROSS;
    if (wii_held & PAD_BUTTON_B)     ps2 |= IOP_PAD_BTN_CIRCLE;
    if (wii_held & PAD_BUTTON_X)     ps2 |= IOP_PAD_BTN_SQUARE;
    if (wii_held & PAD_BUTTON_Y)     ps2 |= IOP_PAD_BTN_TRIANGLE;
    if (wii_held & PAD_BUTTON_START) ps2 |= IOP_PAD_BTN_START;
    if (wii_held & PAD_BUTTON_UP)    ps2 |= IOP_PAD_BTN_UP;
    if (wii_held & PAD_BUTTON_DOWN)  ps2 |= IOP_PAD_BTN_DOWN;
    if (wii_held & PAD_BUTTON_LEFT)  ps2 |= IOP_PAD_BTN_LEFT;
    if (wii_held & PAD_BUTTON_RIGHT) ps2 |= IOP_PAD_BTN_RIGHT;
    if (wii_held & PAD_TRIGGER_L)    ps2 |= IOP_PAD_BTN_L1;
    if (wii_held & PAD_TRIGGER_R)    ps2 |= IOP_PAD_BTN_R1;
    if (wii_held & PAD_TRIGGER_Z)    ps2 |= IOP_PAD_BTN_SELECT;
    return ps2;
}

/* R1308: one build supports GameCube pads and upright Wii Remotes,
 * with optional Nunchuk. Expansion bits are accepted only for Nunchuk,
 * so another expansion cannot accidentally become a shoulder button. */
__attribute__((noinline)) uint16_t wii_remote_to_pad(uint32_t buttons,
                                                   int nunchuk, int sx, int sy)
{
    uint16_t pad=0;
    if(buttons&WPAD_BUTTON_A)pad|=PAD_BUTTON_A;
    if(buttons&WPAD_BUTTON_B)pad|=PAD_BUTTON_B;
    if(buttons&WPAD_BUTTON_1)pad|=PAD_BUTTON_X;
    if(buttons&WPAD_BUTTON_2)pad|=PAD_BUTTON_Y;
    if(buttons&WPAD_BUTTON_PLUS)pad|=PAD_BUTTON_START;
    if(buttons&WPAD_BUTTON_MINUS)pad|=PAD_TRIGGER_Z;
    if(buttons&WPAD_BUTTON_UP)pad|=PAD_BUTTON_UP;
    if(buttons&WPAD_BUTTON_DOWN)pad|=PAD_BUTTON_DOWN;
    if(buttons&WPAD_BUTTON_LEFT)pad|=PAD_BUTTON_LEFT;
    if(buttons&WPAD_BUTTON_RIGHT)pad|=PAD_BUTTON_RIGHT;
    if(nunchuk){
        if(buttons&WPAD_NUNCHUK_BUTTON_C)pad|=PAD_TRIGGER_L;
        if(buttons&WPAD_NUNCHUK_BUTTON_Z)pad|=PAD_TRIGGER_R;
        if(sx<0)pad|=PAD_BUTTON_LEFT;
        if(sx>0)pad|=PAD_BUTTON_RIGHT;
        if(sy<0)pad|=PAD_BUTTON_DOWN;
        if(sy>0)pad|=PAD_BUTTON_UP;
    }
    return pad;
}
/* Calibrated dead zone: 35% of each half-axis; invalid calibration is
 * neutral, rather than treating missing extension data as full deflection. */
__attribute__((noinline)) int wii_stick_direction(int pos,int lo,int center,int hi)
{
    if(lo>=center || center>=hi)return 0;
    int delta=pos-center;
    if(delta>0 && delta*100>(hi-center)*35)return 1;
    if(delta<0 && -delta*100>(center-lo)*35)return -1;
    return 0;
}
static uint16_t g_input_held,g_input_down,g_remote_previous;
/* START/PLUS pauses; Z/MINUS+A sends guest START; Z+START toggles HUD. */
__attribute__((noinline)) unsigned wii_session_controls(uint16_t held,uint16_t down,uint16_t *guest)
{
 int hud=(held&(PAD_BUTTON_START|PAD_TRIGGER_Z))==(PAD_BUTTON_START|PAD_TRIGGER_Z);
 int guest_start=(held&(PAD_BUTTON_A|PAD_TRIGGER_Z))==(PAD_BUTTON_A|PAD_TRIGGER_Z);
 uint16_t result=held&~PAD_BUTTON_START;
 if(hud)result&=~PAD_TRIGGER_Z;
 if(guest_start)result=(result&~(PAD_BUTTON_A|PAD_TRIGGER_Z))|PAD_BUTTON_START;
 if(guest)*guest=result;
 if(hud&&(down&(PAD_BUTTON_START|PAD_TRIGGER_Z)))return 2u;
 return (down&PAD_BUTTON_START)&&!hud?1u:0u;
}
static int g_input_home,g_input_exit;
static int g_remote_error=WPAD_ERR_NOT_READY;
static void wii_input_scan(void)
{
    PAD_ScanPads();WPAD_ScanPads();
    WPADData *data=WPAD_Data(WPAD_CHAN_0);
    uint16_t remote=0;
    g_input_home=0;g_input_exit=0;
    g_remote_error=data?data->err:WPAD_ERR_NO_CONTROLLER;
    if(data && data->err==WPAD_ERR_NONE){
        int nunchuk=data->exp.type==WPAD_EXP_NUNCHUK,sx=0,sy=0;
        if(nunchuk){
            const struct joystick_t *js=&data->exp.nunchuk.js;
            sx=wii_stick_direction(js->pos.x,js->min.x,js->center.x,js->max.x);
            sy=wii_stick_direction(js->pos.y,js->min.y,js->center.y,js->max.y);
        }
        remote=wii_remote_to_pad(data->btns_h,nunchuk,sx,sy);
        g_input_home=(data->btns_d&WPAD_BUTTON_HOME)!=0;
        g_input_exit=(data->btns_h&(WPAD_BUTTON_HOME|WPAD_BUTTON_MINUS))==(WPAD_BUTTON_HOME|WPAD_BUTTON_MINUS);
    }
    g_input_held=PAD_ButtonsHeld(0)|remote;
    g_input_down=PAD_ButtonsDown(0)|(remote&~g_remote_previous);
    g_remote_previous=remote;
    if((g_input_held&(PAD_BUTTON_B|PAD_TRIGGER_Z|PAD_BUTTON_START))==(PAD_BUTTON_B|PAD_TRIGGER_Z|PAD_BUTTON_START))g_input_exit=1;
}

static void wii_console_setup(void)
{
    VIDEO_Init();
    PAD_Init();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC);
    /* libogc 1.8.18 compares idle >= timeout without a zero-disable
     * guard. Zero disconnected at the next tick. UINT32_MAX is about
     * 136 years, effectively no idle disconnect for an emulator session. */
    WPAD_SetIdleTimeout(UINT32_MAX);

    rmode = VIDEO_GetPreferredMode(NULL);
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
    frontend_log_close(&g_session_log,f);g_log_ticks+=gettime()-log_begin;
}

/* HBC supplies the loader return stub used by standard exit(0).
 * SYS_RETURNTOMENU would go to the Wii System Menu instead. */
static void wii_exit_to_loader(void)
{
    if(g_system_started)save_boot_progress("EXIT_HBC");
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
    for(unsigned n=0;n<2;n++)fprintf(f,"BLOCK_CACHE cpu=%s ways=%u lookups=%llu hits=%llu misses=%llu collisions=%llu stale=%llu attempts=%llu installed=%llu failures=%llu compile_sample_tb=%llu compile_samples=%llu sample_stride=64\n",
      n?"IOP":"EE",gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)?4u:1u,
      (unsigned long long)caches[n].lookups,(unsigned long long)caches[n].hits,(unsigned long long)caches[n].misses,
      (unsigned long long)caches[n].collisions,(unsigned long long)caches[n].stale,(unsigned long long)caches[n].attempts,
      (unsigned long long)caches[n].installed,(unsigned long long)caches[n].failures,
      (unsigned long long)caches[n].compile_tb,(unsigned long long)caches[n].compile_samples);
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
    fprintf(f,"ARM_WORKER probes=%llu submitted=%llu completed=%llu errors=%llu stale=%llu enabled=%u available=%d\n",
      (unsigned long long)arm_worker_stat(0),(unsigned long long)arm_worker_stat(1),
      (unsigned long long)arm_worker_stat(2),(unsigned long long)arm_worker_stat(3),(unsigned long long)arm_worker_stat(4),gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER),arm_worker_available());
    fprintf(f,"ARM_LOADER status=%d path=sd:/pcsx2/arm/Gekko2-ARM-Worker.elf\n",arm_loader_status());
    fprintf(f,"ARM_LOAD_DETAIL bytes=%lu base=%08lx capacity=%08lx entry=%08lx crc32=%08lx validation=%lu\n",(unsigned long)arm_loader_stat(0),(unsigned long)arm_loader_stat(1),(unsigned long)arm_loader_stat(2),(unsigned long)arm_loader_stat(3),(unsigned long)arm_loader_stat(4),(unsigned long)arm_loader_stat(5));
    fprintf(f,"ARM_TRANSFER operation=%lu result=%ld address=%08lx length=%lu\n",(unsigned long)arm_loader_stat(6),(long)(int32_t)arm_loader_stat(7),(unsigned long)arm_loader_stat(8),(unsigned long)arm_loader_stat(9));
    fprintf(f,"PAD_INPUT host_held=%04x guest_pressed=%04x samples=%lu serial_commands=%lu remote_error=%d\n",g_input_held,iop_sio2_pad_get_buttons(),(unsigned long)iop_sio2_pad_sample_count(),(unsigned long)iop_sio2_get_pad_command_count(),g_remote_error);
    {const rspu2_stream_state_t *stream=rspu2_stream_get_state();
     fprintf(f,"RSPU2_STREAM init=%lu cmd=%04lx status=%02lx reads=%lu sectors=%lu remaining=%lu failures=%lu audio_sectors=%lu\n",
       (unsigned long)stream->initialized,(unsigned long)stream->last_command,(unsigned long)rspu2_stream_status(),
       (unsigned long)stream->reads,(unsigned long)stream->transferred_sectors,(unsigned long)stream->remaining_sectors,(unsigned long)stream->failures,(unsigned long)stream->audio_sectors_read);}
    fprintf(f,"ARM_IOS status=%d original=%u active=%u\n",wii_arm_ios_status(),wii_arm_ios_original(),wii_arm_ios_active());
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
    fprintf(f,"HOST budget=%lu remote_err=%d fps_overlay=%d gx_output=%d EE_PC=%08lx IOP_PC=%08lx\n",(unsigned long)g_slice_budget,g_remote_error,g_fps_default,g_gx_present,(unsigned long)ee_core_get_state()->pc,(unsigned long)iop_core_get_state()->pc);
    ee_state_t *ee_runtime=ee_core_get_state();iop_state_t *iop_runtime=iop_core_get_state();
    fprintf(f,"CORE_WAIT EE_idle=%u EE_halted=%u EE_Count=%08lx EE_retired=%llu IOP_idle=%u IOP_halted=%u IOP_ticks=%llu\n",
        (unsigned)ee_runtime->idle,(unsigned)ee_runtime->halted,(unsigned long)ee_runtime->cop0[9],
        (unsigned long long)ee_runtime->instructions_executed,(unsigned)iop_runtime->idle,
        (unsigned)iop_runtime->halted,(unsigned long long)iop_runtime->sched_ticks);
    const gs_state_t *scanout=gs_get_state();const gif_state_t *draw_target=gif_get_state();
    uint64_t scanout_fb=(scanout->pmode&1u)?scanout->dispfb1:scanout->dispfb2;
    uint64_t scanout_display=(scanout->pmode&1u)?scanout->display1:scanout->display2;
    uint32_t scan_bp,scan_bw,scan_x,scan_y,scan_w,scan_h;
    gs_decode_dispfb(scanout_fb,&scan_bp,&scan_bw);
    gs_decode_display_region(scanout_fb,scanout_display,scanout->smode2,&scan_x,&scan_y,&scan_w,&scan_h);
    fprintf(f,"GS_SCANOUT PMODE=%llx DISPFB=%llx DISPLAY=%llx bp=%lu bw=%lu origin=%lu,%lu size=%lux%lu image_probes=%llu draw_bp=%lu draw_bw=%lu draw_psm=%lu\n",
        (unsigned long long)scanout->pmode,(unsigned long long)scanout_fb,(unsigned long long)scanout_display,
        (unsigned long)scan_bp,(unsigned long)scan_bw,(unsigned long)scan_x,(unsigned long)scan_y,
        (unsigned long)scan_w,(unsigned long)scan_h,(unsigned long long)g_image_probe_attempts,
        (unsigned long)draw_target->fbp,(unsigned long)draw_target->fbw,(unsigned long)draw_target->frame_psm);
    fprintf(f,"GX requested=%d ready=%d primitives_requested=%d primitives_active=%d first=%d hud=%d attempts=%llu fallbacks=%llu pending=%lu sync_errors=%lu log=%s\n",
       g_gx_present,gs_gx_ready(),g_gx_present,
       frontend_allow_gx_primitives(g_gx_present,g_first_picture)&&gs_gx_ready(),
       g_first_picture,g_hud_active,(unsigned long long)g_gx_attempts,(unsigned long long)g_gx_fallbacks,
       (unsigned long)gs_mem_gpu_pending(),(unsigned long)gs_mem_sync_failures(),boot_log_path());
    fprintf(f,"GX_WORK draws=%llu quads=%llu readback_bytes=%llu resolve_waits=%llu attempts=%llu\n",
            (unsigned long long)gs_gx_work_count(0),(unsigned long long)gs_gx_work_count(1),
            (unsigned long long)gs_gx_work_count(2),(unsigned long long)gs_gx_work_count(3),
            (unsigned long long)gs_gx_work_count(4));
        fprintf(f,"GX_PIPELINE depth_hw=%llu depth_hybrid=%llu blend_hw=%llu blend_hybrid=%llu z32_split=%llu pabe_split=%llu\n",
        (unsigned long long)gs_gx_pipeline_count(0),(unsigned long long)gs_gx_pipeline_count(1),
        (unsigned long long)gs_gx_pipeline_count(2),(unsigned long long)gs_gx_pipeline_count(3),
        (unsigned long long)gs_gx_pipeline_count(4),(unsigned long long)gs_gx_pipeline_count(5));
    fprintf(f,"GX_SURFACE opens=%llu draws=%llu resolves=%llu snapshots=%llu presents=%llu seed_bytes=%llu\n",
        (unsigned long long)gs_gx_surface_count(0),(unsigned long long)gs_gx_surface_count(1),
        (unsigned long long)gs_gx_surface_count(2),(unsigned long long)gs_gx_surface_count(3),
        (unsigned long long)gs_gx_surface_count(4),(unsigned long long)gs_gx_surface_count(5));
    fprintf(f,"GX_SOURCE_CACHE hits=%llu misses=%llu decoded_bytes=%llu\n",
        (unsigned long long)gs_gx_source_cache_count(0),(unsigned long long)gs_gx_source_cache_count(1),
        (unsigned long long)gs_gx_source_cache_count(2));
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
    const gif_state_t *gif=gif_get_state();
    fprintf(f,"GS_STATE frame_psm=%lu prim=%lx prmode=%lx ac=%lu zcfg=%lu ate=%lu fbmask=%lx triangles=%llu sprites=%llu\n",
        (unsigned long)gif->frame_psm,(unsigned long)gif->prim,(unsigned long)gif->prmode,
        (unsigned long)gif->prmodecont_ac,(unsigned long)gif->zbuf_configured,(unsigned long)gif->ate,
        (unsigned long)gif->fbmask,(unsigned long long)gif->triangles_drawn,(unsigned long long)gif->sprites_drawn);
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
    fprintf(f,"BUILD checkpoint=R1341 scope=EE-cache-IPU-video-ARM-client mask=%08lx next_boot_mask=%08lx resident_pipeline=%d EE_blocks=%d scheduler_quanta=%d\n",
        (unsigned long)gekko2_optimization_mask,(unsigned long)gekko2_opt_requested(),
        gekko2_opt_enabled(GEKKO2_OPT_GX_RESIDENT),
        gekko2_opt_enabled(GEKKO2_OPT_EE_JIT)&&gekko2_opt_enabled(GEKKO2_OPT_EE_BLOCKS),
        GEKKO2_SCHEDULER_QUANTA_ENABLED&&gekko2_opt_enabled(GEKKO2_OPT_EE_JIT)&&gekko2_opt_enabled(GEKKO2_OPT_EE_BLOCKS));
    fprintf(f,"CODE_ARENA enabled=%d capacity=%lu used=%lu high_water=%lu failures=%llu\n",
        gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA),(unsigned long)ppc_code_cache_capacity(),(unsigned long)ppc_code_cache_used(),
        (unsigned long)ppc_code_cache_high_water(),(unsigned long long)ppc_code_cache_failures());
    fprintf(f,"FASTMEM enabled=%d ram_page_hits=%llu slow_translations=%llu invalidations=%llu hardware_MMU=0\n",
        gekko2_opt_enabled(GEKKO2_OPT_FASTMEM),(unsigned long long)ee_fastmem_stat(0),
        (unsigned long long)ee_fastmem_stat(1),(unsigned long long)ee_fastmem_stat(2));
    fprintf(f,"OPT_ROUTES EE=%d IOP=%d VU=%d links=%d residency=%d word_alloc=%d GX_source_cache=%d GX_Gouraud=%d GX_disjoint_writes=%d\n",
        gekko2_opt_enabled(GEKKO2_OPT_EE_JIT),gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT),gekko2_opt_enabled(GEKKO2_OPT_VU_JIT),
        gekko2_opt_enabled(GEKKO2_OPT_NATIVE_LINKS),gekko2_opt_enabled(GEKKO2_OPT_RESIDENCY),gekko2_opt_enabled(GEKKO2_OPT_WORD_ALLOCATION),
        gekko2_opt_enabled(GEKKO2_OPT_GX_SOURCE_CACHE),gekko2_opt_enabled(GEKKO2_OPT_GX_GOURAUD),gekko2_opt_enabled(GEKKO2_OPT_GX_DISJOINT_WRITES));
    fprintf(f,"GX_SYNC reuse=%llu readback=%llu present=%llu reuse_tb=%llu readback_tb=%llu present_tb=%llu\n",
      (unsigned long long)gs_gx_sync_count(0),(unsigned long long)gs_gx_sync_count(1),(unsigned long long)gs_gx_sync_count(2),
      (unsigned long long)gs_gx_sync_count(4),(unsigned long long)gs_gx_sync_count(5),(unsigned long long)gs_gx_sync_count(6));
    fprintf(f,"EE_BLOCK runs=%llu retired=%llu native_successors=%llu dispatch_hits=%llu\n",(unsigned long long)ee_jit_get_block_count(),(unsigned long long)ee_jit_get_block_retired(),(unsigned long long)ee_jit_get_native_successors(),(unsigned long long)ee_jit_get_dispatch_hits());
#ifdef GEKKO2_COUNT_BOUNDARIES
    fprintf(f,"EE_BOUNDARY fused_mod32=%llu\n",(unsigned long long)ee_core_get_fused_boundaries());
#endif
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
    FILE *fp=fopen("sd:/pcsx2/Gekko2-R1308-first-fault.txt", "w");
    if (!fp) return;
    fprintf(fp,"Gekko2 R1308 JIT=%s disc=%d BIOS=%s version=%s size=%u\n",
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
            printf("R1308 MENU / RPC-GS JIT=%s REND=%llu hh=%llu S5=%ld/%ld w=%ld | slot2=%lu writer=%lu\033[K\n",jm,(unsigned long long)re,(unsigned long long)hh,(long)sc,(long)sm,(long)sw,(unsigned long)hit,(unsigned long)wh);
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
    printf("\nSTART/PLUS: pause | MINUS+A: PS2 START | Z+START: HUD\n");
    printf("LOG session=%lu errors=%lu errno=%d\033[K\n",(unsigned long)g_session_log.sequence,
      (unsigned long)g_session_log.errors,g_session_log.last_error);
}

/* The real boot flow itself - see the top-of-file header comment and
 * the doc comment above draw_boot_progress_hud() for the full design.
 * Runs automatically once at startup (called from main()) and is also
 * reachable again from the launcher ("Re-run Boot Flow"). */
static void run_real_boot_flow(void)
{
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
        g_gx_present=g_gx_requested;
        g_boot_started=gettime();g_boot_log_next=0;
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
        gs_init();
        gs_mem_init();
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
        printf("[R1308] BIOS mode: no disc mounted.\n");
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
    int stopped_by_user = 0,exit_requested=0;
    int show_hud = g_hud_default;g_hud_active=show_hud; /* Z+START toggles diagnosis; START alone belongs to the PS2. */

    iop_sio2_pad_connect();
    frontend_pad_latch pad_latch={iop_sio2_pad_sample_count(),0,0};
    uint64_t perf_start=gettime(),perf_presents=0,perf_events=ee_core_get_vblank_events();
    uint64_t perf_ee=ee->instructions_executed,perf_core=0,perf_blit=0;

    for (;;) {
        wii_input_scan();
        if(g_input_exit){exit_requested=1;stopped_by_user=1;break;}
        if(g_input_home){stopped_by_user=1;break;}
        uint16_t held = g_input_held;
        uint16_t down = g_input_down;
        uint16_t guest_held=0;unsigned controls=wii_session_controls(held,down,&guest_held);
        if(controls==1u){stopped_by_user=1;break;}
        if(controls==2u){show_hud=!show_hud;g_hud_active=show_hud;last_present_ms=0;}
        iop_sio2_pad_set_buttons(frontend_pad_latch_update(&pad_latch,wii_pad_to_ps2_pad(guest_held),iop_sio2_pad_sample_count()));

        uint64_t stage_started=gettime();
        if (!(ee->halted && iop->halted)) {
            system_run_interleaved(g_slice_budget);
            total_slices += g_slice_budget;
        }

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
            if(probe_bw && gs_display_has_rgb(probe_bp,probe_bw,sx,sy,sw,sh)) {
                g_first_picture=1;save_boot_progress("FIRST_IMAGE");
            }
        }
        gs_gx_set_render_enabled(frontend_allow_gx_primitives(g_gx_present,g_first_picture));
        if(present_due)VIDEO_WaitVSync();
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
                    gx_ok=gs_gx_present(xfb,rmode,bp_words,bw_pixels,sx,sy,sw,sh);
                    if(!gx_ok)g_gx_fallbacks++;
                }
                if(g_first_picture && !gx_ok)
                    gs_blit_scaled_psmct32_to_xfb(xfb, rmode->fbWidth, blit_h,
                                             bp_words, bw_pixels, sx, sy, sw, sh);

            }
        }

        if(present_due){
        if (show_hud || !display_active || !g_first_picture) {
        draw_boot_progress_hud(ee->instructions_executed, iop->instructions_executed,
                                ee->halted, iop->halted, ee->halt_reason, iop->halt_reason,
                                display_active);
        if(!g_first_picture)printf("Waiting for BIOS pixels in VRAM.\033[K\n");
        else printf("BIOS pixels ready. Hide HUD with MINUS+PLUS / Z+START.\033[K\n");
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
        if ((held & (PAD_BUTTON_B | PAD_TRIGGER_Z)) == (PAD_BUTTON_B | PAD_TRIGGER_Z)) { stopped_by_user = 1; break; }
        if (ee->halted && iop->halted) break;
        if (total_slices >= BOOT_TOTAL_CAP) break;
    }

    uint64_t final_ms=ticks_to_millisecs(gettime()-perf_start);
    if(final_ms)save_performance(final_ms,perf_presents,ee_core_get_vblank_events()-perf_events,
      ee->instructions_executed-perf_ee,perf_core,perf_blit);
    save_boot_progress(exit_requested?"EXIT_REQUEST":stopped_by_user?"PAUSED":"STOPPED");
    if(exit_requested)wii_exit_to_loader();
    if (stopped_by_user) return;
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

static int cold_boot_session(int disc)
{
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
    gekko2_opt_apply(); /* Startup must use loaded settings before IOS selection. */
#ifdef GEKKO2_ARM_BETA
    /* Both saved ARM and explicit IOS222 selection are required for reload.
     * Restart after changing ARM: never reload IOS inside a guest session. */
    int ios_result=wii_arm_ios_start(gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER)&&gekko2_opt_enabled(GEKKO2_OPT_ARM_IOS222)?222:0,&g_fat_mounted);
    if(ios_result==-4){printf("IOS recovery failed. Restart Wii before using Gekko2.\n");return 1;}
#endif
    /* Persist a fresh identity before the first launcher draw. Previous
     * BIOS logs can survive a failed startup and must not identify this run. */
    {
        FILE *boot=fopen("sd:/pcsx2/Gekko2-startup.log","w");
        if(boot) {
            fprintf(boot,"BUILD checkpoint=R1341 stage=launcher-ready resident_pipeline=%d\n",
#ifdef GEKKO2_GX_RESIDENT_PIPELINE_DISABLE
                0
#else
                1
#endif
            );
            fprintf(boot,"OPTIONS load_status=%d requested=%08lx active=%08lx ios222=%d\n",opt_load_status,(unsigned long)gekko2_opt_requested(),(unsigned long)gekko2_optimization_mask,gekko2_opt_enabled(GEKKO2_OPT_ARM_IOS222));
            fprintf(boot,"ARM_IOS status=%d original=%u active=%u\n",wii_arm_ios_status(),wii_arm_ios_original(),wii_arm_ios_active());
            fclose(boot);
        }
    }
    int selected=0,page=0,redraw=1,opt_selected=0;
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
                char ios_line[90];snprintf(ios_line,sizeof ios_line,"Current IOS: %u | %s",wii_arm_ios_active(),wii_arm_ios_message());
                ui_text(launcher_rect,36,343,1,ios_line,117,186,237);
            }
            else if(page==3)ui_browser_draw(launcher_rect,&g_browser,notice);
            else ui_draw(launcher_rect,selected,page,g_system_started,g_hud_default,g_throughput,g_fps_default,g_gx_requested,engine,notice);
            flush_screen();redraw=0;}
        VIDEO_WaitVSync();wii_input_scan();uint16_t down=g_input_down;
        if(g_input_exit || (g_input_home && page==0))wii_exit_to_loader();
        if(g_input_home){page=0;redraw=1;continue;}
        if(!down)continue;
        redraw=1;
        if(down&PAD_BUTTON_START) {
            if(g_system_started){page=0;run_real_boot_flow();notice="Paused. START resumes; COLD BOOT starts a new log.";}
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
                }else notice=rc<0?"Could not open selection.":"Choose an ISO / BIN file.";
            }
            continue;
        }
        if(page){
            if(page==4){
                if(down&PAD_BUTTON_B)page=1;
                else if(down&PAD_BUTTON_UP)opt_selected=(opt_selected+GEKKO2_OPT_COUNT-1)%GEKKO2_OPT_COUNT;
                else if(down&PAD_BUTTON_DOWN)opt_selected=(opt_selected+1)%GEKKO2_OPT_COUNT;
                else if(down&(PAD_BUTTON_A|PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT)){
                    if(gekko2_opt_toggle(opt_selected))
                        notice=gekko2_opt_save("sd:/pcsx2/optimization.cfg")==0?
                            (opt_selected==GEKKO2_OPT_ARM_WORKER||opt_selected==GEKKO2_OPT_ARM_IOS222?"Saved. Exit to HBC and restart for ARM / IOS.":"Saved. Applies to the next cold boot."):
                            "Could not save options to SD. Check card / directory.";
                    else notice="This option is unavailable in this build.";
                }
                continue;
            }
            if(down&PAD_BUTTON_B)page=0;
            else if(page==1){if(down&PAD_BUTTON_A)g_hud_default=!g_hud_default;if(down&PAD_BUTTON_X)g_throughput=!g_throughput;if(down&PAD_BUTTON_Y)g_fps_default=!g_fps_default;
                if(down&PAD_BUTTON_DOWN){page=4;notice="A / LEFT / RIGHT: toggle. Next cold boot.";continue;}
                if(down&(PAD_BUTTON_RIGHT|PAD_BUTTON_LEFT)){g_gx_requested=!g_gx_requested;
                    notice=g_gx_requested?"Next cold boot: GX ON.":"Next cold boot: GX OFF.";}}

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
                page=3;notice="Choose an ISO / BIN file.";continue;
            }
            if(cold_boot_session(disc)<0){notice="Cold boot blocked: GX ownership could not resolve.";continue;}
            notice=g_session_log.errors?"Log write failed. Check SD space / access.":
              (disc&&g_system_started&&!g_disc_ok)?"Selected disc failed to mount. SELECT DISC to retry.":
              g_system_started?"Paused. START resumes; COLD BOOT starts a new log.":"Boot failed. Check BIOS paths on SD.";
        }
    }
    return 0;
}
