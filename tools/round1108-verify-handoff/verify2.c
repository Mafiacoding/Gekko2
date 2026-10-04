/* Round 1108 continued: extended-window re-verification. Round 1107
 * stopped after +70M instructions and saw zero signals for semids
 * with wait_id 4/7/8/9/10/11/13. verify.c (this round) found that a
 * longer window (+176M instructions) DOES show sema6-13 signaled
 * 48-49 times each, and EE pc moved from 0x00257964 to 0x00264978 -
 * real forward progress. This tool re-confirms that result and
 * additionally dumps EE HLE thread status + GS PMODE to characterize
 * how far the boot now gets, and to check if it's genuinely
 * progressing organically vs. some other artifact.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/checkpoint.h"
#include "core/hw/gs.h"

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <bios_path> <ckpt_path> [slices]\n", argv[0]); return 1; }
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(argv[2], &bios, &bios, NULL) != 0) { fprintf(stderr, "checkpoint_load FAILED\n"); return 1; }

    ee_state_t *ee = ee_core_get_state();
    int slices = (argc >= 4) ? atoi(argv[3]) : 30000000;
    fprintf(stderr, "[R1108b] start: total_instr=%llu pc=0x%08x\n",
            (unsigned long long)ee->instructions_executed, ee->pc);

    /* step in 5M-slice increments, reporting thread wait status + pc each time,
     * to find roughly when/if progress happens */
    int steps = slices / 5000000;
    if (steps < 1) steps = 1;
    for (int i = 0; i < steps; i++) {
        system_run_interleaved(5000000);
        fprintf(stderr, "[R1108b] after +%d*5M slices: total_instr=%llu pc=0x%08x halted=%d\n",
                i+1, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
    }

    fprintf(stderr, "[R1108b] final signal_calls:\n");
    int semids[] = {0,4,5,6,7,8,9,10,11,13};
    for (int i = 0; i < 10; i++) {
        fprintf(stderr, "  sema[%2d] signal_calls=%llu\n", semids[i],
                (unsigned long long)ee_hle_thread_get_signal_calls(semids[i]));
    }

    fprintf(stderr, "[R1108b] EE HLE thread table:\n");
    for (int tid = 0; tid < 16; tid++) {
        int status = ee_hle_thread_get_status(tid);
        if (status == 0) continue;
        fprintf(stderr, "  tid=%d status=%d wait_type=%d wait_id=%d\n",
                tid, status,
                ee_hle_thread_get_wait_type(tid),
                ee_hle_thread_get_wait_id(tid));
    }

    return 0;
}
