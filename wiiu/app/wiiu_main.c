/* GPL-3.0-or-later. Wii U (wut) front end for the portable Gekko2 run loop. Test port only:
 * interpreter, software framebuffer through OSScreen, no audio, no input, no GX2.
 * Looks for the user's own BIOS at <sd>/wiiu/gekko2/bios/ps2bios.bin. Nothing is bundled. */
#include "g2_app.h"
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
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
#define LINE_CHARS 100

static char lines[LINES][LINE_CHARS];
static unsigned line_count;
static uint32_t shown[1024u * 1024u];
static unsigned shown_w, shown_h;
static void *screen_memory;
static uint32_t tv_size, drc_size;

static void render(void)
{
    OSScreenClearBufferEx(SCREEN_TV, 0x101820ff);
    OSScreenClearBufferEx(SCREEN_DRC, 0x101820ff);
    if (shown_w && shown_h && shown_w <= TV_W && shown_h <= TV_H) {
        const unsigned ox = (TV_W - shown_w) / 2u, oy = (TV_H - shown_h) / 2u;
        for (unsigned y = 0; y < shown_h; y++)
            for (unsigned x = 0; x < shown_w; x++)
                OSScreenPutPixelEx(SCREEN_TV, ox + x, oy + y, shown[(size_t)y * shown_w + x] << 8);
    }
    const unsigned first = line_count > LINES ? line_count - LINES : 0;
    for (unsigned i = first; i < line_count; i++) {
        OSScreenPutFontEx(SCREEN_TV, 0, i - first, lines[i % LINES]);
        OSScreenPutFontEx(SCREEN_DRC, 0, i - first, lines[i % LINES]);
    }
    DCFlushRange(screen_memory, tv_size + drc_size);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

static void on_log(void *u, const char *s)
{
    (void)u;
    snprintf(lines[line_count % LINES], LINE_CHARS, "%s", s);
    line_count++;
    WHBLogPrint(s);
    render();
}

static int on_present(void *u, const uint32_t *p, unsigned w, unsigned h)
{
    (void)u;
    memcpy(shown, p, (size_t)w * h * sizeof shown[0]);
    shown_w = w;
    shown_h = h;
    render();
    return 0;
}

static int keep(void *u) { (void)u; return WHBProcIsRunning(); }
static uint64_t now_ms(void *u) { (void)u; return (uint64_t)OSTicksToMilliseconds(OSGetSystemTime()); }

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
        g2_platform platform = { on_log, on_present, keep, now_ms, NULL };
        g2_config config = { bios, iso[0] && readable(iso) ? iso : NULL, 2000u, 0, 1000u };
        g2_result result;
        g2_run(&config, &platform, &result);
        while (WHBProcIsRunning())
            OSSleepTicks(OSMillisecondsToTicks(100));
    }
    WHBUnmountSdCard();
    OSScreenShutdown();
    MEMFreeToDefaultHeap(screen_memory);
    WHBLogCafeDeinit();
    WHBProcShutdown();
    return 0;
}
