/* Round 1146 - runs the boot against the EXPERIMENTAL forced-FBP-
 * override hack (gif_hacked.c) instead of the real tracked gif.c, to
 * see whether real sprite content becomes visible once GIF's draw
 * target is forced into phase with DISPFB2. NOT a real fix - see
 * gif_hacked.c's own comment for why. Dumps a PNG only if new,
 * non-blank content actually appears. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gif.h"

static void scan(const char *label, uint32_t bp, uint32_t bw, int w, int h, uint32_t *buf_out)
{
    long nz = 0; uint32_t mn = 0xFFFFFFFFu, mx = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t px = gs_mem_read_psmct32(bp, bw, (uint32_t)x, (uint32_t)y);
            if (buf_out) buf_out[y * w + x] = px;
            if (px) nz++;
            if (px < mn) mn = px;
            if (px > mx) mx = px;
        }
    printf("[R1146] %s (bp=%u,bw=%u): non_zero=%ld/%d (%.2f%%) range=[0x%08X,0x%08X]\n",
           label, bp, bw, nz, w*h, 100.0*(double)nz/(double)(w*h), mn, mx);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bios>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "init fail\n"); return 1; }
    ee_state_t *ee = ee_core_get_state();
    gs_state_t *gs = gs_get_state();
    gif_state_t *gif = gif_get_state();

    uint64_t step = 5000000ull, total = 0, cap = 620000000ull;
    while (total < cap && !ee->halted) {
        system_run_interleaved(step);
        total += step;
        fprintf(stderr, "[R1146] instr=%llu fbp=%u ctx1_fbp=%u dispfb2=0x%llX qw=%llu tri=%llu\n",
                (unsigned long long)ee->instructions_executed, gif->fbp, gif->ctx1_fbp,
                (unsigned long long)gs->dispfb2, (unsigned long long)gif->quadwords_seen,
                (unsigned long long)gif->triangles_drawn);
    }

    printf("[R1146] FINAL: instr=%llu pc=0x%08X dispfb2=0x%llX fbp=%u ctx1_fbp=%u qw=%llu tri=%llu unsup=%llu\n",
           (unsigned long long)ee->instructions_executed, ee->pc, (unsigned long long)gs->dispfb2,
           gif->fbp, gif->ctx1_fbp, (unsigned long long)gif->quadwords_seen,
           (unsigned long long)gif->triangles_drawn, (unsigned long long)gif->unsupported_prims_seen);

    uint32_t dfbp = (uint32_t)(gs->dispfb2 & 0x1FFu) * 2048u;
    uint32_t dfbw = (uint32_t)((gs->dispfb2 >> 9) & 0x3Fu) * 64u;
    if (dfbw == 0) dfbw = 640u;

    int w = 640, h = 224;
    uint32_t *pixels = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    scan("CRTC-target(DISPFB2, forced-in-phase)", dfbp, dfbw, w, h, pixels);

    long nz = 0;
    for (int i = 0; i < w * h; i++) if (pixels[i]) nz++;
    if (nz > 0) {
        FILE *f = fopen("/tmp/r1146_forced.ppm", "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int i = 0; i < w * h; i++) {
                uint32_t px = pixels[i];
                uint8_t r = (uint8_t)(px & 0xFF), g = (uint8_t)((px >> 8) & 0xFF), b = (uint8_t)((px >> 16) & 0xFF);
                fputc(r, f); fputc(g, f); fputc(b, f);
            }
            fclose(f);
            printf("[R1146] NEW CONTENT FOUND (%ld non-zero pixels) - PPM written to /tmp/r1146_forced.ppm\n", nz);
        }
    } else {
        printf("[R1146] still blank (0 non-zero pixels) even with forced buffer-phase override.\n");
    }
    free(pixels);
    return 0;
}
