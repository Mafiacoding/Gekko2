/*
 * Round 1100 (task #1008/#887 continuation, per user's explicit
 * request to trace the current WaitSema park at 0x00257964 back to
 * its exact syscall/sema_id/CreateSema/SignalSema/RAM-state chain):
 * checkpoint-chained diskless SCPH-50004 boot compiled with the
 * tree's own existing R1036_REG_TRACE + R933_RPCCALL_TRACE
 * instrumentation (Round 1036/1038's own macros, zero-cost-when-
 * undefined, never removed), to find out - on the CURRENT tree, post
 * Rounds 1039-1097b - exactly which real SIF-RPC service is stuck at
 * the current WaitSema park, continuing the Round 1038/1039/1040/
 * 1041/1042 "identify and answer each real service" methodology
 * rather than re-deriving it from scratch.
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

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path> <start|continue> [budget]\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    const char *ckpt_path = argv[2];
    const char *mode = argv[3];
    uint64_t budget = argc > 4 ? strtoull(argv[4], NULL, 10) : 50000000ull;

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }

    if (strcmp(mode, "start") == 0) {
        if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    } else {
        if (checkpoint_load(ckpt_path, &bios, &bios, NULL) != 0) { fprintf(stderr, "checkpoint_load fail\n"); return 1; }
    }

    ee_state_t *ee = ee_core_get_state();
    uint64_t chunk = 5000000ull, done = 0;
    while (done < budget && !ee->halted) {
        system_run_interleaved(chunk);
        done += chunk;
    }

    int tid = ee_hle_thread_get_current_thread_id();
    uint32_t status = ee_hle_thread_get_status(tid);
    uint32_t wtype = ee_hle_thread_get_wait_type(tid);
    uint32_t wid = ee_hle_thread_get_wait_id(tid);
    int in_use = 0; int32_t max_count = 0, count = 0, wait_threads = 0;
    ee_hle_thread_get_sema_state((int)wid, &in_use, &max_count, &count, &wait_threads);

    fprintf(stderr, "[R1100] instr=%llu ee_pc=0x%08x iop_pc=0x%08x tid=%d status=0x%x wait_type=%u wait_id=%u "
            "sema(in_use=%d max=%d count=%d waiters=%d) halted=%u\n",
            (unsigned long long)done, ee->pc, iop_core_get_state()->pc, tid, status, wtype, wid,
            in_use, max_count, count, wait_threads, ee->halted);

    if (ee->halted) { printf("[R1100] EE halted: %s\n", ee->halt_reason); return 0; }
    if (checkpoint_save(ckpt_path) != 0) { fprintf(stderr, "checkpoint_save fail\n"); return 1; }
    printf("[R1100] checkpoint saved to %s, done=%llu\n", ckpt_path, (unsigned long long)done);
    return 0;
}
