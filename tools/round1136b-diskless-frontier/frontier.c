/*
 * Round 1136b (user-directed, German): "CRACK THE DISKLESS FRONTIER
 * 0x00264980" - deep forensic survey of the diskless SCPH-50004 cold
 * boot's resting PC (0x00264980), reported at the end of Round 1136's
 * post-REND-fix re-verification (384,853,226 cumulative instructions).
 *
 * Explicit user constraints:
 *  - do NOT patch/skip 0x00264980
 *  - first determine WHAT 0x00264980 actually is on a genuine fresh
 *    diskless boot (not inferred from a stale/final PC alone)
 *  - then determine WHY it keeps executing (tight loop vs. sampling
 *    artifact vs. syscall/wait stub vs. larger worker loop)
 *  - build a PC histogram over a real instruction window, not a
 *    single snapshot
 *  - dump EE thread state, and if a wait primitive is involved, trace
 *    its real creator/producer/last-signal history using this
 *    project's own existing introspection accessors (ee_hle_thread.h)
 *  - this driver is READ-ONLY diagnostics. No patch, no skip, no
 *    forced signal is applied here - if the investigation calls for a
 *    causal A/B confirmation test, that is a SEPARATE scratch step,
 *    not folded into this survey.
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

/* Reuse the project's own Round 655 disassembler (self-included, main()
 * renamed away so it doesn't collide with this driver's own main()). */
#define main disasm_tool_unused_main
#include "../round655-ee-disasm/disasm.c"
#undef main

/* ---- simple open-addressing PC histogram (real EE code addresses in
 * this project's boot traces are always < 0x02000000 physical/KUSEG-
 * identity, so a direct-mapped table sized to cover that range in
 * 4-byte-aligned buckets is exact, not a hash approximation). */
#define HIST_SIZE (0x02000000u / 4u)
static uint32_t *g_hist;

static uint32_t g_min_pc = 0xFFFFFFFFu, g_max_pc = 0;
static uint64_t g_total_samples = 0;

static void hist_add(uint32_t pc)
{
    if (pc < 0x02000000u && (pc & 3) == 0) {
        g_hist[pc / 4]++;
        if (pc < g_min_pc) g_min_pc = pc;
        if (pc > g_max_pc) g_max_pc = pc;
    }
    g_total_samples++;
}

static void dump_top_pcs(int topn)
{
    /* simple partial selection sort over the sparse histogram */
    typedef struct { uint32_t pc; uint32_t cnt; } ent_t;
    ent_t *top = calloc((size_t)topn, sizeof(ent_t));
    uint64_t unique_pcs = 0;
    for (uint32_t i = 0; i < HIST_SIZE; i++) {
        uint32_t cnt = g_hist[i];
        if (!cnt) continue;
        unique_pcs++;
        uint32_t pc = i * 4;
        /* insert into top[] if it beats the current minimum */
        int min_idx = -1;
        uint32_t min_cnt = 0xFFFFFFFFu;
        for (int j = 0; j < topn; j++) {
            if (top[j].cnt == 0) { min_idx = j; min_cnt = 0; break; }
            if (top[j].cnt < min_cnt) { min_cnt = top[j].cnt; min_idx = j; }
        }
        if (cnt > min_cnt) { top[min_idx].pc = pc; top[min_idx].cnt = cnt; }
    }
    /* sort descending by count (topn is small, insertion sort is fine) */
    for (int i = 1; i < topn; i++) {
        ent_t key = top[i]; int j = i - 1;
        while (j >= 0 && top[j].cnt < key.cnt) { top[j+1] = top[j]; j--; }
        top[j+1] = key;
    }
    printf("[FRONTIER] unique PCs sampled: %llu, min_pc=0x%08X, max_pc=0x%08X, total_samples=%llu\n",
           (unsigned long long)unique_pcs, g_min_pc, g_max_pc, (unsigned long long)g_total_samples);
    printf("[FRONTIER] TOP %d PCs by hit count:\n", topn);
    for (int i = 0; i < topn; i++) {
        if (!top[i].cnt) break;
        double pct = 100.0 * (double)top[i].cnt / (double)g_total_samples;
        printf("  #%2d  pc=0x%08X  hits=%10u  (%.3f%%)\n", i + 1, top[i].pc, top[i].cnt, pct);
    }
    free(top);
}

static void dump_disasm_range(ee_state_t *ee, uint32_t start, uint32_t end, uint32_t mark_pc)
{
    printf("\n[FRONTIER] disassembly of live EE RAM 0x%08X-0x%08X (as actually resident NOW):\n", start, end);
    for (uint32_t a = start; a < end; a += 4) {
        uint32_t instr = ee_mem_read32(ee, a);
        char line[256];
        disasm_one(instr, a, line, sizeof(line));
        printf("  %s0x%08X: %08X  %s\n", (a == mark_pc) ? ">>> " : "    ", a, instr, line);
    }
}

static const char *wait_type_name(uint32_t wt)
{
    switch (wt) {
        case 0: return "NONE";
        case 1: return "SLEEP(WaitSema-class)";
        default: return "OTHER";
    }
}

static void dump_threads(void)
{
    int n = ee_hle_thread_get_thread_count();
    int cur = ee_hle_thread_get_current_thread_id();
    printf("[FRONTIER] EE thread table (count=%d, current tid=%d):\n", n, cur);
    for (int tid = 1; tid <= 32; tid++) {
        uint32_t status = ee_hle_thread_get_status(tid);
        if (status == 0) continue; /* not in use */
        uint32_t prio = ee_hle_thread_get_priority(tid);
        uint32_t wt = ee_hle_thread_get_wait_type(tid);
        uint32_t wid = ee_hle_thread_get_wait_id(tid);
        uint32_t entry = ee_hle_thread_get_entry(tid);
        uint32_t saved_pc = ee_hle_thread_get_saved_pc(tid);
        uint64_t wakeups = ee_hle_thread_get_wakeup_calls(tid);
        printf("  tid=%d%s status=0x%X prio=%u wait_type=%s wait_id=%u entry=0x%08X saved_pc=0x%08X wakeup_calls=%llu\n",
               tid, (tid == cur) ? " [CURRENT]" : "", status, prio, wait_type_name(wt), wid,
               entry, saved_pc, (unsigned long long)wakeups);
        if (wt != 0) {
            int in_use = 0; int32_t maxc = 0, cnt = 0, waiters = 0;
            if (ee_hle_thread_get_sema_state((int)wid, &in_use, &maxc, &cnt, &waiters) == 0) {
                uint64_t sigs = ee_hle_thread_get_signal_calls((int)wid);
                printf("      -> semaphore %u: in_use=%d max_count=%d count=%d wait_threads=%d signal_calls_this_run=%llu\n",
                       wid, in_use, maxc, cnt, waiters, (unsigned long long)sigs);
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <bios_path>\n", argv[0]);
        return 1;
    }
    g_hist = calloc(HIST_SIZE, sizeof(uint32_t));
    if (!g_hist) { fprintf(stderr, "OOM\n"); return 1; }

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();

    const uint32_t TARGET_LO = 0x00264800u, TARGET_HI = 0x00264B00u;
    uint64_t budget_coarse = 700000000ull; /* generous vs the ~385M observed in Round 1136 */
    uint64_t coarse_chunk = 2000000ull;
    uint64_t done = 0;
    int reached = 0;

    printf("[FRONTIER] phase 1: coarse run until EE pc first enters [0x%08X-0x%08X)\n", TARGET_LO, TARGET_HI);
    while (done < budget_coarse && !ee->halted) {
        system_run_interleaved(coarse_chunk);
        done += coarse_chunk;
        if (ee->pc >= TARGET_LO && ee->pc < TARGET_HI) { reached = 1; break; }
    }
    if (!reached) {
        printf("[FRONTIER] NEVER reached target range within %llu-instruction-slice budget. pc=0x%08X instr=%llu\n",
               (unsigned long long)budget_coarse, ee->pc, (unsigned long long)ee->instructions_executed);
        return 1;
    }

    printf("\n[FRONTIER] === FIRST ARRIVAL AT TARGET RANGE ===\n");
    printf("  instr_count = %llu\n", (unsigned long long)ee->instructions_executed);
    printf("  current tid = %d\n", ee_hle_thread_get_current_thread_id());
    printf("  PC   = 0x%08X\n", ee->pc);
    printf("  RA   = 0x%08X\n", (uint32_t)ee->gpr[31].ud0);
    printf("  SP   = 0x%08X\n", (uint32_t)ee->gpr[29].ud0);
    printf("  GP   = 0x%08X\n", (uint32_t)ee->gpr[28].ud0);
    printf("  EPC  = 0x%08X\n", ee->cop0[14]);
    printf("  Status = 0x%08X\n", ee->cop0[12]);
    printf("  Cause  = 0x%08X\n", ee->cop0[13]);

    dump_disasm_range(ee, 0x00264800u, 0x00264B00u, 0x00264980u);
    dump_threads();

    /* Phase 2: fine-grained per-instruction PC histogram, EE side only
     * (IOP kept running in lockstep via the same interleave ratio the
     * real scheduler uses, by stepping both manually here). */
    printf("\n[FRONTIER] phase 2: fine-grained PC histogram over the next several million EE instructions\n");
    uint64_t fine_budget = 8000000ull; /* several million, per the request */
    for (uint64_t i = 0; i < fine_budget && !ee->halted; i++) {
        hist_add(ee->pc);
        for (int k = 0; k < 8; k++) { if (!ee->halted) ee_core_step(); }
        if (!iop->halted) iop_core_step();
    }
    dump_top_pcs(30);

    printf("\n[FRONTIER] === STATE AFTER PHASE-2 WINDOW ===\n");
    printf("  instr_count = %llu\n", (unsigned long long)ee->instructions_executed);
    printf("  PC = 0x%08X  RA = 0x%08X  SP = 0x%08X\n",
           ee->pc, (uint32_t)ee->gpr[31].ud0, (uint32_t)ee->gpr[29].ud0);
    dump_threads();

    return 0;
}
