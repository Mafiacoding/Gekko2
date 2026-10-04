/*
 * Round 1131 (task #966/#1039-1044, user's explicit "CRACK THE
 * 0x8000FC68 BIOS STALL" directive): live instrumentation to
 * reconstruct the exact control flow, register/MMIO state, and real
 * exit condition of the loop the current tree's GT3 disc-boot survey
 * (Round 1130) sampled resting at pc=0x8000fc68.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/iop/iop_core.h"
#include "core/hw/iop_hle_thread.h"
#include "core/hw/gs.h"
#include "core/hw/gif.h"
#include "core/hw/sif.h"
#include "core/hw/dma.h"
#include "core/hw/ee_intc.h"
#include "core/hw/iop_cdvd.h"

#define EE_INTC_IRQ_SBUS 1

static int in_watchlist(uint32_t pc)
{
    static const uint32_t addrs[] = {
        0x8000FE20u, 0x8000FE24u, 0x8000FE28u, 0x8000FE2Cu, 0x8000FE30u, 0x8000FE34u,
        0x8000FAC8u, 0x8000FB9Cu, 0x8000FBA8u, 0x8000FC54u, 0x8000FC58u, 0x8000FC68u,
        0x8000FC78u, 0x800126C4u, 0x8000FEC8u, 0x8000FF64u, 0x80001D70u,
    };
    for (size_t i = 0; i < sizeof(addrs)/sizeof(addrs[0]); i++)
        if (pc == addrs[i]) return 1;
    return 0;
}

static void log_state(FILE *f, uint64_t step, ee_state_t *ee, uint32_t opcode)
{
    sif_state_t *sif = sif_get_state();
    dma_state_t *dma = dma_get_state();
    ee_intc_state_t *intc = ee_intc_get_state();
    uint32_t busy1 = ee_mem_read32(ee, 0x80023FC8u);
    uint32_t busy2 = ee_mem_read32(ee, 0x80023FCCu);
    fprintf(f,
        "[R1131] step=%llu instr=%llu pc=0x%08x op=0x%08x "
        "v0=0x%08x a0=0x%08x s0=0x%08x t0=0x%08x ra=0x%08x "
        "SMFLAG=0x%08x INTC_STAT=0x%08x INTC_MASK=0x%08x D_STAT=0x%08x "
        "busy1@23FC8=0x%08x busy2@23FCC=0x%08x sbus_raise_count=%u\n",
        (unsigned long long)step, (unsigned long long)ee->instructions_executed,
        ee->pc, opcode,
        (uint32_t)ee->gpr[2].ud0, (uint32_t)ee->gpr[4].ud0, (uint32_t)ee->gpr[16].ud0,
        (uint32_t)ee->gpr[8].ud0, (uint32_t)ee->gpr[31].ud0,
        sif->smflag, intc->stat, intc->mask, dma->d_stat,
        busy1, busy2, ee_intc_get_raise_count(EE_INTC_IRQ_SBUS));
    fflush(f);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *bios_path = argc > 1 ? argv[1] : "/tmp/r1131/bios.bin";
    const char *disc_path = argc > 2 ? argv[2] : "/tmp/r1131/gt3.iso";
    uint64_t phase1_target = argc > 3 ? strtoull(argv[3], NULL, 10) : 700000000ull;
    uint64_t phase3_steps  = argc > 4 ? strtoull(argv[4], NULL, 10) : 40000ull;
    const char *dumpfile   = argc > 5 ? argv[5] : "/tmp/r1131/liveram.bin";
    const char *tracefile  = argc > 6 ? argv[6] : "/tmp/r1131/trace.log";

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { printf("[FAIL] could not load BIOS %s\n", bios_path); return 1; }
    if (system_init(&bios, &bios) != 0) { printf("[FAIL] system_init failed\n"); return 1; }
    int have_disc = (strcmp(disc_path, "none") != 0);
    if (have_disc) {
        if (iop_cdvd_mount_iso(disc_path) != 0) { printf("[FAIL] could not mount disc %s\n", disc_path); return 1; }
    }
    printf("[R1131] disc=%s, phase1_target=%llu phase3_steps=%llu\n",
           have_disc ? disc_path : "NONE(diskless)", (unsigned long long)phase1_target, (unsigned long long)phase3_steps);

    ee_state_t *ee = ee_core_get_state();

    /* Phase 1+2 merged: coarse slices with periodic progress prints (so a
     * wall-clock timeout still leaves useful partial evidence on disk),
     * watching for entry into the target BIOS-code family. */
    const uint64_t SLICE1 = 20000000ull;
    uint64_t max_slices = (phase1_target / SLICE1) + 100ull;
    int found = 0;
    for (uint64_t s = 0; s < max_slices; s++) {
        system_run_interleaved(SLICE1);
        printf("[R1131-P1] slice=%llu instr=%llu pc=0x%08x halted=%u\n",
               (unsigned long long)s, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
        if (ee->halted) { printf("[R1131] halted during phase 1: %s\n", ee->halt_reason); return 0; }
        if (ee->pc >= 0x8000F800u && ee->pc < 0x80010000u) {
            printf("[R1131-P1] entered target window at slice=%llu instr=%llu pc=0x%08x\n",
                   (unsigned long long)s, (unsigned long long)ee->instructions_executed, ee->pc);
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("[R1131-P1] NEVER entered target window [0x8000F800,0x80010000) within budget. "
               "final instr=%llu pc=0x%08x\n",
               (unsigned long long)ee->instructions_executed, ee->pc);
    }

    /* Dump the live BIOS-code region for offline re-disassembly. */
    {
        FILE *df = fopen(dumpfile, "wb");
        if (df) {
            for (uint32_t a = 0x8000FA80u; a < 0x8000FEC0u; a += 4) {
                uint32_t w = ee_mem_read32(ee, a);
                fwrite(&w, 4, 1, df);
            }
            fclose(df);
            printf("[R1131] dumped live 0x8000FA80-0x8000FEC0 to %s\n", dumpfile);
        }
    }

    /* Phase 3: fine-grained single-instruction trace + watchlist logging. */
    FILE *tf = fopen(tracefile, "w");
    if (!tf) { printf("[FAIL] could not open tracefile %s\n", tracefile); return 1; }

    uint64_t visit_counts[32]; memset(visit_counts, 0, sizeof(visit_counts));
    static const uint32_t watch[] = {
        0x8000FE20u, 0x8000FE24u, 0x8000FE28u, 0x8000FE2Cu, 0x8000FE30u, 0x8000FE34u,
        0x8000FAC8u, 0x8000FB9Cu, 0x8000FBA8u, 0x8000FC54u, 0x8000FC58u, 0x8000FC68u,
        0x8000FC78u, 0x800126C4u, 0x8000FEC8u, 0x8000FF64u, 0x80001D70u,
    };
    const int NWATCH = (int)(sizeof(watch)/sizeof(watch[0]));

    for (uint64_t step = 0; step < phase3_steps; step++) {
        uint32_t prev_pc = ee->pc;
        uint32_t opcode = ee_mem_read32(ee, prev_pc);
        int is_watch = in_watchlist(prev_pc);
        if (is_watch) {
            for (int i = 0; i < NWATCH; i++) if (watch[i] == prev_pc) { visit_counts[i]++; break; }
            log_state(tf, step, ee, opcode);
        }
        system_run_interleaved(1);
        if (ee->halted) {
            fprintf(tf, "[R1131] halted mid-trace at step=%llu: %s\n", (unsigned long long)step, ee->halt_reason);
            break;
        }
    }
    fclose(tf);

    printf("[R1131-P3] visit histogram over %llu fine-traced instructions:\n", (unsigned long long)phase3_steps);
    for (int i = 0; i < NWATCH; i++)
        printf("  pc=0x%08x visits=%llu\n", watch[i], (unsigned long long)visit_counts[i]);

    printf("[R1131-SUMMARY] final instr=%llu pc=0x%08x halted=%u\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
    return 0;
}
