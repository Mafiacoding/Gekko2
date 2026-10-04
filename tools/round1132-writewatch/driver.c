/*
 * Round 1132 (task #1043, follow-up to Round 1131's explicitly-named
 * "single missing observation"): full-boot write-watch on EE RAM words
 * 0x80023FC8 / 0x80023FCC (the two SIF2-kick gate/"busy" flags Round
 * 1131 proved are read but never written by either real kick-dispatcher
 * function itself). Must be compiled with -DR1132_WRITE_WATCH so the
 * hooks placed in ee_core.c's ee_mem_write8/16/32/64 and dma.c's
 * dma_channel_receive_quadwords() are active. This driver just runs a
 * full cold boot to (at least) the Round-1130 baseline instruction
 * count in coarse slices (for wall-clock-timeout resilience) while
 * [R1132_WRITE_WATCH] lines are emitted to stderr by the instrumented
 * write primitives - redirect stderr to a log file when invoking this.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/iop_cdvd.h"
#include "core/checkpoint.h"

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *bios_path = argc > 1 ? argv[1] : "/tmp/r1131/bios.bin";
    const char *disc_path = argc > 2 ? argv[2] : "/tmp/r1131/gt3.iso";
    uint64_t target = argc > 3 ? strtoull(argv[3], NULL, 10) : 900000000ull;
    uint64_t slice  = argc > 4 ? strtoull(argv[4], NULL, 10) : 20000000ull;
    /* Round 1132 checkpoint-chain extension: ckpt_path is read (if it
     * already exists) to resume instead of cold-booting, and is
     * re-saved after every slice, so a wall-clock-timeout kill still
     * leaves a resumable checkpoint - the exact same pattern this
     * project's tools/roundNNN chain_driver.c-class tools already use
     * (Round 575/750/etc), applied here to survive across separate
     * sandbox tool-call invocations (each of which gets its own
     * process lifetime capped around ~170s wall-clock). */
    const char *ckpt_path = argc > 5 ? argv[5] : "/tmp/r1132/ckpt.bin";
    uint64_t wall_budget_slices = argc > 6 ? strtoull(argv[6], NULL, 10) : 1000000000ull; /* effectively unbounded unless capped by caller */

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { printf("[FAIL] could not load BIOS %s\n", bios_path); return 1; }
    int have_disc = (strcmp(disc_path, "none") != 0);

    FILE *probe = fopen(ckpt_path, "rb");
    int resumed = 0;
    if (probe) {
        fclose(probe);
        if (checkpoint_load(ckpt_path, &bios, &bios, have_disc ? disc_path : NULL) == 0) {
            resumed = 1;
            printf("[R1132] resumed from checkpoint %s\n", ckpt_path);
        } else {
            printf("[R1132] checkpoint_load(%s) FAILED, falling back to cold boot\n", ckpt_path);
        }
    }
    if (!resumed) {
        if (system_init(&bios, &bios) != 0) { printf("[FAIL] system_init failed\n"); return 1; }
        if (have_disc) {
            if (iop_cdvd_mount_iso(disc_path) != 0) { printf("[FAIL] could not mount disc %s\n", disc_path); return 1; }
        }
        printf("[R1132] cold-booted (no prior checkpoint at %s)\n", ckpt_path);
    }
    printf("[R1132] disc=%s target=%llu slice=%llu ckpt=%s\n",
           have_disc ? disc_path : "NONE(diskless)", (unsigned long long)target, (unsigned long long)slice, ckpt_path);

    ee_state_t *ee = ee_core_get_state();
    for (uint64_t s = 0; s < wall_budget_slices && ee->instructions_executed < target; s++) {
        system_run_interleaved(slice);
        printf("[R1132-P] slice=%llu instr=%llu pc=0x%08x halted=%u\n",
               (unsigned long long)s, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
        if (ee->halted) { printf("[R1132] halted: %s\n", ee->halt_reason); break; }
        if (checkpoint_save(ckpt_path) != 0) {
            printf("[R1132] WARNING: checkpoint_save(%s) failed at instr=%llu\n",
                   ckpt_path, (unsigned long long)ee->instructions_executed);
        }
    }
    printf("[R1132-SUMMARY] final instr=%llu pc=0x%08x halted=%u\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
    return 0;
}
