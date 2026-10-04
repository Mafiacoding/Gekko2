/*
 * Round 1144 - direct continuation of Round 1142-1143's finding (real
 * PMODE/DISPFB2 configured + real GIF traffic, but framebuffer blank
 * and CRTC/GIF-draw-target buffer mismatch). This round settles which
 * of the two candidate causes is real:
 *   (a) the VIF1/GIF content isn't a triangle-draw stream at all
 *       (state-only / unsupported PRIM type), or
 *   (b) OSDSYS just hasn't (yet, in the sampled window) issued a
 *       FRAME_1 write pointing the draw target at DISPFB2's buffer.
 *
 * Periodically samples gif_state_t's live PRIM/context/framebuffer
 * fields across a long run (not just one snapshot) to see whether
 * fbp/ctx1_fbp/ctx2_fbp/prim ever change, and what real PRIM type
 * value dominates. Read-only - no patch, no forced write.
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
#include "core/hw/gif.h"

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bios_path>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t  *ee  = ee_core_get_state();
    (void)ee;

    gif_state_t *gif = gif_get_state();
    uint32_t last_prim = 0xFFFFFFFFu, last_fbp = 0xFFFFFFFFu, last_ctx1 = 0xFFFFFFFFu, last_ctx2 = 0xFFFFFFFFu;
    uint64_t last_tri = 0, last_qw = 0, last_unsup = 0;

    /* Sample every 5M instructions out to 150M total - covers the
     * Round 1142/1143 window several times over, with fine enough
     * granularity to catch any FRAME_1/PRIM change events. */
    uint64_t step = 5000000ull, total = 0, cap = 150000000ull;
    int sample_n = 0;
    while (total < cap && !ee_core_get_state()->halted) {
        system_run_interleaved(step);
        total += step;

        uint32_t ptype = gif->prim & 0x7u;
        if (gif->prim != last_prim || gif->fbp != last_fbp || gif->ctx1_fbp != last_ctx1 ||
            gif->ctx2_fbp != last_ctx2 || gif->triangles_drawn != last_tri ||
            gif->unsupported_prims_seen != last_unsup) {
            printf("[R1144] instr=%llu CHANGE: prim=0x%X(type=%u,IIP=%d,TME=%d) fbp=%u ctx1_fbp=%u ctx2_fbp=%u "
                   "qw_seen=%llu(+%llu) tri_drawn=%llu(+%llu) unsupported=%llu(+%llu)\n",
                   (unsigned long long)ee_core_get_state()->instructions_executed,
                   gif->prim, ptype, (gif->prim & 0x8u) ? 1 : 0, (gif->prim & 0x10u) ? 1 : 0,
                   gif->fbp, gif->ctx1_fbp, gif->ctx2_fbp,
                   (unsigned long long)gif->quadwords_seen, (unsigned long long)(gif->quadwords_seen - last_qw),
                   (unsigned long long)gif->triangles_drawn, (unsigned long long)(gif->triangles_drawn - last_tri),
                   (unsigned long long)gif->unsupported_prims_seen, (unsigned long long)(gif->unsupported_prims_seen - last_unsup));
            last_prim = gif->prim; last_fbp = gif->fbp; last_ctx1 = gif->ctx1_fbp; last_ctx2 = gif->ctx2_fbp;
            last_tri = gif->triangles_drawn; last_qw = gif->quadwords_seen; last_unsup = gif->unsupported_prims_seen;
        }
        sample_n++;
        fprintf(stderr, "[R1144] sample %d: instr=%llu qw_seen=%llu\n", sample_n,
                (unsigned long long)ee_core_get_state()->instructions_executed,
                (unsigned long long)gif->quadwords_seen);
    }

    printf("\n[R1144] FINAL: instr=%llu prim=0x%X fbp=%u ctx1_fbp=%u/%u ctx2_fbp=%u/%u "
           "qw_seen=%llu tri_drawn=%llu unsupported_prims=%llu\n",
           (unsigned long long)ee_core_get_state()->instructions_executed,
           gif->prim, gif->fbp, gif->ctx1_fbp, gif->ctx1_fbw, gif->ctx2_fbp, gif->ctx2_fbw,
           (unsigned long long)gif->quadwords_seen, (unsigned long long)gif->triangles_drawn,
           (unsigned long long)gif->unsupported_prims_seen);

    return 0;
}
