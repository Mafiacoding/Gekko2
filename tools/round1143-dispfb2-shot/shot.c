/*
 * Round 1143 - direct follow-up to Round 1142's finding that PMODE=0x66
 * (EN2 set, circuit 2 active) and DISPFB2 are ALREADY genuinely
 * configured and actively updated (0x1400 -> 0x1450, real
 * double-buffering) during the diskless SCPH-50004 steady state, via
 * real writes at pc=0x0026CF10/0x0026CF20 plus (per Round 1141's real
 * VIF1/GIF DMA evidence) further updates riding on GIF-embedded
 * register writes that a plain EE-store instrumentation can't see.
 *
 * The open question this round answers: given PMODE/DISPFB2/DISPLAY2
 * are for real configured and changing, does the actual GS memory
 * content at that framebuffer address contain a real picture (matching
 * the confirmed VIF1/GIF DMA traffic), or is it still blank/garbage?
 * This is the concrete, decisive test of "are we closer to a visible
 * menu" - not more register-level inference, but reading the actual
 * pixels real hardware would scan out.
 *
 * Read-only. No patch/skip/forced-write - this driver only reads
 * gs_state_t and gs_mem, never writes either.
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

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bios_path>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();

    /* Warm up well past Round 1142's window so DISPFB2 has had time to
     * settle into its real steady-state double-buffer cadence. */
    uint64_t warm = 0, warm_step = 5000000ull, warm_cap = 115000000ull;
    while (warm < warm_cap && !ee->halted) {
        system_run_interleaved(warm_step);
        warm += warm_step;
        fprintf(stderr, "[R1143] warmup instr=%llu\n", (unsigned long long)warm);
    }
    printf("[R1143] warmup done: instr_count=%llu pc=0x%08X\n",
           (unsigned long long)ee->instructions_executed, ee->pc);

    gs_state_t *gs = gs_get_state();
    uint64_t dispfb2 = gs->dispfb2;
    uint32_t fbp = (uint32_t)(dispfb2 & 0x1FFu);          /* FBP: bits 0-8, unit=2048 words */
    uint32_t fbw = (uint32_t)((dispfb2 >> 9) & 0x3Fu) * 64u; /* FBW: bits 9-14, unit=64 px */
    uint32_t psm = (uint32_t)((dispfb2 >> 15) & 0x1Fu);
    uint32_t dbx = (uint32_t)((dispfb2 >> 32) & 0x7FFu);
    uint32_t dby = (uint32_t)((dispfb2 >> 43) & 0x7FFu);
    if (fbw == 0) fbw = 640u;

    printf("[R1143] pmode=0x%llX dispfb2=0x%llX -> decoded FBP=%u (word-base=%u) FBW=%u PSM=%u DBX=%u DBY=%u\n",
           (unsigned long long)gs->pmode, (unsigned long long)dispfb2, fbp, fbp * 2048u, fbw, psm, dbx, dby);
    printf("[R1143] display2=0x%llX csr=0x%llX imr=0x%llX\n",
           (unsigned long long)gs->display2, (unsigned long long)gs->csr, (unsigned long long)gs->imr);

    /* Dump a 640x224 window from the decoded DISPFB2 framebuffer base
     * (standard PS2 NTSC field-mode visible resolution), same
     * convention as Round 450/588's own screenshot tools. */
    int w = 640, h = 224;
    long non_zero = 0;
    uint32_t min_val = 0xFFFFFFFFu, max_val = 0;
    FILE *f = fopen("/tmp/r1143_dispfb2.ppm", "wb");
    if (f) fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t px = gs_mem_read_psmct32(fbp * 2048u, fbw, (uint32_t)x, (uint32_t)y);
            if (px != 0) non_zero++;
            if (px < min_val) min_val = px;
            if (px > max_val) max_val = px;
            if (f) {
                uint8_t r = (uint8_t)(px & 0xFF), g = (uint8_t)((px >> 8) & 0xFF), b = (uint8_t)((px >> 16) & 0xFF);
                fputc(r, f); fputc(g, f); fputc(b, f);
            }
        }
    }
    if (f) fclose(f);

    printf("[R1143] DISPFB2 framebuffer scan: %dx%d window, non_zero_pixels=%ld / %d (%.2f%%), value_range=[0x%08X,0x%08X]\n",
           w, h, non_zero, w * h, 100.0 * (double)non_zero / (double)(w * h), min_val, max_val);

    /* Also sample a handful of representative pixels for a quick,
     * concrete look without needing to open the PPM. */
    printf("[R1143] sample pixels:\n");
    int sx[] = {0, 100, 200, 320, 400, 500, 600, 320, 320};
    int sy[] = {0, 50,  100, 112, 150, 180, 220, 0,   223};
    for (int i = 0; i < 9; i++) {
        uint32_t px = gs_mem_read_psmct32(fbp * 2048u, fbw, (uint32_t)sx[i], (uint32_t)sy[i]);
        printf("  (%3d,%3d) = 0x%08X\n", sx[i], sy[i], px);
    }

    gif_state_t *gif = gif_get_state();
    printf("[R1143] gif.c own tracked draw-target state: fbp=%u (word-base=%u) fbw=%u quadwords_seen=%llu triangles_drawn=%llu\n",
           gif->fbp, gif->fbp, gif->fbw, (unsigned long long)gif->quadwords_seen, (unsigned long long)gif->triangles_drawn);
    printf("[R1143] comparison: CRTC DISPFB2 scans out word-base=%u, GIF pipeline last drew to word-base=%u -> %s\n",
           fbp * 2048u, gif->fbp, (fbp * 2048u == gif->fbp) ? "SAME BUFFER" : "DIFFERENT BUFFERS (mismatch)");

    /* Also scan the buffer gif.c itself last drew to, in case it
     * differs from what DISPFB2 is scanning out. */
    {
        uint32_t gfbw = gif->fbw ? gif->fbw : 640u;
        long gnz = 0;
        for (int y = 0; y < 224; y++)
            for (int x = 0; x < 640; x++)
                if (gs_mem_read_psmct32(gif->fbp, gfbw, (uint32_t)x, (uint32_t)y) != 0) gnz++;
        printf("[R1143] scan of GIF's own draw-target buffer (bp=%u,bw=%u): non_zero_pixels=%ld / 143360\n",
               gif->fbp, gfbw, gnz);
    }

    printf("[R1143] final state: instr_count=%llu pc=0x%08X halted=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
    return 0;
}
