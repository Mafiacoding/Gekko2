/* GPL-3.0-or-later. GX2 probe for the Wii U test port. Two tests, chosen by what is on the SD card:
 *  - <sd>/wiiu/gekko2/present.gsh exists  -> test A: GPU presenter (texture + quad, shaders from present.gsh)
 *  - otherwise                            -> test B: shader-free hardware scaling: a 640x480 TV scan buffer
 *                                            (GX2_TV_RENDER_MODE_STANDARD_480P) that the console scales to the
 *                                            output resolution, written by the CPU.
 * Every step is logged to <sd>/wiiu/gekko2/gx2probe.log (fsync'd) with "about to ..." before risky calls.
 *  - built with the GS shaders embedded (wiiu/tools/build_all.sh with latte-assembler) -> test C first: the GPU
 *    draws GS-style primitives off screen, the CPU reads them back and checks pixels against expected values.
 * Nothing here is verified by the author on Cemu or a Wii U; the log tells us what happens. */
#include "gx2_present.h"
#include "gx2_gs.h"
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *logf;
static void say(const char *fmt, ...)
{
    char b[200];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    if (logf) {
        fprintf(logf, "%s\n", b);
        fflush(logf);
        fsync(fileno(logf));
    }
}

/* Test pattern: colour bars on top, a grey ramp below, a white frame and a box that moves with the frame count. */
static void pattern(uint32_t *fb, unsigned w, unsigned h, unsigned frame)
{
    static const uint32_t bars[8] = { 0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x000000 };
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            uint32_t c;
            if (y < h / 2)
                c = bars[x * 8 / w];
            else {
                const unsigned g = x * 255 / w;
                c = (g << 16) | (g << 8) | g;
            }
            if (x < 4 || y < 4 || x >= w - 4 || y >= h - 4)
                c = 0xffffff;
            fb[(size_t)y * w + x] = c;
        }
    const unsigned bx = (frame * 4) % (w - 64), by = h / 2 - 32 + (unsigned)(frame % 32);
    for (unsigned y = by; y < by + 48 && y < h; y++)
        for (unsigned x = bx; x < bx + 48 && x < w; x++)
            fb[(size_t)y * w + x] = 0xff8000;
}

static void *read_file(const char *path, uint32_t *size)
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

static void test_a(void *gsh, uint32_t size)
{
    static uint32_t fb[640 * 480];
    say("test A: about to gx2_present_init (gsh %u bytes)", (unsigned)size);
    if (!gx2_present_init(gsh, size, 640, 480)) {
        say("test A: gx2_present_init FAILED (shader file rejected, or WHBGfxInit failed)");
        return;
    }
    say("test A: init ok; drawing frames (nearest for 5 s, then bilinear)");
    unsigned frame = 0;
    const uint64_t start = OSTicksToMilliseconds(OSGetSystemTime());
    uint64_t last = start;
    unsigned shown = 0;
    while (WHBProcIsRunning()) {
        pattern(fb, 640, 480, frame++);
        const uint64_t now = OSTicksToMilliseconds(OSGetSystemTime());
        gx2_present_frame(fb, 640, 480, now - start > 5000);
        shown++;
        if (now - last >= 1000) {
            say("test A: %u frames in the last second", shown);
            shown = 0;
            last = now;
        }
    }
    gx2_present_shutdown();
}

static void test_b(void)
{
    uint32_t attribs[] = { GX2_INIT_END };
    say("test B: about to GX2Init");
    GX2Init(attribs);
    uint32_t size = 0, unk = 0;
    GX2CalcTVSize(GX2_TV_RENDER_MODE_STANDARD_480P, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_SINGLE, &size, &unk);
    say("test B: 480p single buffer needs %u bytes (unk=%u)", (unsigned)size, (unsigned)unk);
    void *buf = MEMAllocFromDefaultHeapEx(size, GX2_SCAN_BUFFER_ALIGNMENT);
    if (!buf) {
        say("test B: no memory for the scan buffer");
        return;
    }
    memset(buf, 0, size);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, buf, size);
    say("test B: about to GX2SetTVBuffer 640x480");
    GX2SetTVBuffer(buf, size, GX2_TV_RENDER_MODE_STANDARD_480P, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_SINGLE);
    GX2SetTVEnable(TRUE);
    say("test B: scan buffer set; writing the pattern (RGBA, pitch 640 assumed)");
    static uint32_t fb[640 * 480];
    unsigned frame = 0;
    while (WHBProcIsRunning()) {
        pattern(fb, 640, 480, frame++);
        uint32_t *dst = buf;
        for (unsigned i = 0; i < 640u * 480u && (i + 1) * 4u <= size; i++)
            dst[i] = (fb[i] << 8) | 0xffu;
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, buf, size);
        GX2WaitForVsync();
        if (frame % 120 == 0)
            say("test B: frame %u", frame);
    }
    GX2SetTVEnable(FALSE);
    GX2Shutdown();
    MEMFreeToDefaultHeap(buf);
}

#if __has_include("gs_color_gsh.h") && __has_include("gs_mod_gsh.h") && __has_include("present_gsh_probe.h") && \
    __has_include("gs_zload_gsh.h")
#include "gs_color_gsh.h"
#include "gs_mod_gsh.h"
#include "gs_zload_gsh.h"
#include "present_gsh_probe.h"
#define HAVE_TEST_C 1
static float vz;
static gx2_gs_vertex vtx(float x, float y, uint32_t argb, float u, float v)
{
    /* GS-style window coordinates (0..640, 0..480) to clip space; colour with GS 0x80 = 1.0; depth vz */
    gx2_gs_vertex r = { { x / 320.0f - 1.0f, 1.0f - y / 240.0f, vz, 1.0f },
                        { (float)((argb >> 16) & 0xffu) / 128.0f, (float)((argb >> 8) & 0xffu) / 128.0f,
                          (float)(argb & 0xffu) / 128.0f, (float)(argb >> 24) / 128.0f },
                        { u, v, 0.0f, 1.0f } };
    return r;
}
static void scene(void)
{
    static uint32_t checker[64 * 64];
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++)
            checker[y * 64 + x] = ((x / 8 + y / 8) & 1) ? 0x80ffffffu : 0x80ff0000u;
    gx2_gs_begin(1, 0x80202040u);
    /* Gouraud triangle: red, green, blue corners. */
    gx2_gs_vertex t[3] = { vtx(40, 40, 0x80ff0000u, 0, 0), vtx(280, 40, 0x8000ff00u, 0, 0), vtx(40, 280, 0x800000ffu, 0, 0) };
    gx2_gs_draw(t, 3, GX2_GS_TRIANGLES, 0);
    /* Textured quad (MODULATE with 0x80 = unchanged texels). */
    gx2_gs_set_texture(checker, 64, 64, 1, 0, 0);
    gx2_gs_vertex q[4] = { vtx(360, 40, 0x80808080u, 0, 0), vtx(600, 40, 0x80808080u, 1, 0),
                           vtx(600, 280, 0x80808080u, 1, 1), vtx(360, 280, 0x80808080u, 0, 1) };
    gx2_gs_draw(q, 4, GX2_GS_QUADS, 1);
    /* Same texture slot, same size, new contents in the same frame (Cemu caches textures per frame). */
    static uint32_t green[64 * 64];
    for (unsigned i = 0; i < 64 * 64; i++)
        green[i] = 0x8000ff00u;
    gx2_gs_set_texture(green, 64, 64, 1, 0, 0);
    gx2_gs_vertex g[4] = { vtx(440, 290, 0x80808080u, 0, 0), vtx(520, 290, 0x80808080u, 1, 0),
                           vtx(520, 310, 0x80808080u, 1, 1), vtx(440, 310, 0x80808080u, 0, 1) };
    gx2_gs_draw(g, 4, GX2_GS_QUADS, 1);
    /* Additive: white * FIX 0x40 (0.5) + Cd. */
    gx2_gs_set_blend(GX2_GS_BLEND_ADD_FIX, 0x40);
    gx2_gs_vertex a[4] = { vtx(40, 320, 0x80ffffffu, 0, 0), vtx(280, 320, 0x80ffffffu, 0, 0),
                           vtx(280, 440, 0x80ffffffu, 0, 0), vtx(40, 440, 0x80ffffffu, 0, 0) };
    gx2_gs_draw(a, 4, GX2_GS_QUADS, 0);
    /* Mix: red with As 0x40 (0.5) over the background. */
    gx2_gs_set_blend(GX2_GS_BLEND_MIX_AS, 0x80);
    gx2_gs_vertex m[4] = { vtx(360, 320, 0x40ff0000u, 0, 0), vtx(600, 320, 0x40ff0000u, 0, 0),
                           vtx(600, 440, 0x40ff0000u, 0, 0), vtx(360, 440, 0x40ff0000u, 0, 0) };
    gx2_gs_draw(m, 4, GX2_GS_QUADS, 0);
    gx2_gs_set_blend(GX2_GS_BLEND_OFF, 0x80);
    if (gx2_gs_depth_ready()) {
        /* Depth: blue at 0.75 (always, write), yellow at 0.25 GEQUAL (must fail), white at 0.9 GEQUAL lower half (passes). */
        gx2_gs_depth_clear(0.0f);
        vz = 0.75f;
        gx2_gs_set_depth(7, 1);
        gx2_gs_vertex b[4] = { vtx(300, 300, 0x800000ffu, 0, 0), vtx(340, 300, 0x800000ffu, 0, 0),
                               vtx(340, 460, 0x800000ffu, 0, 0), vtx(300, 460, 0x800000ffu, 0, 0) };
        gx2_gs_draw(b, 4, GX2_GS_QUADS, 0);
        vz = 0.25f;
        gx2_gs_set_depth(6, 0);
        gx2_gs_vertex y[4] = { vtx(300, 300, 0x80ffff00u, 0, 0), vtx(340, 300, 0x80ffff00u, 0, 0),
                               vtx(340, 460, 0x80ffff00u, 0, 0), vtx(300, 460, 0x80ffff00u, 0, 0) };
        gx2_gs_draw(y, 4, GX2_GS_QUADS, 0);
        vz = 0.9f;
        gx2_gs_vertex w[4] = { vtx(300, 380, 0x80ffffffu, 0, 0), vtx(340, 380, 0x80ffffffu, 0, 0),
                               vtx(340, 460, 0x80ffffffu, 0, 0), vtx(300, 460, 0x80ffffffu, 0, 0) };
        gx2_gs_draw(w, 4, GX2_GS_QUADS, 0);
        gx2_gs_set_depth(-1, 0);
        vz = 0.0f;
    }
    /* CPU upload of whole rows into the target (the path used when software-drawn pixels go back to the GPU). */
    unsigned tp = 0;
    uint32_t *up = gx2_gs_target_cpu(640, 480, &tp);
    if (up) {
        for (unsigned y = 470; y < 476; y++)
            for (unsigned x = 0; x < 640; x++)
                up[(size_t)y * tp + x] = 0xff00ff80u;
        gx2_gs_target_ready_rows(470, 476);
    }
}
static int near(uint32_t a, uint32_t b, unsigned tol)
{
    for (unsigned s = 0; s < 24; s += 8) {
        const int d = (int)((a >> s) & 0xffu) - (int)((b >> s) & 0xffu);
        if (d > (int)tol || d < -(int)tol)
            return 0;
    }
    return 1;
}
static void test_c(void)
{
    static uint32_t fb[640 * 480];
    say("test C: about to gx2_present_init (embedded present.gsh)");
    if (!gx2_present_init(present_gsh_probe_data, sizeof present_gsh_probe_data, 640, 480)) {
        say("test C: gx2_present_init FAILED");
        return;
    }
    say("test C: about to gx2_gs_init (embedded gs shaders)");
    if (!gx2_gs_init(gs_color_gsh_data, gs_mod_gsh_data, 640, 480)) {
        say("test C: gx2_gs_init FAILED (shader groups, context state or memory)");
        gx2_present_shutdown();
        return;
    }
    say("test C: about to gx2_gs_init_depth (calibrates depth byte order)");
    gx2_gs_init_depth(gs_zload_gsh_data);
    say("test C: %s", gx2_gs_depth_status());
    say("test C: init ok; drawing the GS test scene on the GPU");
    unsigned frame = 0, shown = 0, checked = 0;
    const uint64_t start = OSTicksToMilliseconds(OSGetSystemTime());
    uint64_t last = start;
    while (WHBProcIsRunning()) {
        const uint64_t t0 = OSTicksToMicroseconds(OSGetSystemTime());
        scene();
        unsigned pitch = 0;
        const uint32_t *px = gx2_gs_end(&pitch);
        const uint64_t t1 = OSTicksToMicroseconds(OSGetSystemTime());
        if (!px) {
            say("test C: gx2_gs_end returned no pixels");
            break;
        }
        for (unsigned y = 0; y < 480; y++)
            for (unsigned x = 0; x < 640; x++)
                fb[y * 640 + x] = px[(size_t)y * pitch + x] >> 8;
        if (!checked) {
            checked = 1;
            static const struct { unsigned x, y; uint32_t want; unsigned tol; const char *what; } chk[] = {
                { 5, 5, 0x202040, 2, "clear colour" },
                { 44, 44, 0xff0000, 24, "triangle red corner" },
                { 272, 44, 0x00ff00, 40, "triangle green corner" },
                { 44, 272, 0x0000ff, 40, "triangle blue corner" },
                { 364, 44, 0xff0000, 2, "checker texel (0,0) red" },
                { 364 + 30, 44, 0xffffff, 2, "checker texel (1,0) white" },
                { 160, 380, 0x9f9fbf, 4, "additive white*0.5 + background" },
                { 480, 380, 0x8f1020, 4, "mix red As=0.5 over background" },
                { 620, 460, 0x202040, 2, "clear colour (corner)" },
                { 480, 300, 0x00ff00, 2, "second texture, same frame, green" },
                { 100, 473, 0xff00ff, 2, "CPU row upload magenta" },
                { 320, 340, 0x0000ff, 2, "depth: yellow behind blue rejected" },
                { 320, 420, 0xffffff, 2, "depth: white in front passes" },
            };
            unsigned ok = 0;
            const unsigned nchk = gx2_gs_depth_ready() ? sizeof chk / sizeof chk[0] : sizeof chk / sizeof chk[0] - 2;
            for (unsigned i = 0; i < nchk; i++) {
                const uint32_t got = fb[chk[i].y * 640 + chk[i].x];
                const int good = near(got, chk[i].want, chk[i].tol);
                ok += good;
                say("test C: pixel (%u,%u) %s = %06x, expected %06x: %s", chk[i].x, chk[i].y, chk[i].what,
                    (unsigned)got, (unsigned)chk[i].want, good ? "OK" : "DIFFERENT");
            }
            say("test C: %u of %u pixel checks OK; GPU scene + readback took %u us", ok, nchk, (unsigned)(t1 - t0));
        }
        gx2_present_frame(fb, 640, 480, 0);
        frame++;
        shown++;
        const uint64_t now = OSTicksToMilliseconds(OSGetSystemTime());
        if (now - last >= 1000) {
            say("test C: %u frames in the last second (GPU scene + readback %u us); draws=%llu waits=%llu", shown,
                (unsigned)(t1 - t0), (unsigned long long)gx2_gs_stat(0), (unsigned long long)gx2_gs_stat(3));
            shown = 0;
            last = now;
        }
    }
    gx2_gs_shutdown();
    gx2_present_shutdown();
}
#endif

int main(void)
{
    WHBProcInit();
    char path[256] = "";
    if (WHBMountSdCard()) {
        snprintf(path, sizeof path, "%s/wiiu/gekko2/gx2probe.log", WHBGetSdCardMountPath());
        logf = fopen(path, "w");
    }
    say("Gekko2 GX2 probe (log: sd:/wiiu/gekko2/gx2probe.log, %s)", logf ? "open" : "not available");
    uint32_t size = 0;
    void *gsh = NULL;
    if (WHBGetSdCardMountPath()) {
        snprintf(path, sizeof path, "%s/wiiu/gekko2/present.gsh", WHBGetSdCardMountPath());
        gsh = read_file(path, &size);
    }
#ifdef HAVE_TEST_C
    if (1) {
        say("GS shaders built in: test C (GPU draws GS primitives, pixels are checked)");
        test_c();
    } else
#endif
    if (gsh) {
        say("present.gsh found (%u bytes): test A", (unsigned)size);
        test_a(gsh, size);
    } else {
        say("no present.gsh on the SD card: test B (shader-free hardware scaling)");
        test_b();
    }
    say("done");
    if (logf)
        fclose(logf);
    WHBUnmountSdCard();
    WHBProcShutdown();
    return 0;
}
