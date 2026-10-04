/*
 * Round 1099 (task #930/#887/#447 continuation): fresh GT3 disc-boot
 * from instruction 0 against the tree now including Round 1093/1094's
 * semaphore-array unification and Round 1097b's ReferSemaStatus/
 * iReferSemaStatus/iDeleteSema unification. Deliberately NOT resuming
 * the stale Round-767 checkpoint (predates ~330 rounds of scheduler/
 * semaphore/timer fixes - resuming it would either fail the
 * checkpoint's own byte-size compatibility gate or, worse, silently
 * carry forward pre-fix state that no longer reflects the current
 * tree's real behavior). Same checkpoint-chaining pattern as Round
 * 730/939/1098.
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
#include "core/hw/iop_cdvd.h"

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s <bios_path> <disc_path> <ckpt_path> <start|continue> [budget]\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    const char *disc_path = argv[2];
    const char *ckpt_path = argv[3];
    const char *mode = argv[4];
    uint64_t budget = argc > 5 ? strtoull(argv[5], NULL, 10) : 50000000ull;

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }

    if (strcmp(mode, "start") == 0) {
        if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
        if (iop_cdvd_mount_iso(disc_path) != 0) { fprintf(stderr, "disc mount fail\n"); return 1; }
    } else {
        if (checkpoint_load(ckpt_path, &bios, &bios, disc_path) != 0) { fprintf(stderr, "checkpoint_load fail\n"); return 1; }
    }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();
    gs_state_t  *gs  = gs_get_state();
    gif_state_t *gif = gif_get_state();
    vu1_state_t *vu1 = vu1_get_state();

    uint64_t chunk = 5000000ull, done = 0;
    uint8_t last_pmode = (uint8_t)gs->pmode;
    uint32_t last_ee_pc = ee->pc;
    uint64_t stall_count = 0;
    while (done < budget && !ee->halted) {
        system_run_interleaved(chunk);
        done += chunk;

        if ((uint8_t)gs->pmode != last_pmode) {
            fprintf(stderr, "[R1099-MILESTONE] PMODE changed at instr=%llu: 0x%02x -> 0x%02x\n",
                    (unsigned long long)done, last_pmode, (unsigned)gs->pmode);
            last_pmode = (uint8_t)gs->pmode;
        }
        if (ee->pc == last_ee_pc) stall_count++; else { stall_count = 0; last_ee_pc = ee->pc; }
    }

    int tid = ee_hle_thread_get_current_thread_id();
    printf("[R1099-CHAIN] ran %llu more, total_instr=%llu ee_pc=0x%08x halted=%u tid=%d "
           "iop_pc=0x%08x vu1_instr=%llu gif_path1=%llu pmode=0x%02x dispfb1=0x%08x dispfb2=0x%08x "
           "display1=0x%016llx display2=0x%016llx ee_pc_stable_chunks=%llu\n",
           (unsigned long long)done, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted, tid,
           iop->pc,
           (unsigned long long)(vu1 ? vu1->instructions_executed : 0),
           (unsigned long long)(gif ? gif->gif_path1_transfers : 0),
           (unsigned)gs->pmode, (unsigned)gs->dispfb1, (unsigned)gs->dispfb2,
           (unsigned long long)gs->display1, (unsigned long long)gs->display2,
           (unsigned long long)stall_count);

    if (ee->halted) {
        printf("[R1099-CHAIN] EE halted: %s\n", ee->halt_reason);
        return 0;
    }

    if (checkpoint_save(ckpt_path) != 0) { fprintf(stderr, "checkpoint_save fail\n"); return 1; }
    printf("[R1099-CHAIN] checkpoint saved to %s\n", ckpt_path);
    return 0;
}
