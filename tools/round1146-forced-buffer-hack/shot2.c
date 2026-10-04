/* Round 1146b - same forced-buffer-phase experiment as shot.c, but
 * scans+dumps AS SOON AS quadwords_seen plateaus (2 consecutive
 * unchanged, non-zero samples) instead of waiting for a fixed
 * instruction cap that the sandbox wall-clock never lets us reach.
 * Content stops growing well before the cap anyway (established in
 * Round 1144/1145), so this captures the decisive scan within budget. */
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
    printf("[R1146b] %s (bp=%u,bw=%u): non_zero=%ld/%d (%.2f%%) range=[0x%08X,0x%08X]\n",
           label, bp, bw, nz, w*h, 100.0*(double)nz/(double)(w*h), mn, mx);
}

static void dump_ppm(const char *path, uint32_t *pixels, int w, int h)
{
    long nz = 0;
    for (int i = 0; i < w * h; i++) if (pixels[i]) nz++;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t px = pixels[i];
        uint8_t r = (uint8_t)(px & 0xFF), g = (uint8_t)((px >> 8) & 0xFF), b = (uint8_t)((px >> 16) & 0xFF);
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
    printf("[R1146b] wrote %s (%ld non-zero pixels / %d)\n", path, nz, w*h);
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

    int w = 640, h = 224;
    uint32_t *pixels = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));

    uint64_t step = 5000000ull, total = 0, cap = 900000000ull; uint64_t target_instr = 800000000ull;
    uint64_t last_qw = 0xFFFFFFFFFFFFFFFFull;
    int stable_count = 0;
    while (total < cap && !ee->halted) {
        system_run_interleaved(step);
        total += step;
        uint64_t qw = gif->quadwords_seen;
        fprintf(stderr, "[R1146b] instr=%llu fbp=%u ctx1_fbp=%u dispfb2=0x%llX qw=%llu tri=%llu\n",
                (unsigned long long)ee->instructions_executed, gif->fbp, gif->ctx1_fbp,
                (unsigned long long)gs->dispfb2, (unsigned long long)qw,
                (unsigned long long)gif->triangles_drawn);

        if (qw != 0 && qw == last_qw) {
            stable_count++;
        } else {
            stable_count = 0;
        }
        last_qw = qw;

        if (stable_count >= 1) {
            printf("[R1146b] quadwords_seen plateaued at %llu (instr=%llu) - scanning now.\n",
                   (unsigned long long)qw, (unsigned long long)ee->instructions_executed);
            break;
        }
        if (ee->instructions_executed >= target_instr) {
            printf("[R1146b] reached target_instr=%llu (instr=%llu, qw=%llu) - scanning now.\n",
                   (unsigned long long)target_instr, (unsigned long long)ee->instructions_executed,
                   (unsigned long long)qw);
            break;
        }
    }

    printf("[R1146b] FINAL: instr=%llu pc=0x%08X dispfb2=0x%llX fbp=%u ctx1_fbp=%u qw=%llu tri=%llu unsup=%llu\n",
           (unsigned long long)ee->instructions_executed, ee->pc, (unsigned long long)gs->dispfb2,
           gif->fbp, gif->ctx1_fbp, (unsigned long long)gif->quadwords_seen,
           (unsigned long long)gif->triangles_drawn, (unsigned long long)gif->unsupported_prims_seen);

    uint32_t dfbp = (uint32_t)(gs->dispfb2 & 0x1FFu) * 2048u;
    uint32_t dfbw = (uint32_t)((gs->dispfb2 >> 9) & 0x3Fu) * 64u;
    if (dfbw == 0) dfbw = 640u;
    scan("CRTC-target(DISPFB2, forced-in-phase)", dfbp, dfbw, w, h, pixels);
    dump_ppm("/tmp/r1146b_forced.ppm", pixels, w, h);

    free(pixels);
    return 0;
}
