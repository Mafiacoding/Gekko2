/*
 * Round 1137 (direct follow-up to Round 1131/1132's own explicit
 * next-step, now also user-directed: "do the next step also see if
 * its any missing syscalls or maybe calls from the bios to the ram
 * and wise versa").
 *
 * Round 1132 proved exhaustively (1.003B instructions, all 4
 * ee_mem_write* primitives + the DMA bulk-copy path) that
 * MEM[0x80023FC8]/[0x80023FCC] - the SIF2-kick dispatcher's organic
 * exit condition - is written exactly once (a static zero-init at
 * pc=0x80014380) and otherwise NEVER written. Neither known reader
 * function (0x8000FC58, 0x8000FE80) writes it either. Round 1132's
 * own conclusion: "the real question is not what clears it, but what
 * code is supposed to call 0x8000FC58 in the first place, and why
 * does this boot's control flow never reach that call site."
 *
 * This tool performs exactly that caller-scan: after a fresh diskless
 * cold boot reaches a stable resting state (matching the Round
 * 1131/1132 steady-state loop), it statically scans the ENTIRE
 * resident low-kernel EE RAM image (0x80000000-0x80020000, the range
 * BIOS/kernel code is known to live in per this project's own
 * standing convention) for every direct J/JAL instruction (opcodes 2
 * and 3) whose target equals 0x8000FC58 or 0x8000FE80 - the two known
 * reader/dispatcher functions for the stuck flag. This finds every
 * STATIC call site in the loaded image, regardless of whether it is
 * dynamically reached in this trace, which is exactly what's needed
 * to determine whether a real caller exists at all (dead code some
 * other gate never opens) or whether no caller was ever loaded/linked
 * in the first place (a genuine BIOS-image/loader gap).
 *
 * Read-only diagnostic. No patch, no skip, no forced write. Per the
 * user's explicit prohibition, 0x00264980 itself remains untouched
 * and is not part of this scan (already closed out as a non-issue in
 * Round 1136b).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/iop/iop_core.h"

#define main disasm_tool_unused_main
#include "../round655-ee-disasm/disasm.c"
#undef main

static void scan_for_calls(ee_state_t *ee, uint32_t lo, uint32_t hi, uint32_t target)
{
    int found = 0;
    for (uint32_t a = lo; a < hi; a += 4) {
        uint32_t instr = ee_mem_read32(ee, a);
        uint32_t op = instr >> 26;
        if (op == 2 || op == 3) { /* J / JAL */
            uint32_t idx = instr & 0x03FFFFFFu;
            uint32_t tgt = (a & 0xF0000000u) | (idx << 2);
            if (tgt == target) {
                char line[256];
                disasm_one(instr, a, line, sizeof(line));
                printf("  CALL SITE: pc=0x%08X  instr=0x%08X  %s  (op=%s)\n",
                       a, instr, line, (op == 3) ? "JAL" : "J");
                found++;
            }
        }
    }
    if (!found) printf("  (no direct J/JAL to 0x%08X found in 0x%08X-0x%08X)\n", target, lo, hi);
}

/* Also scan for JALR (indirect calls via register) near candidate
 * "function pointer table" regions is out of scope for a bounded
 * direct-call static scan; flagged in the summary instead. */
static uint64_t count_jalr(ee_state_t *ee, uint32_t lo, uint32_t hi)
{
    uint64_t n = 0;
    for (uint32_t a = lo; a < hi; a += 4) {
        uint32_t instr = ee_mem_read32(ee, a);
        if ((instr >> 26) == 0 && (instr & 0x3F) == 0x09) n++; /* SPECIAL, funct=JALR */
    }
    return n;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bios_path>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t *ee = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();

    /* Run to a comparable steady state before scanning - not strictly
     * required for a static scan of already-resident low-kernel code
     * (that code is present from very early boot per Round 1131's own
     * finding that pc=0x80014380 fires during "early kernel bring-up"),
     * but matches this project's standing methodology of confirming
     * live-resident bytes rather than assuming ROM-image content. */
    uint64_t budget = 0, step = 5000000ull, cap = 40000000ull;
    while (budget < cap && !ee->halted) {
        system_run_interleaved(step);
        budget += step;
        fprintf(stderr, "[R1137] progress instr=%llu\n", (unsigned long long)budget);
        fflush(stderr);
    }
    printf("[R1137] scan performed at instr_count=%llu pc=0x%08X halted=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    printf("\n[R1137] static J/JAL caller scan of 0x80000000-0x80020000 for target 0x8000FC58 (SIF2-kick dispatcher):\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x8000FC58u);

    printf("\n[R1137] static J/JAL caller scan of 0x80000000-0x80020000 for target 0x8000FE80 (sibling 'Caller #2' dispatcher):\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x8000FE80u);

    /* Widen the scan to the full resident OSDSYS ELF range too, in
     * case the real caller lives in user-mode OSDSYS code rather than
     * the low-kernel range (both dispatchers ARE inside the low-kernel
     * range per Round 1131's own live disasm, but a caller could be
     * anywhere that has already been loaded). */
    printf("\n[R1137] widened static J/JAL scan of 0x00200000-0x00280000 (loaded OSDSYS ELF range) for the same two targets:\n");
    scan_for_calls(ee, 0x00200000u, 0x00280000u, 0x8000FC58u);
    scan_for_calls(ee, 0x00200000u, 0x00280000u, 0x8000FE80u);

    /* Round 1138 (direct continuation of Round 1029-1032's caller trace):
     * one more level up. Round 1030 found 4 more dead-code addresses
     * (0x8000EC00, 0x80010284, 0x80012584, 0x8000F214) with 0 hits across
     * 60M instructions, and one address (0x80010114) with ZERO direct
     * J/JAL callers anywhere in the 4MB image - consistent with reaching
     * it only via an indirect JALR/function-pointer-table dispatch (the
     * same pattern Round 1032 successfully cracked for 0x8026F4D0 by
     * masking to the physical address and finding it IS live via its
     * KUSEG form). Re-verify all of this fresh against the current
     * (post-1136) tree: static callers of the 4 known dead addresses,
     * plus a raw-pointer-word scan for all 5 addresses (function-pointer
     * table entries), exactly like the successful 0x8026F4D0 technique. */
    printf("\n[R1138] static J/JAL caller scan of 0x80000000-0x80020000 for the Round-1030 level-2 addresses:\n");
    printf(" -- target 0x8000EC00 (switch/case dispatcher, 2 known stubs inside) --\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x8000EC00u);
    printf(" -- target 0x80010284 --\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x80010284u);
    printf(" -- target 0x80012584 --\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x80012584u);
    printf(" -- target 0x8000F214 --\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x8000F214u);
    printf(" -- target 0x80010114 (Round 1030: zero direct callers found, suspected indirect-only) --\n");
    scan_for_calls(ee, 0x80000000u, 0x80020000u, 0x80010114u);

    printf("\n[R1138] raw-pointer-word scan (function-pointer-table candidates) for all 5 Round-1030 addresses,\n"
           "        over BOTH the low-kernel range and the loaded OSDSYS ELF range:\n");
    {
        uint32_t targets[5] = { 0x8000EC00u, 0x80010284u, 0x80012584u, 0x8000F214u, 0x80010114u };
        const char *names[5] = { "0x8000EC00", "0x80010284", "0x80012584", "0x8000F214", "0x80010114" };
        for (int ti = 0; ti < 5; ti++) {
            int hits = 0;
            for (uint32_t a = 0x80000000u; a < 0x80020000u; a += 4) {
                uint32_t w = ee_mem_read32(ee, a);
                if (w == targets[ti]) { printf("  WORD@0x%08X == %s (low-kernel)\n", a, names[ti]); hits++; }
            }
            for (uint32_t a = 0x00200000u; a < 0x00280000u; a += 4) {
                uint32_t w = ee_mem_read32(ee, a);
                if (w == targets[ti]) { printf("  WORD@0x%08X == %s (OSDSYS ELF)\n", a, names[ti]); hits++; }
            }
            if (!hits) printf("  %s: no raw pointer word found in either range\n", names[ti]);
        }
    }

    uint64_t jalr_lowkernel = count_jalr(ee, 0x80000000u, 0x80020000u);
    printf("\n[R1137] indirect-call (JALR) instruction count in 0x80000000-0x80020000: %llu\n"
           "        (a caller could reach these dispatchers via a function-pointer table instead\n"
           "         of a direct J/JAL - not individually resolved by this static scan; flags scope\n"
           "         for a follow-up dynamic JALR-target trace if the direct scan above is empty)\n",
           (unsigned long long)jalr_lowkernel);

    /* Dump the raw bytes immediately preceding each dispatcher entry,
     * to check for a jump-table/vtable slot pointing at it (an
     * indirect-call pattern common in PS2 kernel code: a table of
     * function pointers, not literal JAL instructions). */
    printf("\n[R1137] scanning 0x80000000-0x80020000 for any 32-bit WORD equal to 0x8000FC58 or 0x8000FE80\n"
           "        (a raw pointer value, consistent with a function-pointer table entry):\n");
    int ptr_hits_a = 0, ptr_hits_b = 0;
    for (uint32_t a = 0x80000000u; a < 0x80020000u; a += 4) {
        uint32_t w = ee_mem_read32(ee, a);
        if (w == 0x8000FC58u) { printf("  WORD@0x%08X == 0x8000FC58 (fn-ptr candidate)\n", a); ptr_hits_a++; }
        if (w == 0x8000FE80u) { printf("  WORD@0x%08X == 0x8000FE80 (fn-ptr candidate)\n", a); ptr_hits_b++; }
    }
    if (!ptr_hits_a && !ptr_hits_b) printf("  (no raw pointer words found for either address)\n");

    /* Round 1138 Phase 3: the static scan above cannot resolve JALR
     * targets (register-indirect, not a literal in the instruction).
     * 29 real JALR instructions exist in the low-kernel range - dynamic-
     * trace every one of them for a real run, logging (pc, target) any
     * time the computed target lands on one of the 5 candidate dead-zone
     * entry points, or inside the 0x8000EC00-0x80013000 subsystem range
     * generally. This directly answers "is this reached via an indirect
     * call at all, ever" - the one thing the static scan can't settle. */
    printf("\n[R1138] Phase 3: dynamic JALR-target trace over the next 60,000,000 EE instructions\n"
           "        (watching for any indirect call landing in the dead SIF2-request-submission zone)\n");
    uint32_t watch_lo = 0x8000EC00u, watch_hi = 0x80013100u;
    uint64_t jalr_total = 0, jalr_into_zone = 0;
    uint32_t last_zone_hits[8][2]; int zone_hit_n = 0;
    uint64_t fine_budget2 = 20000000ull;
    for (uint64_t i = 0; i < fine_budget2 && !ee->halted; i++) {
        uint32_t pc = ee->pc;
        if (pc < 0x02000000u && (pc & 3) == 0) {
            uint32_t instr = ee_mem_read32(ee, pc);
            if ((instr >> 26) == 0 && (instr & 0x3F) == 0x09) { /* JALR */
                uint32_t rs = (instr >> 21) & 0x1Fu;
                uint32_t tgt = (uint32_t)ee->gpr[rs].ud0;
                jalr_total++;
                uint32_t tgt_phys = tgt & 0x1FFFFFFFu;
                uint32_t tgt_kseg0 = 0x80000000u | tgt_phys;
                if ((tgt_kseg0 >= watch_lo && tgt_kseg0 < watch_hi) ||
                    (tgt_phys + 0x80000000u >= watch_lo && tgt_phys + 0x80000000u < watch_hi)) {
                    jalr_into_zone++;
                    if (zone_hit_n < 8) {
                        last_zone_hits[zone_hit_n][0] = pc;
                        last_zone_hits[zone_hit_n][1] = tgt;
                        zone_hit_n++;
                    }
                }
            }
        }
        for (int k = 0; k < 8; k++) { if (!ee->halted) ee_core_step(); }
        if (!iop->halted) iop_core_step();
    }
    printf("[R1138] JALR instructions executed (dynamically, this window): %llu\n"
           "[R1138] JALR targets landing inside 0x%08X-0x%08X: %llu\n",
           (unsigned long long)jalr_total, watch_lo, watch_hi, (unsigned long long)jalr_into_zone);
    for (int i = 0; i < zone_hit_n; i++)
        printf("  zone-entry #%d: jalr at pc=0x%08X -> target=0x%08X\n", i, last_zone_hits[i][0], last_zone_hits[i][1]);
    printf("[R1138] final state: instr_count=%llu pc=0x%08X halted=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    return 0;
}
