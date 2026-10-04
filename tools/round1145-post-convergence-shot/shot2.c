/*
 * Round 1145 - direct correction of Round 1143's timing. Round 1144
 * found that gif->fbp/ctx1_fbp only converges to word-base=163840
 * (matching DISPFB2's real target) at instr~756M - AFTER Round 1143's
 * pixel scan, which was taken at instr=707M (a stale, PRE-convergence
 * snapshot). Round 1144 also found sprite content (PRIM type=6) is
 * the real draw stream, and confirmed via source read that
 * rasterize_sprite() in gif.c IS a real, complete implementation
 * (mipmap/texture/Z/fog aware) - so "missing sprite rasterizer" is
 * NOT the bug. This round re-runs Round 1143's exact pixel-scan
 * methodology, but warmed up past the real convergence point, to
 * settle: once GIF's own draw target genuinely matches what DISPFB2
 * scans out, does real pixel content appear, or does a genuine bug
 * remain even then?
 *
 * Read-only. No patch/skip/forced-write.
 */
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

static void scan_and_report(const char *label, uint32_t bp, uint32_t bw, int w, int h)
{
    long non_zero = 0;
    uint32_t min_val = 0xFFFFFFFFu, max_val = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t px = gs_mem_read_psmct32(bp, bw, (uint32_t)x, (uint32_t)y);
            if (px != 0) non_zero++;
            if (px < min_val) min_val = px;
            if (px > max_val) max_val = px;
        }
    }
    printf("[R1145] %s scan (bp=%u,bw=%u,%dx%d): non_zero=%ld/%d (%.2f%%) range=[0x%08X,0x%08X]\n",
           label, bp, bw, w, h, non_zero, w * h, 100.0 * (double)non_zero / (double)(w * h), min_val, max_val);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bios_path>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t  *ee  = ee_core_get_state();

    uint64_t step = 5000000ull, total = 0, cap = 160000000ull;
    gs_state_t *gs = gs_get_state();
    gif_state_t *gif = gif_get_state();

    while (total < cap && !ee->halted) {
        system_run_interleaved(step);
        total += step;
        fprintf(stderr, "[R1145] instr=%llu fbp=%u ctx1_fbp=%u dispfb2=0x%llX\n",
                (unsigned long long)ee->instructions_executed, gif->fbp, gif->ctx1_fbp,
                (unsigned long long)gs->dispfb2);

        uint32_t dfbp = (uint32_t)(gs->dispfb2 & 0x1FFu) * 2048u;
        uint32_t dfbw = (uint32_t)((gs->dispfb2 >> 9) & 0x3Fu) * 64u;
        if (dfbw == 0) dfbw = 640u;

        if (gif->fbp == dfbp && gif->fbp != 0) {
            printf("[R1145] instr=%llu: CONVERGED (gif->fbp==DISPFB2 word-base=%u) - scanning now\n",
                   (unsigned long long)ee->instructions_executed, gif->fbp);
            scan_and_report("CRTC-target(DISPFB2)", dfbp, dfbw, 640, 224);
            uint32_t gfbw = gif->fbw ? gif->fbw : 640u;
            scan_and_report("GIF-own-target", gif->fbp, gfbw, 640, 224);
            printf("[R1145] gif state: prim=0x%X qw_seen=%llu tri_drawn=%llu unsupported=%llu\n",
                   gif->prim, (unsigned long long)gif->quadwords_seen,
                   (unsigned long long)gif->triangles_drawn, (unsigned long long)gif->unsupported_prims_seen);
        }
    }

    printf("[R1145] FINAL: instr=%llu pc=0x%08X halted=%d dispfb2=0x%llX fbp=%u ctx1_fbp=%u ctx2_fbp=%u\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted,
           (unsigned long long)gs->dispfb2, gif->fbp, gif->ctx1_fbp, gif->ctx2_fbp);

    {
        uint32_t dfbp = (uint32_t)(gs->dispfb2 & 0x1FFu) * 2048u;
        uint32_t dfbw = (uint32_t)((gs->dispfb2 >> 9) & 0x3Fu) * 64u;
        if (dfbw == 0) dfbw = 640u;
        scan_and_report("FINAL CRTC-target(DISPFB2)", dfbp, dfbw, 640, 224);
        uint32_t gfbw = gif->fbw ? gif->fbw : 640u;
        scan_and_report("FINAL GIF-own-target", gif->fbp, gfbw, 640, 224);
    }

    return 0;
}
