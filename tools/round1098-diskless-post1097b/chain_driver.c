/*
 * Round 1098 (task #887/#536 continuation, post-Round-1097/1097b):
 * re-run the diskless SCPH-50004 boot survey against the tree that
 * now includes Round 1097's unknown-call_sid completion fallback
 * (already independently verified to produce organic PMODE=0x66/
 * DISPFB2/DISPLAY2 in a fresh scratch-tree test) AND Round 1097b's
 * ReferSemaStatus/iReferSemaStatus/iDeleteSema unification - to see
 * where the CURRENT tracked tree's diskless boot actually rests,
 * confirm PMODE/DISP2 state directly (not just in an isolated
 * scratch test), and dump a framebuffer snapshot for a PNG render.
 *
 * Checkpoint-chained (same pattern as Round 939/960) to fit the
 * sandbox's per-call time ceiling. Every chunk checks PMODE/DISPFB2/
 * DISPLAY2 and reports the instant they go non-zero (Round 927/928
 * "coarse-sampling artifact" lesson: report on every observed change,
 * not just at chunk boundaries printed to the user).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/iop/iop_core.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gif.h"
#include "core/hw/vu.h"

static long dump_ppm(const char *path, uint32_t bp, uint32_t bw, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    long non_zero = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t px = gs_mem_read_psmct32(bp, bw ? bw : (uint32_t)w, (uint32_t)x, (uint32_t)y);
            uint8_t r = px & 0xFF, g = (px >> 8) & 0xFF, b = (px >> 16) & 0xFF;
            if (px != 0) non_zero++;
            fputc(r, f); fputc(g, f); fputc(b, f);
        }
    }
    fclose(f);
    return non_zero;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path> <start|continue> [budget] [ppm_out_prefix]\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    const char *ckpt_path = argv[2];
    const char *mode = argv[3];
    uint64_t budget = argc > 4 ? strtoull(argv[4], NULL, 10) : 100000000ull;
    const char *ppm_prefix = argc > 5 ? argv[5] : NULL;

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }

    if (strcmp(mode, "start") == 0) {
        if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
        /* diskless: no iop_cdvd_mount_iso() call, matches main.c's own fallback */
    } else {
        if (checkpoint_load(ckpt_path, &bios, &bios, NULL) != 0) { fprintf(stderr, "checkpoint_load fail\n"); return 1; }
    }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();
    gs_state_t  *gs  = gs_get_state();
    gif_state_t *gif = gif_get_state();
    vu1_state_t *vu1 = vu1_get_state();

    uint64_t chunk = 5000000ull, done = 0;
    uint8_t last_pmode = (uint8_t)gs->pmode;
    uint64_t last_report = 0;
    while (done < budget && !ee->halted) {
        system_run_interleaved(chunk);
        done += chunk;

        if ((uint8_t)gs->pmode != last_pmode || done - last_report >= 20000000ull) {
            fprintf(stderr, "[R1098] instr=%llu ee_pc=0x%08x iop_pc=0x%08x pmode=0x%02x dispfb1=0x%08x dispfb2=0x%08x "
                    "display1=0x%016llx display2=0x%016llx gif_p1=%llu vu1_instr=%llu\n",
                    (unsigned long long)done, ee->pc, iop->pc, (unsigned)gs->pmode,
                    (unsigned)gs->dispfb1, (unsigned)gs->dispfb2,
                    (unsigned long long)gs->display1, (unsigned long long)gs->display2,
                    (unsigned long long)(gif ? gif->gif_path1_transfers : 0),
                    (unsigned long long)(vu1 ? vu1->instructions_executed : 0));
            last_pmode = (uint8_t)gs->pmode;
            last_report = done;
        }
    }

    int tid = ee_hle_thread_get_current_thread_id();
    printf("[R1098-CHAIN] ran %llu more, total_instr=%llu ee_pc=0x%08x halted=%u tid=%d "
           "iop_pc=0x%08x vu1_instr=%llu gif_path1=%llu pmode=0x%02x dispfb1=0x%08x dispfb2=0x%08x "
           "display1=0x%016llx display2=0x%016llx\n",
           (unsigned long long)done, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted, tid,
           iop->pc,
           (unsigned long long)(vu1 ? vu1->instructions_executed : 0),
           (unsigned long long)(gif ? gif->gif_path1_transfers : 0),
           (unsigned)gs->pmode, (unsigned)gs->dispfb1, (unsigned)gs->dispfb2,
           (unsigned long long)gs->display1, (unsigned long long)gs->display2);

    if (ppm_prefix) {
        uint32_t bp1, bw1, bp2, bw2;
        /* DISPFB1/2 layout per gs.c's own gs_decode_dispfb() convention:
         * bits 0-8 = FBP (word address >> 11), bits 9-14 = FBW (in
         * units of 64 pixels) - matches this project's existing
         * Round 730/941 decode already proven correct. */
        bp1 = (uint32_t)(gs->dispfb1 & 0x1FFull) << 11;
        bw1 = (uint32_t)((gs->dispfb1 >> 9) & 0x3Full) * 64u;
        bp2 = (uint32_t)(gs->dispfb2 & 0x1FFull) << 11;
        bw2 = (uint32_t)((gs->dispfb2 >> 9) & 0x3Full) * 64u;

        char path1[512], path2[512];
        snprintf(path1, sizeof(path1), "%s_disp1.ppm", ppm_prefix);
        snprintf(path2, sizeof(path2), "%s_disp2.ppm", ppm_prefix);
        long nz1 = dump_ppm(path1, bp1, bw1, 640, 448);
        long nz2 = dump_ppm(path2, bp2, bw2, 640, 448);
        printf("[R1098-PPM] disp1 bp=0x%x bw=%u nonzero_px=%ld -> %s\n", bp1, bw1, nz1, path1);
        printf("[R1098-PPM] disp2 bp=0x%x bw=%u nonzero_px=%ld -> %s\n", bp2, bw2, nz2, path2);
    }

    if (ee->halted) {
        printf("[R1098-CHAIN] EE halted: %s\n", ee->halt_reason);
        return 0;
    }

    if (checkpoint_save(ckpt_path) != 0) { fprintf(stderr, "checkpoint_save fail\n"); return 1; }
    printf("[R1098-CHAIN] checkpoint saved to %s\n", ckpt_path);
    return 0;
}
