/* Round 1145b - simplified: warm to ~800M instructions (past Round
 * 1144's observed fbp/DISPFB2 convergence window), then do exactly
 * Round 1143's pixel-scan methodology against whatever DISPFB2
 * currently targets, plus GIF's own current draw target - no
 * equality gating (double-buffering means they're legitimately out
 * of phase most of the time). Read-only. */
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

static void scan(const char *label, uint32_t bp, uint32_t bw, int w, int h)
{
    long nz = 0; uint32_t mn = 0xFFFFFFFFu, mx = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t px = gs_mem_read_psmct32(bp, bw, (uint32_t)x, (uint32_t)y);
            if (px) nz++;
            if (px < mn) mn = px;
            if (px > mx) mx = px;
        }
    printf("[R1145b] %s (bp=%u,bw=%u): non_zero=%ld/%d (%.2f%%) range=[0x%08X,0x%08X]\n",
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

    uint64_t step = 5000000ull, total = 0, cap = 650000000ull;
    while (total < cap && !ee->halted) {
        system_run_interleaved(step);
        total += step;
        fprintf(stderr, "[R1145b] instr=%llu fbp=%u ctx1_fbp=%u dispfb2=0x%llX qw=%llu\n",
                (unsigned long long)ee->instructions_executed, gif->fbp, gif->ctx1_fbp,
                (unsigned long long)gs->dispfb2, (unsigned long long)gif->quadwords_seen);
    }

    printf("[R1145b] FINAL: instr=%llu pc=0x%08X dispfb2=0x%llX fbp=%u ctx1_fbp=%u ctx2_fbp=%u qw=%llu tri=%llu unsup=%llu\n",
           (unsigned long long)ee->instructions_executed, ee->pc, (unsigned long long)gs->dispfb2,
           gif->fbp, gif->ctx1_fbp, gif->ctx2_fbp, (unsigned long long)gif->quadwords_seen,
           (unsigned long long)gif->triangles_drawn, (unsigned long long)gif->unsupported_prims_seen);

    uint32_t dfbp = (uint32_t)(gs->dispfb2 & 0x1FFu) * 2048u;
    uint32_t dfbw = (uint32_t)((gs->dispfb2 >> 9) & 0x3Fu) * 64u;
    if (dfbw == 0) dfbw = 640u;
    scan("CRTC-target(DISPFB2)", dfbp, dfbw, 640, 224);
    uint32_t gfbw = gif->fbw ? gif->fbw : 640u;
    scan("GIF-own-target", gif->fbp, gfbw, 640, 224);
    uint32_t c1fbw = gif->ctx1_fbw ? gif->ctx1_fbw : 640u;
    scan("ctx1_fbp-target", gif->ctx1_fbp, c1fbw, 640, 224);
    return 0;
}
