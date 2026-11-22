/* GPL-3.0-or-later. Wii U (wut) front end for the portable Gekko2 run loop. Test port only:
 * interpreter, software framebuffer through OSScreen, GamePad input, no audio, no GX2.
 * Looks for the user's own BIOS at <sd>/wiiu/gekko2/bios/ps2bios.bin. Nothing is bundled. */
#include "g2_app.h"
#include "wiiu_menu.h"
#include "core/hw/iop_sio2.h"
#include <coreinit/cache.h>
#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/mutex.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <vpad/input.h>
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
#define PIC_MAX_W 1280u
#define PIC_MAX_H 624u /* rows 0..25 of the TV text grid; the log uses the last three rows */
#define LINE_CHARS 100

static char lines[LINES][LINE_CHARS];
static unsigned line_count;
static uint32_t shown[1024u * 1024u];
static unsigned shown_w, shown_h;
static void *screen_memory;
static uint32_t tv_size, drc_size;

#ifdef GEKKO2_WIIU_JIT
/* JIT build: the emulator runs on its own thread on the codegen core (core 0); this thread (core 1) keeps
 * ProcUI, VPAD and OSScreen. Shared state is guarded by one mutex. */
static OSMutex shared_lock;
static volatile int stop_requested;
static volatile uint16_t shared_pressed;
static volatile uint8_t shared_axes[4] = { 0x80, 0x80, 0x80, 0x80 };
static volatile int shared_pad_ok;
#define SHARED_LOCK() OSLockMutex(&shared_lock)
#define SHARED_UNLOCK() OSUnlockMutex(&shared_lock)
#else
#define SHARED_LOCK() ((void)0)
#define SHARED_UNLOCK() ((void)0)
#endif

static int dirty = 1;
static uint64_t last_render_ms;
static volatile int return_to_menu; /* hold L+R+ZL+ZR and press + while emulating */

static void render(void)
{
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
    }
    const unsigned first = line_count > LINES ? line_count - LINES : 0;
    const unsigned tv_first = line_count > TV_LOG_LINES ? line_count - TV_LOG_LINES : 0;
    for (unsigned i = first; i < line_count; i++)
        OSScreenPutFontEx(SCREEN_DRC, 0, i - first, lines[i % LINES]);
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
#ifndef GEKKO2_WIIU_JIT
    WHBLogPrint(s);
#endif
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
#ifdef GEKKO2_WIIU_JIT
    return !stop_requested;
#else
    if (!WHBProcIsRunning() || return_to_menu)
        return 0;
    const uint64_t now = (uint64_t)OSTicksToMilliseconds(OSGetSystemTime());
    if (dirty && now - last_render_ms >= 500u) {
        last_render_ms = now;
        render();
    }
    return 1;
#endif
}
static uint64_t now_ms(void *u) { (void)u; return (uint64_t)OSTicksToMilliseconds(OSGetSystemTime()); }


/* Wii U GamePad to PS2 pad. Nintendo layout: B = Cross, A = Circle, Y = Square, X = Triangle,
 * L/R = L1/R1, ZL/ZR = L2/R2, + = Start, - = Select, stick clicks = L3/R3. HOME is left to the OS. */
static int read_pad(uint16_t *pressed, uint8_t axes[4])
{
    VPADStatus st;
    VPADReadError err = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &st, 1, &err) < 1 || err != VPAD_READ_SUCCESS)
        return 1;
    const uint32_t h = st.hold;
    {
        const uint32_t combo = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_ZL | VPAD_BUTTON_ZR;
        if ((h & combo) == combo && (st.trigger & VPAD_BUTTON_PLUS))
            return_to_menu = 1;
    }
    uint16_t p = 0;
    if (h & VPAD_BUTTON_B) p |= IOP_PAD_BTN_CROSS;
    if (h & VPAD_BUTTON_A) p |= IOP_PAD_BTN_CIRCLE;
    if (h & VPAD_BUTTON_Y) p |= IOP_PAD_BTN_SQUARE;
    if (h & VPAD_BUTTON_X) p |= IOP_PAD_BTN_TRIANGLE;
    if (h & VPAD_BUTTON_L) p |= IOP_PAD_BTN_L1;
    if (h & VPAD_BUTTON_R) p |= IOP_PAD_BTN_R1;
    if (h & VPAD_BUTTON_ZL) p |= IOP_PAD_BTN_L2;
    if (h & VPAD_BUTTON_ZR) p |= IOP_PAD_BTN_R2;
    if (h & VPAD_BUTTON_PLUS) p |= IOP_PAD_BTN_START;
    if (h & VPAD_BUTTON_MINUS) p |= IOP_PAD_BTN_SELECT;
    if (h & VPAD_BUTTON_UP) p |= IOP_PAD_BTN_UP;
    if (h & VPAD_BUTTON_DOWN) p |= IOP_PAD_BTN_DOWN;
    if (h & VPAD_BUTTON_LEFT) p |= IOP_PAD_BTN_LEFT;
    if (h & VPAD_BUTTON_RIGHT) p |= IOP_PAD_BTN_RIGHT;
    if (h & VPAD_BUTTON_STICK_L) p |= IOP_PAD_BTN_L3;
    if (h & VPAD_BUTTON_STICK_R) p |= IOP_PAD_BTN_R3;
    *pressed = p;
    const float v[4] = { st.rightStick.x, -st.rightStick.y, st.leftStick.x, -st.leftStick.y };
    for (unsigned i = 0; i < 4; i++) {
        float f = v[i] * 127.0f + 128.0f;
        axes[i] = (uint8_t)(f < 0.0f ? 0 : f > 255.0f ? 255 : (int)f);
    }
    return 0;
}

#ifdef GEKKO2_WIIU_JIT
static int poll_pad(void *u, uint16_t *pressed, uint8_t axes[4])
{
    (void)u;
    if (!shared_pad_ok)
        return 1;
    *pressed = shared_pressed;
    for (unsigned i = 0; i < 4; i++)
        axes[i] = shared_axes[i];
    return 0;
}
#else
static int poll_pad(void *u, uint16_t *pressed, uint8_t axes[4])
{
    (void)u;
    return read_pad(pressed, axes);
}
#endif

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
#ifdef GEKKO2_WIIU_JIT
static int emu_entry(int argc, void *argv)
{
    (void)argc; (void)argv;
    g2_run(&g_config, &g_platform, &g_result);
    return 0;
}
#endif

static wiiu_menu *menu;
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

/* D-pad / buttons to menu edges, with a simple key repeat on UP/DOWN. */
static unsigned menu_edges(uint32_t trigger, uint32_t hold, uint64_t now)
{
    static uint64_t next_repeat;
    unsigned e = 0;
    if (trigger & VPAD_BUTTON_UP) e |= WM_UP;
    if (trigger & VPAD_BUTTON_DOWN) e |= WM_DOWN;
    if (trigger & VPAD_BUTTON_LEFT) e |= WM_LEFT;
    if (trigger & VPAD_BUTTON_RIGHT) e |= WM_RIGHT;
    if (trigger & VPAD_BUTTON_A) e |= WM_A;
    if (trigger & VPAD_BUTTON_B) e |= WM_B;
    if (trigger & VPAD_BUTTON_PLUS) e |= WM_START;
    if (trigger & VPAD_BUTTON_L) e |= WM_PAGE_UP;
    if (trigger & VPAD_BUTTON_R) e |= WM_PAGE_DOWN;
    if (trigger & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN))
        next_repeat = now + 400u;
    else if ((hold & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN)) && now >= next_repeat) {
        e |= (hold & VPAD_BUTTON_UP) ? WM_UP : WM_DOWN;
        next_repeat = now + 80u;
    }
    return e;
}

static void run_emulation(void)
{
    return_to_menu = 0;
#ifdef GEKKO2_WIIU_JIT
    /* Emulator thread on the codegen core; this thread keeps ProcUI, VPAD and the screen. */
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
    OSCreateThread(&emu_thread, emu_entry, 0, NULL, (uint8_t *)stack + stack_size, stack_size, 16,
                   1u << (unsigned)OSGetCodegenCore());
    OSSetThreadName(&emu_thread, "gekko2-emu");
    OSResumeThread(&emu_thread);
    while (WHBProcIsRunning() && !return_to_menu) {
        uint16_t pressed = 0;
        uint8_t axes[4] = { 0x80, 0x80, 0x80, 0x80 };
        const int ok = read_pad(&pressed, axes) == 0;
        shared_pressed = pressed;
        for (unsigned i = 0; i < 4; i++)
            shared_axes[i] = axes[i];
        shared_pad_ok = ok;
        const uint64_t now = now_ms(NULL);
        if (dirty && now - last_render_ms >= 500u) {
            last_render_ms = now;
            render();
        }
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    stop_requested = 1;
    int ret = 0;
    OSJoinThread(&emu_thread, &ret);
#else
    g2_run(&g_config, &g_platform, &g_result);
#endif
}

int main(void)
{
    WHBProcInit();
    WHBLogCafeInit();
    VPADInit();
    OSScreenInit();
    tv_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
    screen_memory = MEMAllocFromDefaultHeapEx(tv_size + drc_size, 0x100);
    if (!screen_memory)
        return 1;
    OSScreenSetBufferEx(SCREEN_TV, screen_memory);
    OSScreenSetBufferEx(SCREEN_DRC, (char *)screen_memory + tv_size);
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
#ifdef GEKKO2_WIIU_JIT
    OSInitMutex(&shared_lock);
#endif

    on_log(NULL, "Gekko2 Wii U / Cemu test port (interpreter, software video)");
    on_log(NULL, "Test build: not a release, no speed claim.");
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

    menu = wiiu_menu_create(sd_root[0] ? sd_root : "/vol/external01", "ps2bios.bin");
    if (menu) {
        if (!bios)
            wiiu_menu_set_notice(menu, "BIOS not found: sd:/wiiu/gekko2/bios/ps2bios.bin");
        show_menu();
    }
    g_platform = (g2_platform){ on_log, on_present, keep, now_ms, poll_pad, NULL };
    while (menu && WHBProcIsRunning()) {
        VPADStatus st;
        VPADReadError err = VPAD_READ_SUCCESS;
        uint32_t hold = 0, trigger = 0;
        if (VPADRead(VPAD_CHAN_0, &st, 1, &err) >= 1 && err == VPAD_READ_SUCCESS) {
            hold = st.hold;
            trigger = st.trigger;
        }
        const uint64_t now = now_ms(NULL);
        const unsigned edges = menu_edges(trigger, hold, now);
        if (edges) {
            const int action = wiiu_menu_input(menu, edges);
            show_menu();
            if (action == WM_ACTION_EXIT)
                break;
            if (action == WM_ACTION_START_BIOS || action == WM_ACTION_START_DISC) {
                if (!bios) {
                    wiiu_menu_set_notice(menu, "BIOS not found: sd:/wiiu/gekko2/bios/ps2bios.bin");
                } else {
                    g_config = (g2_config){ bios, action == WM_ACTION_START_DISC ? wiiu_menu_disc(menu) : NULL,
                                            2000u, 0, 1000u, 0, 1 };
                    run_emulation();
                    wiiu_menu_set_notice(menu, "Back from emulation. Restart the app for a clean boot.");
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
    wiiu_menu_destroy(menu);
    VPADShutdown();
    WHBUnmountSdCard();
    OSScreenShutdown();
    MEMFreeToDefaultHeap(screen_memory);
    WHBLogCafeDeinit();
    WHBProcShutdown();
    return 0;
}
