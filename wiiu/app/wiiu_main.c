/* GPL-3.0-or-later. Wii U (wut) front end for the portable Gekko2 run loop. Test port only:
 * interpreter, software framebuffer through OSScreen, GamePad input, no audio, no GX2.
 * Looks for the user's own BIOS at <sd>/wiiu/gekko2/bios/ps2bios.bin. Nothing is bundled. */
#include "g2_app.h"
#include "core/hw/iop_sio2.h"
#include <coreinit/cache.h>
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

static int dirty = 1;
static uint64_t last_render_ms;

static void render(void)
{
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
}

static void on_log(void *u, const char *s)
{
    (void)u;
    snprintf(lines[line_count % LINES], LINE_CHARS, "%s", s);
    line_count++;
    WHBLogPrint(s);
    dirty = 1;
}

static int on_present(void *u, const uint32_t *p, unsigned w, unsigned h)
{
    (void)u;
    memcpy(shown, p, (size_t)w * h * sizeof shown[0]);
    shown_w = w;
    shown_h = h;
    dirty = 1;
    return 0;
}

static int keep(void *u)
{
    (void)u;
    if (!WHBProcIsRunning())
        return 0;
    const uint64_t now = (uint64_t)OSTicksToMilliseconds(OSGetSystemTime());
    if (dirty && now - last_render_ms >= 500u) {
        last_render_ms = now;
        render();
    }
    return 1;
}
static uint64_t now_ms(void *u) { (void)u; return (uint64_t)OSTicksToMilliseconds(OSGetSystemTime()); }


/* Wii U GamePad to PS2 pad. Nintendo layout: B = Cross, A = Circle, Y = Square, X = Triangle,
 * L/R = L1/R1, ZL/ZR = L2/R2, + = Start, - = Select, stick clicks = L3/R3. HOME is left to the OS. */
static int poll_pad(void *u, uint16_t *pressed, uint8_t axes[4])
{
    (void)u;
    VPADStatus st;
    VPADReadError err = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &st, 1, &err) < 1 || err != VPAD_READ_SUCCESS)
        return 1;
    const uint32_t h = st.hold;
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

static int readable(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
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

    on_log(NULL, "Gekko2 Wii U / Cemu test port (interpreter, software video)");
    on_log(NULL, "Test build: not a release, no speed claim.");
    char mount_bios[256] = "", fallback_bios[256] = "fs:/vol/external01/wiiu/gekko2/bios/ps2bios.bin";
    char plain_bios[256] = "/vol/external01/wiiu/gekko2/bios/ps2bios.bin", iso[256] = "";
    if (WHBMountSdCard()) {
        const char *root = WHBGetSdCardMountPath();
        if (root) {
            snprintf(mount_bios, sizeof mount_bios, "%s/wiiu/gekko2/bios/ps2bios.bin", root);
            snprintf(iso, sizeof iso, "%s/wiiu/gekko2/game.iso", root);
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
    if (!bios) {
        on_log(NULL, "Put your own PS2 BIOS at sd:/wiiu/gekko2/bios/ps2bios.bin (4 MiB dump).");
        while (WHBProcIsRunning())
            OSSleepTicks(OSMillisecondsToTicks(100));
    } else {
        g2_platform platform = { on_log, on_present, keep, now_ms, poll_pad, NULL };
        g2_config config = { bios, iso[0] && readable(iso) ? iso : NULL, 2000u, 0, 1000u };
        g2_result result;
        g2_run(&config, &platform, &result);
        while (WHBProcIsRunning())
            OSSleepTicks(OSMillisecondsToTicks(100));
    }
    VPADShutdown();
    WHBUnmountSdCard();
    OSScreenShutdown();
    MEMFreeToDefaultHeap(screen_memory);
    WHBLogCafeDeinit();
    WHBProcShutdown();
    return 0;
}
