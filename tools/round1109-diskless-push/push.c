/*
 * Round 1109 (task #1033 follow-up, user-directed): push the SCPH-50004
 * diskless boot far past Round 1108's +176M-instruction extended window
 * (which proved real forward progress resumes given enough real time,
 * correcting Round 1107's premature "never" conclusion), and capture
 * whatever real picture content exists in GS local memory at DISPFB2's
 * real configured target - mirroring the established Round 450/588/717
 * PPM-dump convention (gs_mem_read_psmct32 at fbp/fbw, PSMCT32 640x224).
 *
 * Loads the tracked r1103 checkpoint (SCPH-50004 diskless boot,
 * 267,354,194 instructions already executed, real BIOS, no disc), then
 * runs forward in large chunks, printing PMODE/DISPFB2/GIF-quadword/
 * triangle progress and dumping an intermediate PPM after every chunk
 * so a stopped run still leaves a usable image, not just a final one.
 *
 * Read-only diagnostic. No BIOS/checkpoint content committed to the
 * tree - paths are CLI args, per the project's leak-check discipline.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/checkpoint.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gif.h"

static void dump_ppm(const char *out_path, uint32_t fbp, uint32_t fbw)
{
    int W = 640, H = 224;
    FILE *f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "[R1109] failed to open %s for write\n", out_path); return; }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    long non_bg = 0;
    for (int yy = 0; yy < H; yy++) {
        for (int xx = 0; xx < W; xx++) {
            uint32_t px = gs_mem_read_psmct32(fbp, fbw, (uint32_t)xx, (uint32_t)yy);
            uint8_t r = px & 0xFF, g = (px >> 8) & 0xFF, b = (px >> 16) & 0xFF;
            if (px != 0) non_bg++;
            fputc(r, f); fputc(g, f); fputc(b, f);
        }
    }
    fclose(f);
    fprintf(stderr, "[R1109] dumped %s: non_zero_pixels=%ld / %d\n", out_path, non_bg, W * H);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path> <out_ppm> [chunk_slices] [n_chunks] [out_ckpt]\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    const char *ckpt_path = argv[2];
    const char *out_path = argv[3];
    uint64_t chunk = (argc >= 5) ? strtoull(argv[4], NULL, 10) : 20000000ull;
    int n_chunks = (argc >= 6) ? atoi(argv[5]) : 10;
    const char *out_ckpt = (argc >= 7) ? argv[6] : NULL;

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(ckpt_path, &bios, &bios, NULL) != 0) {
        fprintf(stderr, "checkpoint_load FAILED for %s\n", ckpt_path);
        return 1;
    }

    ee_state_t *ee = ee_core_get_state();
    gif_state_t *gif = gif_get_state();
    gs_state_t *gs = gs_get_state();

    fprintf(stderr, "[R1109] loaded %s: total_instr=%llu pc=0x%08x\n",
            ckpt_path, (unsigned long long)ee->instructions_executed, ee->pc);

    for (int i = 0; i < n_chunks; i++) {
        system_run_interleaved(chunk);
        uint32_t fbp = gif->fbp, fbw = gif->fbw ? gif->fbw : 640u;
        fprintf(stderr, "[R1109] chunk %d/%d: total_instr=%llu pc=0x%08x halted=%d "
                "PMODE=0x%llx DISPFB1=0x%llx DISPFB2=0x%llx DISPLAY2=0x%llx "
                "fbp=%u fbw=%u quadwords_seen=%llu triangles_drawn=%llu\n",
                i + 1, n_chunks,
                (unsigned long long)ee->instructions_executed, ee->pc, ee->halted,
                (unsigned long long)gs->pmode, (unsigned long long)gs->dispfb1,
                (unsigned long long)gs->dispfb2, (unsigned long long)gs->display2,
                fbp, fbw,
                (unsigned long long)gif->quadwords_seen,
                (unsigned long long)gif->triangles_drawn);
        dump_ppm(out_path, fbp, fbw);
        if (out_ckpt) {
            if (checkpoint_save(out_ckpt) == 0) {
                fprintf(stderr, "[R1109] saved checkpoint to %s at total_instr=%llu\n",
                        out_ckpt, (unsigned long long)ee->instructions_executed);
            } else {
                fprintf(stderr, "[R1109] checkpoint_save FAILED for %s\n", out_ckpt);
            }
        }
        if (ee->halted) {
            fprintf(stderr, "[R1109] EE halted, stopping early\n");
            break;
        }
    }

    return 0;
}
