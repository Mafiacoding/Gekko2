/*
 * Round 1108 (task #1033 continuation): independent verification of
 * an uploaded "handoff_notes.md" claim from a separate/parallel Claude
 * session ("Claude cloud"), per the user's explicit instruction to
 * check whether it actually resolves task #1033's disc/pad-events
 * open question, and fix it ourselves if not.
 *
 * The handoff notes claim a real VBLANK_START handler at EE
 * 0x00208088-0x00208190 gates iSignalSema(sema7) behind
 * iPollSema(sema6) at pc=0x00208110/0x00208134, and that sema6 has
 * ee_hle_thread_get_signal_calls(6)==0 across the whole boot.
 *
 * This tool independently checks, against THIS tree's own r1103
 * checkpoint (SCPH-50004 diskless boot, the same checkpoint Round
 * 1107 used): (1) what real code actually lives at EE
 * 0x00208088-0x00208190 in this tree's BIOS/checkpoint state; (2) the
 * real signal_calls count for semids 4/6/7/8/9/10/11/13 - but critically,
 * AFTER stepping the emulator forward (per Round 1107's own
 * self-caught methodology fix and the pre-existing Round ~1071
 * "signal_calls is not checkpointed, reads 0 right after
 * checkpoint_load() regardless of true history" finding) rather than
 * sampling immediately after checkpoint_load(), to avoid reproducing
 * that exact known false-negative trap.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/checkpoint.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path>\n", argv[0]);
        return 1;
    }
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(argv[2], &bios, &bios, NULL) != 0) {
        fprintf(stderr, "checkpoint_load FAILED\n");
        return 1;
    }

    ee_state_t *ee = ee_core_get_state();
    fprintf(stderr, "[R1108] loaded ckpt: total_instr=%llu pc=0x%08x halted=%d\n",
            (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    fprintf(stderr, "[R1108] raw words at claimed handler range 0x00208088-0x00208190:\n");
    for (uint32_t a = 0x00208080; a <= 0x00208190; a += 4) {
        uint32_t w = ee_mem_read32(ee, a);
        fprintf(stderr, "  [0x%08x] = 0x%08x\n", a, w);
    }

    fprintf(stderr, "[R1108] signal_calls BEFORE stepping (expected ~0 regardless of history, per Round ~1071 known artifact):\n");
    int semids[] = {0,4,5,6,7,8,9,10,11,13};
    for (int i = 0; i < 10; i++) {
        fprintf(stderr, "  sema[%2d] signal_calls=%llu\n", semids[i],
                (unsigned long long)ee_hle_thread_get_signal_calls(semids[i]));
    }

    uint64_t before = ee->instructions_executed;
    int slices = (argc >= 4) ? atoi(argv[3]) : 50000000;
    fprintf(stderr, "[R1108] stepping %d slices forward...\n", slices);
    system_run_interleaved((uint64_t)slices);
    fprintf(stderr, "[R1108] after stepping: total_instr=%llu (+%llu) pc=0x%08x halted=%d\n",
            (unsigned long long)ee->instructions_executed,
            (unsigned long long)(ee->instructions_executed - before),
            ee->pc, ee->halted);

    fprintf(stderr, "[R1108] signal_calls AFTER stepping (this is the real evidence):\n");
    for (int i = 0; i < 10; i++) {
        fprintf(stderr, "  sema[%2d] signal_calls=%llu\n", semids[i],
                (unsigned long long)ee_hle_thread_get_signal_calls(semids[i]));
    }

    return 0;
}
