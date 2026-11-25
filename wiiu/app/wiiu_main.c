/* GPL-3.0-or-later. Wii U (wut) front end for the portable Gekko2 run loop. Test port only:
 * interpreter, software framebuffer through OSScreen, GamePad input, no audio, no GX2.
 * Looks for the user's own BIOS at <sd>/wiiu/gekko2/bios/ps2bios.bin. Nothing is bundled. */
#include "g2_app.h"
#include "wiiu_menu.h"
#include "wiiu_text.h"
#include "gx2_present.h"
#include "gx2_gs.h"
#include "core/hw/iop_sio2.h"
#include <coreinit/title.h>
#include <coreinit/cache.h>
#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/mutex.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TV_W 1280u
#define TV_H 720u
#define LINES 16
#define TV_LOG_LINES 3u
#define TV_LOG_FIRST_ROW 26u
#define DRC_LOG_FIRST_ROW 15u /* GamePad text rows 15..17 */
#define PIC_MAX_W 1280u
#define PIC_MAX_H 624u /* rows 0..25 of the TV text grid; the log uses the last three rows */
#define LINE_CHARS 100

static char lines[LINES][LINE_CHARS];
static unsigned line_count;
static uint32_t shown[1024u * 1024u];
static unsigned shown_w, shown_h;
static void *screen_memory;
static uint32_t tv_size, drc_size;

/* Threaded mode: the emulator runs on its own thread; this main thread (core 1) keeps ProcUI, controllers, the
 * screen and GX2. JIT build: always (emulator on the codegen core 0). Normal build: with the GPU renderer and
 * "GS core: own" (emulator on core 2, the GS worker runs on this thread, which owns GX2). Shared state is guarded
 * by one mutex. */
static OSMutex shared_lock;
static int threaded;          /* the emulator runs on its own thread during this session */
static int gs_on_main;        /* the GS worker runs on this main thread (GX2 core) */
static volatile int stop_requested, emu_done;
static volatile uint16_t shared_pressed;
static volatile uint8_t shared_axes[4] = { 0x80, 0x80, 0x80, 0x80 };
static volatile int shared_pad_ok;
#define SHARED_LOCK() OSLockMutex(&shared_lock)
#define SHARED_UNLOCK() OSUnlockMutex(&shared_lock)

static int dirty = 1;
static uint64_t last_render_ms;
static volatile int return_to_menu; /* hold L+R+ZL+ZR and press + while emulating */

static int use_gx2, use_gx2_gs, gx2_gs_available;

/* GPU GS renderer (menu switch "-"): GX2 draws GS triangles/sprites. Started lazily on the first game start with
 * the switch on, so the menu costs no GPU memory when it is off. */
static int gx2_gs_start(void)
{
#if defined(GEKKO2_WIIU_GX2) && __has_include("gs_color_gsh.h") && __has_include("gs_mod_gsh.h") && __has_include("gs_zload_gsh.h")
#include "gs_color_gsh.h"
#include "gs_mod_gsh.h"
#include "gs_zload_gsh.h"
    if (use_gx2_gs || !gx2_gs_available)
        return use_gx2_gs;
    use_gx2_gs = gx2_gs_init(gs_color_gsh_data, gs_mod_gsh_data, 1024u, 1024u);
    if (use_gx2_gs)
        gx2_gs_init_depth(gs_zload_gsh_data); /* without depth, Z draws stay in software */
    else
        gx2_gs_available = 0;
    return use_gx2_gs;
#else
    return 0;
#endif
}
static int show_log; /* status lines over the picture: only with the verbose log switched on */

/* ProcUI (Aroma HOME menu): while the app is in the background nothing may be drawn, and after it comes back the
 * OSScreen buffers have to be set again. WHBProcIsRunning() shuts ProcUI down every time it returns false, so its
 * result is latched and it is never called again after the exit request. */
static volatile int in_foreground = 1;
static int osscreen_up; /* OSScreen initialised and owning the display */
static int app_alive = 1;
static uint32_t procui_acquire(void *ctx)
{
    (void)ctx;
    if (!use_gx2 && screen_memory && !osscreen_up) {
        OSScreenInit();
        OSScreenSetBufferEx(SCREEN_TV, screen_memory);
        OSScreenSetBufferEx(SCREEN_DRC, (char *)screen_memory + tv_size);
        OSScreenEnableEx(SCREEN_TV, 1);
        OSScreenEnableEx(SCREEN_DRC, 1);
        osscreen_up = 1;
        dirty = 1;
    }
    in_foreground = 1;
    if (use_gx2_gs)
        gx2_gs_context_lost(); /* the system used the GPU while we were in the background */
    return 0;
}
static uint32_t procui_release(void *ctx)
{
    (void)ctx;
    in_foreground = 0;
    if (!use_gx2 && osscreen_up) {
        /* Software picture output: hand the display back before ProcUI releases the foreground (HOME menu, exit).
         * Shutting OSScreen down only after the system owns the display can hang on exit. */
        OSScreenEnableEx(SCREEN_TV, 0);
        OSScreenEnableEx(SCREEN_DRC, 0);
        OSScreenShutdown();
        osscreen_up = 0;
    }
    return 0;
}
static int app_running(void)
{
    if (app_alive && !WHBProcIsRunning())
        app_alive = 0;
    return app_alive;
}
static uint32_t compose[640u * 480u];

/* GPU path: compose the current picture (menu or emulator frame) and the last log lines into 640x480 on the CPU,
 * then let the GPU scale it to the TV and the GamePad. */
static void render_gx2(void)
{
    SHARED_LOCK();
    if (shown_w && shown_h)
        wiiu_scale_nearest(compose, 640u, 480u, shown, shown_w, shown_h);
    else
        memset(compose, 0, sizeof compose);
    if (show_log) wiiu_text_dim_rows(compose, 640u, 480u - 3u * 15u - 4u, 480u);
    const unsigned first = !show_log ? line_count : line_count > 3u ? line_count - 3u : 0;
    for (unsigned i = first; i < line_count; i++) {
        char t[81];
        snprintf(t, sizeof t, "%.80s", lines[i % LINES]);
        wiiu_text_draw(compose, 640u, 480u, 0, (int)(480u - 3u * 15u + (i - first) * 15u), t, 255, 255, 255);
    }
    dirty = 0;
    SHARED_UNLOCK();
    gx2_present_frame(compose, 640u, 480u, 1);
}

static void render(void)
{
    if (!in_foreground || (!use_gx2 && !osscreen_up))
        return; /* HOME menu / background: the display belongs to the system */
    if (use_gx2) {
        render_gx2();
        return;
    }
    SHARED_LOCK();
    OSScreenClearBufferEx(SCREEN_TV, 0x101820ff);
    OSScreenClearBufferEx(SCREEN_DRC, 0x101820ff);
    if (shown_w && shown_h) {
        /* Scale to the largest 4:3 picture that fits (the PS2 output is 4:3 whatever field size the GS read
         * returns, e.g. 640x256 or 640x448), nearest neighbour, centred horizontally. */
        unsigned dh = PIC_MAX_H, dw = dh * 4u / 3u;
        if (dw > PIC_MAX_W) { dw = PIC_MAX_W; dh = dw * 3u / 4u; }
        const unsigned ox = (TV_W - dw) / 2u;
        static uint16_t xmap[PIC_MAX_W];
        for (unsigned x = 0; x < dw; x++)
            xmap[x] = (uint16_t)((unsigned long)x * shown_w / dw);
        for (unsigned y = 0; y < dh; y++) {
            const uint32_t *row = &shown[(size_t)((unsigned long)y * shown_h / dh) * shown_w];
            for (unsigned x = 0; x < dw; x++)
                OSScreenPutPixelEx(SCREEN_TV, ox + x, y, row[xmap[x]] << 8);
        }
        {
            /* GamePad: 854x480, 4:3 picture 640x480 centred (always; the verbose log is drawn over its bottom). */
            static uint16_t dmap[640];
            for (unsigned x = 0; x < 640u; x++)
                dmap[x] = (uint16_t)((unsigned long)x * shown_w / 640u);
            for (unsigned y = 0; y < 480u; y++) {
                const uint32_t *row = &shown[(size_t)((unsigned long)y * shown_h / 480u) * shown_w];
                for (unsigned x = 0; x < 640u; x++)
                    OSScreenPutPixelEx(SCREEN_DRC, 107u + x, y, row[dmap[x]] << 8);
            }
        }
    }
    const unsigned first = !show_log ? line_count : line_count > TV_LOG_LINES ? line_count - TV_LOG_LINES : 0;
    const unsigned tv_first = !show_log ? line_count : line_count > TV_LOG_LINES ? line_count - TV_LOG_LINES : 0;
    for (unsigned i = first; i < line_count; i++)
        OSScreenPutFontEx(SCREEN_DRC, 0, DRC_LOG_FIRST_ROW + (i - first), lines[i % LINES]);
    for (unsigned i = tv_first; i < line_count; i++)
        OSScreenPutFontEx(SCREEN_TV, 0, TV_LOG_FIRST_ROW + (i - tv_first), lines[i % LINES]);
    DCFlushRange(screen_memory, tv_size + drc_size);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    dirty = 0;
    SHARED_UNLOCK();
}

static void on_log(void *u, const char *s)
{
    (void)u;
    SHARED_LOCK();
    snprintf(lines[line_count % LINES], LINE_CHARS, "%s", s);
    line_count++;
    dirty = 1;
    SHARED_UNLOCK();
    if (!threaded)
        WHBLogPrint(s);
}

static int on_present(void *u, const uint32_t *p, unsigned w, unsigned h)
{
    (void)u;
    SHARED_LOCK();
    memcpy(shown, p, (size_t)w * h * sizeof shown[0]);
    shown_w = w;
    shown_h = h;
    dirty = 1;
    SHARED_UNLOCK();
    return 0;
}

static int keep(void *u)
{
    (void)u;
    if (threaded)
        return !stop_requested;
    static uint64_t last_proc_ms;
    const uint64_t now = (uint64_t)OSTicksToMilliseconds(OSGetSystemTime());
    if (now - last_proc_ms >= 16u) { /* ProcUI messages (HOME button) about 60 times a second */
        last_proc_ms = now;
        if (!app_running())
            return 0;
    }
    if (!app_alive || return_to_menu)
        return 0;
    if (dirty && now - last_render_ms >= 500u) {
        last_render_ms = now;
        render();
    }
    return 1;
}
static uint64_t now_ms(void *u) { (void)u; return (uint64_t)OSTicksToMilliseconds(OSGetSystemTime()); }


static wiiu_menu *menu;

/* Every Wii U controller is read as unified buttons (G2B_*, see wiiu_menu.h); the menu's mapping table turns them
 * into the PS2 pad. GamePad and Pro/Classic Controller use their own labels; a Wii Remote held sideways uses
 * 2 = B, 1 = Y, B = A, A = X; with a Nunchuk (held upright) A = B, B = A, 1 = Y, 2 = X, C = L, Z = ZL. */
static uint32_t ub(int cond, unsigned b) { return cond ? 1u << b : 0u; }
static uint32_t from_vpad(uint32_t h)
{
    return ub(h & VPAD_BUTTON_A, G2B_A) | ub(h & VPAD_BUTTON_B, G2B_B) | ub(h & VPAD_BUTTON_X, G2B_X) | ub(h & VPAD_BUTTON_Y, G2B_Y) |
           ub(h & VPAD_BUTTON_L, G2B_L) | ub(h & VPAD_BUTTON_R, G2B_R) | ub(h & VPAD_BUTTON_ZL, G2B_ZL) | ub(h & VPAD_BUTTON_ZR, G2B_ZR) |
           ub(h & VPAD_BUTTON_PLUS, G2B_PLUS) | ub(h & VPAD_BUTTON_MINUS, G2B_MINUS) | ub(h & VPAD_BUTTON_UP, G2B_UP) |
           ub(h & VPAD_BUTTON_DOWN, G2B_DOWN) | ub(h & VPAD_BUTTON_LEFT, G2B_LEFT) | ub(h & VPAD_BUTTON_RIGHT, G2B_RIGHT) |
           ub(h & VPAD_BUTTON_STICK_L, G2B_L3) | ub(h & VPAD_BUTTON_STICK_R, G2B_R3);
}
static uint32_t from_classic(uint32_t h, int pro)
{
    return ub(h & WPAD_CLASSIC_BUTTON_A, G2B_A) | ub(h & WPAD_CLASSIC_BUTTON_B, G2B_B) | ub(h & WPAD_CLASSIC_BUTTON_X, G2B_X) |
           ub(h & WPAD_CLASSIC_BUTTON_Y, G2B_Y) | ub(h & WPAD_CLASSIC_BUTTON_L, G2B_L) | ub(h & WPAD_CLASSIC_BUTTON_R, G2B_R) |
           ub(h & WPAD_CLASSIC_BUTTON_ZL, G2B_ZL) | ub(h & WPAD_CLASSIC_BUTTON_ZR, G2B_ZR) | ub(h & WPAD_CLASSIC_BUTTON_PLUS, G2B_PLUS) |
           ub(h & WPAD_CLASSIC_BUTTON_MINUS, G2B_MINUS) | ub(h & WPAD_CLASSIC_BUTTON_UP, G2B_UP) | ub(h & WPAD_CLASSIC_BUTTON_DOWN, G2B_DOWN) |
           ub(h & WPAD_CLASSIC_BUTTON_LEFT, G2B_LEFT) | ub(h & WPAD_CLASSIC_BUTTON_RIGHT, G2B_RIGHT) |
           (pro ? ub(h & WPAD_PRO_BUTTON_STICK_L, G2B_L3) | ub(h & WPAD_PRO_BUTTON_STICK_R, G2B_R3) : 0u);
}
static uint32_t from_wiimote(uint32_t h, int nunchuk)
{
    if (nunchuk)
        return ub(h & WPAD_BUTTON_A, G2B_B) | ub(h & WPAD_BUTTON_B, G2B_A) | ub(h & WPAD_BUTTON_1, G2B_Y) | ub(h & WPAD_BUTTON_2, G2B_X) |
               ub(h & WPAD_BUTTON_C, G2B_L) | ub(h & WPAD_BUTTON_Z, G2B_ZL) | ub(h & WPAD_BUTTON_PLUS, G2B_PLUS) |
               ub(h & WPAD_BUTTON_MINUS, G2B_MINUS) | ub(h & WPAD_BUTTON_UP, G2B_UP) | ub(h & WPAD_BUTTON_DOWN, G2B_DOWN) |
               ub(h & WPAD_BUTTON_LEFT, G2B_LEFT) | ub(h & WPAD_BUTTON_RIGHT, G2B_RIGHT);
    /* Sideways: the D-pad is rotated a quarter turn. */
    return ub(h & WPAD_BUTTON_2, G2B_B) | ub(h & WPAD_BUTTON_1, G2B_Y) | ub(h & WPAD_BUTTON_B, G2B_A) | ub(h & WPAD_BUTTON_A, G2B_X) |
           ub(h & WPAD_BUTTON_PLUS, G2B_PLUS) | ub(h & WPAD_BUTTON_MINUS, G2B_MINUS) | ub(h & WPAD_BUTTON_RIGHT, G2B_UP) |
           ub(h & WPAD_BUTTON_LEFT, G2B_DOWN) | ub(h & WPAD_BUTTON_UP, G2B_LEFT) | ub(h & WPAD_BUTTON_DOWN, G2B_RIGHT);
}
static int kpad_ready;
/* All controllers ORed together. axes: right x, right y, left x, left y in -1..1 (y up). Returns how many answered. */
static int read_unified(uint32_t *hold, float axes[4])
{
    int n = 0;
    *hold = 0;
    for (unsigned i = 0; i < 4; i++) axes[i] = 0.0f;
    VPADStatus st;
    VPADReadError err = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &st, 1, &err) >= 1 && err == VPAD_READ_SUCCESS) {
        *hold |= from_vpad(st.hold);
        axes[0] = st.rightStick.x; axes[1] = st.rightStick.y; axes[2] = st.leftStick.x; axes[3] = st.leftStick.y;
        n++;
    }
    if (kpad_ready)
        for (unsigned c = 0; c < 4u; c++) {
            KPADStatus k;
            KPADError ke = KPAD_ERROR_OK;
            memset(&k, 0, sizeof k);
            if (KPADReadEx((KPADChan)c, &k, 1, &ke) < 1 || ke != KPAD_ERROR_OK) continue;
            float lx = 0, ly = 0, rx = 0, ry = 0;
            switch (k.extensionType) {
            case WPAD_EXT_PRO_CONTROLLER:
                *hold |= from_classic(k.pro.hold, 1);
                lx = k.pro.leftStick.x; ly = k.pro.leftStick.y; rx = k.pro.rightStick.x; ry = k.pro.rightStick.y;
                break;
            case WPAD_EXT_CLASSIC:
            case WPAD_EXT_MPLUS_CLASSIC:
                *hold |= from_classic(k.classic.hold, 0);
                lx = k.classic.leftStick.x; ly = k.classic.leftStick.y; rx = k.classic.rightStick.x; ry = k.classic.rightStick.y;
                break;
            case WPAD_EXT_NUNCHUK:
            case WPAD_EXT_MPLUS_NUNCHUK:
                *hold |= from_wiimote(k.hold, 1);
                lx = k.nunchuk.stick.x; ly = k.nunchuk.stick.y;
                break;
            default:
                *hold |= from_wiimote(k.hold, 0);
                break;
            }
            /* The stick pushed furthest wins when several controllers are used at once. */
            const float v[4] = { rx, ry, lx, ly };
            for (unsigned a = 0; a < 4; a++)
                if ((v[a] < 0 ? -v[a] : v[a]) > (axes[a] < 0 ? -axes[a] : axes[a])) axes[a] = v[a];
            n++;
        }
    return n;
}

/* PS2 pad from all controllers through the user's mapping. Hold L+R+ZL+ZR and press + to return to the menu. */
static int read_pad(uint16_t *pressed, uint8_t axes[4])
{
    static uint32_t prev;
    uint32_t h;
    float a[4];
    if (!read_unified(&h, a))
        return 1;
    const uint32_t combo = (1u << G2B_L) | (1u << G2B_R) | (1u << G2B_ZL) | (1u << G2B_ZR);
    if ((h & combo) == combo && (h & ~prev & (1u << G2B_PLUS)))
        return_to_menu = 1;
    prev = h;
    *pressed = wiiu_menu_map_buttons(menu, h);
    const float v[4] = { a[0], -a[1], a[2], -a[3] };
    for (unsigned i = 0; i < 4; i++) {
        float f = v[i] * 127.0f + 128.0f;
        axes[i] = (uint8_t)(f < 0.0f ? 0 : f > 255.0f ? 255 : (int)f);
    }
    return 0;
}

static int poll_pad(void *u, uint16_t *pressed, uint8_t axes[4])
{
    (void)u;
    if (!threaded)
        return read_pad(pressed, axes);
    if (!shared_pad_ok)
        return 1;
    *pressed = shared_pressed;
    for (unsigned i = 0; i < 4; i++)
        axes[i] = shared_axes[i];
    return 0;
}

static int readable(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

#ifdef GEKKO2_WIIU_JIT
#endif
static g2_platform g_platform;
static g2_config g_config;
static g2_result g_result;
static int file_exists_flag(const char *sd_root, const char *name)
{
    char path[300];
    snprintf(path, sizeof path, "%s/wiiu/gekko2/%s", sd_root[0] ? sd_root : "/vol/external01", name);
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}
static int gs_async_wanted(const char *sd_root)
{
    char path[300];
    snprintf(path, sizeof path, "%s/wiiu/gekko2/gs_async_off.txt", sd_root[0] ? sd_root : "/vol/external01");
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 0;
    }
    return 1;
}
static int emu_entry(int argc, void *argv)
{
    (void)argc; (void)argv;
    g2_run(&g_config, &g_platform, &g_result);
    emu_done = 1;
    return 0;
}

/* GX2 calls requested by the emulation thread (GPU renderer resolves) run on this main thread. */
static OSThread *gx2_thread;
static volatile int rpc_pending, rpc_done;
static int (*rpc_fn)(void *, uint8_t *, uint32_t);
static void *rpc_opaque;
static uint8_t *rpc_vram;
static uint32_t rpc_size;
static int rpc_ret;
static int gx2_call(int (*fn)(void *, uint8_t *, uint32_t), void *opaque, uint8_t *vram, uint32_t size)
{
    if (OSGetCurrentThread() == gx2_thread)
        return fn(opaque, vram, size);
    rpc_fn = fn;
    rpc_opaque = opaque;
    rpc_vram = vram;
    rpc_size = size;
    rpc_done = 0;
    __sync_synchronize();
    rpc_pending = 1;
    while (!rpc_done)
        OSSleepTicks(OSMicrosecondsToTicks(20));
    __sync_synchronize();
    return rpc_ret;
}
static void rpc_service(void)
{
    if (!rpc_pending)
        return;
    __sync_synchronize();
    rpc_pending = 0;
    rpc_ret = rpc_fn(rpc_opaque, rpc_vram, rpc_size);
    __sync_synchronize();
    rpc_done = 1;
}

/* GS/GIF worker thread: a second Espresso core runs the GIF/GS work, the emulation thread only hands data over.
 * On by default; create <sd>/wiiu/gekko2/gs_async_off.txt to run everything on one thread again. */
static OSThread gs_thread;
static unsigned gs_core = 0xffu;
static void *gs_stack;
static void (*gs_fn)(void);
static int gs_entry(int argc, void *argv)
{
    (void)argc; (void)argv;
    gs_fn();
    return 0;
}
static volatile int gs_main_pending;
static int start_worker(void (*fn)(void), void *user)
{
    (void)user;
    if (gs_on_main) {
        /* The main thread (GX2 core) runs the GS worker: run_emulation() picks it up. */
        gs_fn = fn;
        gs_core = (unsigned)OSGetMainCoreId();
        __sync_synchronize();
        gs_main_pending = 1;
        return 0;
    }
    const uint32_t stack_size = 512u * 1024u;
    if (!gs_stack)
        gs_stack = MEMAllocFromDefaultHeapEx(stack_size, 16);
    if (!gs_stack)
        return -1;
    gs_fn = fn;
    unsigned core = 2;
    if ((unsigned)OSGetCoreId() == core)
        core = 1;
    gs_core = core;
    if (!OSCreateThread(&gs_thread, gs_entry, 0, NULL, (uint8_t *)gs_stack + stack_size, stack_size, 16, 1u << core))
        return -1;
    OSSetThreadName(&gs_thread, "gekko2-gs");
    OSResumeThread(&gs_thread);
    OSDetachThread(&gs_thread);
    return 0;
}
static void worker_idle(void) { OSYieldThread(); }
static void worker_nap(void) { OSSleepTicks(OSMicrosecondsToTicks(50)); }

/* IOP worker: the third Espresso core (neither the emulation thread's nor the GS worker's). The core needs the stack
 * range to tell the worker apart, so the caller provides the stack. */
static OSThread iop_thread;
static void (*iop_fn)(void);
static int iop_entry(int argc, void *argv)
{
    (void)argc; (void)argv;
    iop_fn();
    return 0;
}
static int start_worker_stack(void (*fn)(void), void *user, void *stack, unsigned stack_size)
{
    (void)user;
    iop_fn = fn;
    const unsigned emu = (unsigned)OSGetCoreId();
    const unsigned gs = gs_core <= 2u ? gs_core : (emu == 2u ? 1u : 2u);
    unsigned core = 3u - emu - gs;
    if (core > 2u || core == emu || core == gs)
        core = emu == 0u ? 1u : 0u;
    if (!OSCreateThread(&iop_thread, iop_entry, 0, NULL, (uint8_t *)stack + stack_size, stack_size, 16, 1u << core))
        return -1;
    OSSetThreadName(&iop_thread, "gekko2-iop");
    OSResumeThread(&iop_thread);
    OSDetachThread(&iop_thread);
    return 0;
}

/* VU1 worker: the third Espresso core as well (shared with the IOP worker only if both are switched on). */
static OSThread vu_thread;
static void *vu_stack;
static void (*vu_fn)(void);
static int vu_entry(int argc, void *argv)
{
    (void)argc; (void)argv;
    vu_fn();
    return 0;
}
static int start_worker_vu(void (*fn)(void), void *user)
{
    (void)user;
    const uint32_t stack_size = 256u * 1024u;
    if (!vu_stack)
        vu_stack = MEMAllocFromDefaultHeapEx(stack_size, 16);
    if (!vu_stack)
        return -1;
    vu_fn = fn;
    const unsigned emu = (unsigned)OSGetCoreId();
    const unsigned gs = gs_core <= 2u ? gs_core : (emu == 2u ? 1u : 2u);
    unsigned core = 3u - emu - gs;
    if (core > 2u || core == emu || core == gs)
        core = emu == 0u ? 1u : 0u;
    if (!OSCreateThread(&vu_thread, vu_entry, 0, NULL, (uint8_t *)vu_stack + stack_size, stack_size, 16, 1u << core))
        return -1;
    OSSetThreadName(&vu_thread, "gekko2-vu1");
    OSResumeThread(&vu_thread);
    OSDetachThread(&vu_thread);
    return 0;
}

static uint32_t menu_fb[WM_W * WM_H];

static void show_menu(void)
{
    wiiu_menu_render(menu, menu_fb);
    SHARED_LOCK();
    memcpy(shown, menu_fb, sizeof menu_fb);
    shown_w = WM_W;
    shown_h = WM_H;
    dirty = 1;
    SHARED_UNLOCK();
}

/* Controller buttons (unified) to menu edges, with a simple key repeat on UP/DOWN. */
static unsigned menu_edges(uint32_t trigger, uint32_t hold, uint64_t now)
{
    static uint64_t next_repeat;
    const uint32_t up = 1u << G2B_UP, down = 1u << G2B_DOWN;
    unsigned e = 0;
    if (trigger & up) e |= WM_UP;
    if (trigger & down) e |= WM_DOWN;
    if (trigger & (1u << G2B_LEFT)) e |= WM_LEFT;
    if (trigger & (1u << G2B_RIGHT)) e |= WM_RIGHT;
    if (trigger & (1u << G2B_A)) e |= WM_A;
    if (trigger & (1u << G2B_B)) e |= WM_B;
    if (trigger & (1u << G2B_PLUS)) e |= WM_START;
    if (trigger & (1u << G2B_L)) e |= WM_PAGE_UP;
    if (trigger & (1u << G2B_R)) e |= WM_PAGE_DOWN;
    if (trigger & (1u << G2B_X)) e |= WM_X;
    if (trigger & (1u << G2B_Y)) e |= WM_Y;
    if (trigger & (1u << G2B_MINUS)) e |= WM_MINUS;
    if (trigger & (1u << G2B_ZR)) e |= WM_ZR;
    if (trigger & (up | down))
        next_repeat = now + 400u;
    else if ((hold & (up | down)) && now >= next_repeat) {
        e |= (hold & up) ? WM_UP : WM_DOWN;
        next_repeat = now + 80u;
    }
    return e;
}

/* Main-thread housekeeping while the emulator runs on its own thread: controllers, HOME/ProcUI, picture,
 * GX2 requests. full = 0: only the GX2 requests (called very often from the GS worker loop). */
static int no_render_in_worker; /* diagnostics: norender_worker.txt */
static void main_tick(int full)
{
    rpc_service();
    static uint64_t last_ms;
    const uint64_t now = now_ms(NULL);
    if (!full && now - last_ms < 4u)
        return;
    last_ms = now;
    uint16_t pressed = 0;
    uint8_t axes[4] = { 0x80, 0x80, 0x80, 0x80 };
    const int ok = read_pad(&pressed, axes) == 0;
    shared_pressed = pressed;
    for (unsigned i = 0; i < 4; i++)
        shared_axes[i] = axes[i];
    shared_pad_ok = ok;
    static uint64_t last_proc_ms;
    if (now - last_proc_ms >= 16u) {
        last_proc_ms = now;
        if (!app_running())
            stop_requested = 1;
    }
    if (return_to_menu)
        stop_requested = 1;
    if (dirty && now - last_render_ms >= 500u && !(no_render_in_worker && !full)) {
        last_render_ms = now;
        render();
    }
}
static void worker_tick(void) { main_tick(0); }

static void run_emulation(void)
{
    return_to_menu = 0;
    if (!threaded) {
        g2_run(&g_config, &g_platform, &g_result);
        return;
    }
    static OSThread emu_thread;
    static void *stack;
    const uint32_t stack_size = 1024u * 1024u;
    if (!stack)
        stack = MEMAllocFromDefaultHeapEx(stack_size, 16);
    if (!stack) {
        on_log(NULL, "no memory for the emulator thread");
        return;
    }
    stop_requested = 0;
    emu_done = 0;
    gs_main_pending = 0;
    gx2_thread = OSGetCurrentThread();
#ifdef GEKKO2_WIIU_JIT
    const unsigned emu_core = (unsigned)OSGetCodegenCore(); /* generated code only runs on the codegen core */
#else
    const unsigned emu_core = (unsigned)OSGetMainCoreId() == 2u ? 0u : 2u;
#endif
#ifdef GEKKO2_WIIU_GX2
    if (gs_on_main) {
        extern int (*gekko2_gx2_call)(int (*fn)(void *, uint8_t *, uint32_t), void *, uint8_t *, uint32_t);
        gekko2_gx2_call = gx2_call;
    }
#endif
    OSCreateThread(&emu_thread, emu_entry, 0, NULL, (uint8_t *)stack + stack_size, stack_size, 16, 1u << emu_core);
    OSSetThreadName(&emu_thread, "gekko2-emu");
    OSResumeThread(&emu_thread);
    while (!emu_done) {
        main_tick(1);
        if (gs_main_pending) {
            /* GS worker on this thread (GX2 core) until the run loop detaches it; housekeeping via the tick hook. */
            extern void (*gekko2_worker_tick)(void);
            gs_main_pending = 0;
            gekko2_worker_tick = worker_tick;
            gs_fn();
            gekko2_worker_tick = NULL;
            continue;
        }
        OSSleepTicks(OSMillisecondsToTicks(2));
    }
    int ret = 0;
    OSJoinThread(&emu_thread, &ret);
#ifdef GEKKO2_WIIU_GX2
    if (gs_on_main) {
        extern int (*gekko2_gx2_call)(int (*fn)(void *, uint8_t *, uint32_t), void *, uint8_t *, uint32_t);
        gekko2_gx2_call = NULL;
    }
#endif
}

static void *read_whole_file(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = n > 0 ? MEMAllocFromDefaultHeapEx((uint32_t)n, 64) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        MEMFreeToDefaultHeap(buf);
        buf = NULL;
    }
    fclose(f);
    *size = buf ? (uint32_t)n : 0;
    return buf;
}

/* OSScreen path: software drawing, the fallback when there is no usable present.gsh. */
static int init_osscreen(void)
{
    OSScreenInit();
    tv_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
    screen_memory = MEMAllocFromDefaultHeapEx(tv_size + drc_size, 0x100);
    if (!screen_memory)
        return 0;
    OSScreenSetBufferEx(SCREEN_TV, screen_memory);
    OSScreenSetBufferEx(SCREEN_DRC, (char *)screen_memory + tv_size);
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
    osscreen_up = 1;
    return 1;
}

int main(void)
{
    WHBProcInit();
    ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, procui_acquire, NULL, 200);
    ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, procui_release, NULL, 200);
    WHBLogCafeInit();
    VPADInit();
    KPADInit();
    WPADEnableURCC(1);
    kpad_ready = 1;
    OSInitMutex(&shared_lock);

    on_log(NULL, "Gekko2 Wii U Edition " G2_WIIU_VERSION " (ALPHA)");
    on_log(NULL, "Alpha build: expect bugs; speed is far below full PS2 speed.");
    char mount_bios[256] = "", fallback_bios[256] = "fs:/vol/external01/wiiu/gekko2/bios/ps2bios.bin";
    char plain_bios[256] = "/vol/external01/wiiu/gekko2/bios/ps2bios.bin", sd_root[256] = "";
    if (WHBMountSdCard()) {
        const char *root = WHBGetSdCardMountPath();
        if (root) {
            snprintf(sd_root, sizeof sd_root, "%s", root);
            snprintf(mount_bios, sizeof mount_bios, "%s/wiiu/gekko2/bios/ps2bios.bin", root);
            on_log(NULL, "SD card mounted");
        }
    } else {
        on_log(NULL, "SD card could not be mounted");
    }
    const char *candidates[] = { mount_bios, plain_bios, fallback_bios };
    const char *bios = NULL;
    for (unsigned i = 0; i < 3; i++) {
        if (!candidates[i][0])
            continue;
        if (readable(candidates[i])) {
            bios = candidates[i];
            break;
        }
        char tried[LINE_CHARS];
        snprintf(tried, sizeof tried, "not found: %.80s", candidates[i]);
        on_log(NULL, tried);
    }
    if (!bios)
        on_log(NULL, "Put your own PS2 BIOS at sd:/wiiu/gekko2/bios/ps2bios.bin (4 MiB dump).");

    /* Display: GX2 (GPU scaling) when <sd>/wiiu/gekko2/present.gsh loads, otherwise OSScreen. They exclude each other. */
    {
        char gsh_path[300];
        uint32_t gsh_size = 0;
        void *gsh = NULL;
        const int gx2_off = sd_root[0] && file_exists_flag(sd_root, "gx2_off.txt"); /* force the OSScreen path */
        if (sd_root[0] && !gx2_off) {
            snprintf(gsh_path, sizeof gsh_path, "%s/wiiu/gekko2/present.gsh", sd_root);
            gsh = read_whole_file(gsh_path, &gsh_size);
        }
#if __has_include("present_gsh_embedded.h")
        /* Release builds carry the assembled presenter shaders; a present.gsh on the SD card still overrides them. */
        if (!gsh && !gx2_off) {
#include "present_gsh_embedded.h"
            gsh = MEMAllocFromDefaultHeapEx(sizeof present_gsh_data, 0x100);
            if (gsh) {
                memcpy(gsh, present_gsh_data, sizeof present_gsh_data);
                gsh_size = sizeof present_gsh_data;
            }
        }
#endif
        if (gsh && gx2_present_init(gsh, gsh_size, 640u, 480u)) {
            use_gx2 = 1;
            on_log(NULL, "GX2 presenter active (GPU scaling, present.gsh)");
#if defined(GEKKO2_WIIU_GX2) && __has_include("gs_color_gsh.h") && __has_include("gs_mod_gsh.h") && __has_include("gs_zload_gsh.h")
            gx2_gs_available = 1; /* GPU GS renderer: started on the first game start with the menu switch on */
#endif
        } else {
            on_log(NULL, gx2_off ? "gx2_off.txt: OSScreen (software)" : gsh ? "present.gsh was rejected: using OSScreen" : "no present.gsh on the SD card: OSScreen (software)");
            if (!init_osscreen())
                return 1;
        }
    }

    menu = wiiu_menu_create(sd_root[0] ? sd_root : "/vol/external01", "ps2bios.bin");
    if (menu) {
        if (!bios)
            wiiu_menu_set_notice(menu, "BIOS not found: sd:/wiiu/gekko2/bios/ps2bios.bin");
        show_menu();
    }
    { extern void (*gekko2_worker_nap)(void); gekko2_worker_nap = worker_nap; }
    g_platform = (g2_platform){ on_log, on_present, keep, now_ms, poll_pad, start_worker, worker_idle, NULL, start_worker_stack, start_worker_vu };
    while (menu && app_running()) {
        static uint32_t prev_hold;
        uint32_t hold = 0;
        float axes_unused[4];
        read_unified(&hold, axes_unused);
        const uint32_t trigger = hold & ~prev_hold;
        prev_hold = hold;
        const uint64_t now = now_ms(NULL);
        /* Controls page: a captured button assigns a mapping and is not also a menu command. */
        const int captured = trigger && wiiu_menu_raw_trigger(menu, trigger);
        if (captured) show_menu();
        unsigned edges = captured ? 0u : menu_edges(trigger, hold, now);
        /* Test hook for unattended runs (Cemu without input): autostart_disc.txt starts the selected disc,
         * autostart_bios.txt the BIOS, once per app start. */
        static int autostart_done;
        int auto_action = 0;
        if (!autostart_done && sd_root[0]) {
            autostart_done = 1;
            /* autostart_disc.txt holds a file name inside sd:/wiiu/gekko2/games/ */
            char ap[300], name[128] = "";
            snprintf(ap, sizeof ap, "%s/wiiu/gekko2/autostart_disc.txt", sd_root);
            FILE *af = fopen(ap, "r");
            if (af) {
                if (fgets(name, sizeof name, af)) {
                    size_t n = strlen(name);
                    while (n && (name[n - 1] == '\n' || name[n - 1] == '\r' || name[n - 1] == ' ')) name[--n] = 0;
                }
                fclose(af);
                if (name[0]) {
                    snprintf(ap, sizeof ap, "%s/wiiu/gekko2/games/%s", sd_root, name);
                    wiiu_menu_set_disc(menu, ap);
                }
            }
            auto_action = wiiu_menu_disc(menu) && name[0] ? WM_ACTION_START_DISC
                          : file_exists_flag(sd_root, "autostart_bios.txt") ? WM_ACTION_START_BIOS : 0;
        }
        if (edges || auto_action) {
            const int action = auto_action ? auto_action : wiiu_menu_input(menu, edges);
            show_menu();
            if (action == WM_ACTION_EXIT)
                break;
            if (action == WM_ACTION_START_BIOS || action == WM_ACTION_START_DISC) {
                if (!bios) {
                    wiiu_menu_set_notice(menu, "BIOS not found: sd:/wiiu/gekko2/bios/ps2bios.bin");
                } else {
                    static char session_log[300];
                    snprintf(session_log, sizeof session_log, "%s/wiiu/gekko2/session.log", sd_root[0] ? sd_root : "/vol/external01");
                    const int verbose = wiiu_menu_verbose(menu);
                    show_log = verbose;
                    SHARED_LOCK();
                    line_count = 0;
                    SHARED_UNLOCK();
                    wiiu_menu_render_loading(menu, menu_fb, action == WM_ACTION_START_DISC ? wiiu_menu_disc(menu) : "PS2 BIOS");
                    SHARED_LOCK();
                    memcpy(shown, menu_fb, sizeof menu_fb);
                    shown_w = WM_W;
                    shown_h = WM_H;
                    dirty = 1;
                    SHARED_UNLOCK();
                    render();
                    last_render_ms = now_ms(NULL);
                    /* Cemu (directly loaded RPX, title ID ffffffff...): GS work on its own core made Cemu write back
                     * GPU pixels late, so stale rows reached the PS2 picture (Tekken: second Namco logo). There the
                     * emulator and the GS work share the main core, as with GS CORE: SHARED. */
                    const int on_cemu = (OSGetTitleID() >> 32) == 0xffffffffull;
#ifdef GEKKO2_WIIU_JIT
                    /* JIT app on a Wii U: the emulator runs on the codegen core, so the GPU renderer needs the GS work
                     * on this main thread (the GX2 core); without the GS worker thread it stays off. On Cemu the
                     * emulator runs here (fast Cemu codegen mode), unless jit_cemu_safe.txt asks for the Wii U layout. */
                    /* Real Wii U (Aroma): generated code in the RWX heap (see wiiu_codegen.c) can run on this main core
                     * as well, so the layout is the same as on Cemu. jit_codegen.txt keeps the official codegen area. */
                    extern int gekko2_wiiu_rwx_jit;
                    extern int gekko2_codegen_try_rwx(void);
                    gekko2_wiiu_rwx_jit = !on_cemu && !file_exists_flag(sd_root, "jit_codegen.txt");
                    const int rwx = gekko2_wiiu_rwx_jit && gekko2_codegen_try_rwx();
                    {
                        extern const char *gekko2_codegen_rwx_info(void);
                        if (gekko2_wiiu_rwx_jit)
                            on_log(NULL, gekko2_codegen_rwx_info());
                    }
                    const int cemu_main = (on_cemu && !file_exists_flag(sd_root, "jit_cemu_safe.txt")) || rwx;
                    const int gpu = wiiu_menu_gpu(menu) && (cemu_main || gs_async_wanted(sd_root)) && gx2_gs_start();
                    const int split = gpu && !cemu_main;
#else
                    const int gpu = wiiu_menu_gpu(menu) && gx2_gs_start();
                    const int split = gpu && !on_cemu && wiiu_menu_gpu_split(menu) && gs_async_wanted(sd_root);
#endif
#ifdef GEKKO2_WIIU_JIT
                    threaded = !cemu_main;
                    {
                        extern int gekko2_wiiu_cemu_fast_jit;
                        gekko2_wiiu_cemu_fast_jit = !file_exists_flag(sd_root, "jit_cemu_safe.txt");
                        {
                            extern int gekko2_wiiu_jit_exp;
                            char ep[300];
                            snprintf(ep, sizeof ep, "%s/wiiu/gekko2/jit_exp.txt", sd_root[0] ? sd_root : "/vol/external01");
                            FILE *ef = fopen(ep, "r");
                            if (ef) {
                                if (fscanf(ef, "%i", &gekko2_wiiu_jit_exp) != 1) gekko2_wiiu_jit_exp = 0;
                                fclose(ef);
                            }
                        }
                        /* jit_dump.txt: "lo hi" guest pc range (hex) whose native blocks go into the session log */
                        extern uint32_t gekko2_wiiu_jit_dump_lo, gekko2_wiiu_jit_dump_hi;
                        char dp[300];
                        snprintf(dp, sizeof dp, "%s/wiiu/gekko2/jit_dump.txt", sd_root[0] ? sd_root : "/vol/external01");
                        FILE *df = fopen(dp, "r");
                        if (df) {
                            unsigned lo = 0, hi = 0;
                            if (fscanf(df, "%x %x", &lo, &hi) == 2) {
                                gekko2_wiiu_jit_dump_lo = lo;
                                gekko2_wiiu_jit_dump_hi = hi;
                            }
                            fclose(df);
                        }
                    }
#else
                    threaded = split;
#endif
                    gs_on_main = split;
                    g_config = (g2_config){ bios, action == WM_ACTION_START_DISC ? wiiu_menu_disc(menu) : NULL,
                                            2000u, 0, 1000u, 0, 1, verbose ? session_log : NULL, gs_async_wanted(sd_root), !file_exists_flag(sd_root, "fast_run_off.txt"),
                                            file_exists_flag(sd_root, "event_sched_on.txt"), file_exists_flag(sd_root, "iop_async_on.txt"),
                                            file_exists_flag(sd_root, "vu_jit_off.txt"), file_exists_flag(sd_root, "vu1_async_on.txt"),
                                            gpu, split };
                    {
                        extern int gekko2_wiiu_display_sync;
                        gekko2_wiiu_display_sync = file_exists_flag(sd_root, "display_sync.txt");
                        no_render_in_worker = file_exists_flag(sd_root, "norender_worker.txt");
                        {
                            extern int gekko2_wiiu_g2_notex;
                            gekko2_wiiu_g2_notex = file_exists_flag(sd_root, "gpu_notex.txt");
                            extern int tex_pad_rows;
                            tex_pad_rows = file_exists_flag(sd_root, "tex_pad.txt");
                            extern int gekko2_wiiu_g2trace_on;
                            gekko2_wiiu_g2trace_on = file_exists_flag(sd_root, "g2trace.txt");
                        }
                        extern unsigned gekko2_wiiu_dump_first, gekko2_wiiu_dump_last;
                        extern char gekko2_wiiu_dump_dir[200];
                        snprintf(gekko2_wiiu_dump_dir, sizeof gekko2_wiiu_dump_dir, "%s/wiiu/gekko2", sd_root[0] ? sd_root : "/vol/external01");
                        char fp[300];
                        snprintf(fp, sizeof fp, "%s/frame_dump.txt", gekko2_wiiu_dump_dir);
                        FILE *ff = fopen(fp, "r");
                        gekko2_wiiu_dump_first = gekko2_wiiu_dump_last = 0;
                        if (ff) {
                            if (fscanf(ff, "%u %u", &gekko2_wiiu_dump_first, &gekko2_wiiu_dump_last) != 2)
                                gekko2_wiiu_dump_first = gekko2_wiiu_dump_last = 0;
                            fclose(ff);
                        }
                    }
                    run_emulation();
                    threaded = 0;
                    gs_on_main = 0;
                    show_log = 0;
                    wiiu_menu_set_notice(menu, verbose ? "Back. Log saved: sd:/wiiu/gekko2/session.log" : "Back from emulation. Restart the app for a clean boot.");
                }
                show_menu();
            }
        }
        if (dirty && now - last_render_ms >= 100u) {
            last_render_ms = now;
            render();
        }
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    if (app_alive) {
        /* "Exit" in our menu: under Aroma the app may not just return; launch the Wii U Menu and wait for EXITING. */
        SYSLaunchMenu();
        while (app_running())
            OSSleepTicks(OSMillisecondsToTicks(16));
    }
    wiiu_menu_destroy(menu);
    KPADShutdown();
    VPADShutdown();
    WHBUnmountSdCard();
    if (use_gx2) {
        if (use_gx2_gs)
            gx2_gs_shutdown();
        gx2_present_shutdown();
    } else {
        if (osscreen_up) /* normally already shut down by the ProcUI release callback */
            OSScreenShutdown();
        osscreen_up = 0;
        MEMFreeToDefaultHeap(screen_memory);
    }
    WHBLogCafeDeinit();
    WHBProcShutdown();
    return 0;
}
