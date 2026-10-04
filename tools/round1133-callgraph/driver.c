/*
 * Round 1133 (task #1043 continuation, user's explicit escalation:
 * "don't just find the caller - check every line, disassemble
 * everything, fix it, even if it needs a beta-fix or reverse
 * engineering"). This driver implements Task 1 (call-graph capture)
 * and Task 4 (scheduler snapshot) of the Round 1133 directive against
 * the Round 1132 checkpoint (/tmp/r1132/ckpt.bin, instr=1,003,074,548,
 * EE resting in the confirmed stall loop 0x8000FE68->0x8000FC58->
 * 0x8000FC64).
 *
 * METHOD (per the user's explicit instruction NOT to rely on static
 * JAL search alone): resumes from the checkpoint, then single-steps
 * the EE one real instruction at a time (via ee_core_step(), bypassing
 * system_run_interleaved()'s coarser slice granularity), watching the
 * ACTUAL RESULTING PC after every single instruction. Any transition
 * whose new pc lands on a watched address, coming from a previous pc
 * that was NOT the instruction immediately preceding it in the same
 * function body, is logged as a genuine "entry from outside" - this
 * catches JAL, JALR, JR, ERET, and exception-vector entries alike,
 * since all of them are just "pc became X via some non-linear
 * mechanism" from the observer's point of view. Also logs every
 * EXIT from the wider loop region [0x8000F800,0x80010000).
 *
 * At entry/exit events, and periodically, dumps: prev_pc, new_pc,
 * $ra (gpr[31].ud0), EPC (cop0[14]), Cause (cop0[13]), Status
 * (cop0[12], with EXL/ERL/IE/EIE decoded), current EE HLE thread id,
 * and that thread's status/wait_type/wait_id - satisfying Aufgabe 1's
 * explicit per-entry logging requirement.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/sif.h"
#include "core/checkpoint.h"

/* The five addresses of interest from Round 1131/1132's live
 * disassembly, plus the wider loop bracket. */
#define LOOP_LO 0x8000F800u
#define LOOP_HI 0x80010000u

static int is_watched(uint32_t pc)
{
    return pc == 0x8000FC58u || pc == 0x8000FE80u ||
           pc == 0x8000FC64u || pc == 0x8000FC68u || pc == 0x8000FE68u;
}

static void dump_context(const char *tag, uint32_t prev_pc, uint32_t new_pc, ee_state_t *ee)
{
    uint32_t status = ee->cop0[12];
    uint32_t cause  = ee->cop0[13];
    uint32_t epc    = ee->cop0[14];
    int tid = ee_hle_thread_get_current_thread_id();
    uint32_t tstat = ee_hle_thread_get_status(tid);
    uint32_t wtype = ee_hle_thread_get_wait_type(tid);
    uint32_t wid   = ee_hle_thread_get_wait_id(tid);
    printf("[R1133-%s] instr=%llu prev_pc=0x%08x new_pc=0x%08x ra=0x%08x epc=0x%08x cause=0x%08x status=0x%08x (IE=%d EXL=%d ERL=%d EIE=%d) tid=%d tstat=0x%x wtype=%u wid=%u\n",
           tag,
           (unsigned long long)ee->instructions_executed,
           prev_pc, new_pc,
           (uint32_t)ee->gpr[31].ud0,
           epc, cause, status,
           (status & 1) ? 1 : 0, (status & 2) ? 1 : 0, (status & 4) ? 1 : 0, (status & 0x10000) ? 1 : 0,
           tid, tstat, wtype, wid);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *bios_path = argc > 1 ? argv[1] : "/tmp/r1131/bios.bin";
    const char *disc_path = argc > 2 ? argv[2] : "/tmp/r1131/gt3.iso";
    const char *ckpt_path = argc > 3 ? argv[3] : "/tmp/r1132/ckpt.bin";
    uint64_t step_budget  = argc > 4 ? strtoull(argv[4], NULL, 10) : 50000000ull;

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { printf("[FAIL] could not load BIOS %s\n", bios_path); return 1; }
    int have_disc = (strcmp(disc_path, "none") != 0);

    if (checkpoint_load(ckpt_path, &bios, &bios, have_disc ? disc_path : NULL) != 0) {
        printf("[FAIL] checkpoint_load(%s) failed\n", ckpt_path);
        return 1;
    }
    printf("[R1133] resumed from checkpoint %s\n", ckpt_path);

    ee_state_t *ee = ee_core_get_state();
    printf("[R1133] start instr=%llu pc=0x%08x\n", (unsigned long long)ee->instructions_executed, ee->pc);

    /* One-off: dump the region around ra=0x800126cc (the constant return
     * address observed on every ENTRY/EXIT of the stall loop) to find
     * the real caller and whatever code, if any, disables interrupts
     * (Status.IE) before invoking the loop. */
    if (getenv("R1133_DUMP_CALLER")) {
        uint32_t base = 0x800125C0u;
        for (uint32_t a = base; a < base + 0x180; a += 4) {
            printf("[R1133-WORD] 0x%08x: %08x\n", a, ee_mem_read32(ee, a));
        }
    }

    uint32_t prev_pc = ee->pc;
    int prev_in_loop = (prev_pc >= LOOP_LO && prev_pc < LOOP_HI);
    uint64_t entries = 0, exits = 0;
    uint64_t last_snapshot = ee->instructions_executed;

    for (uint64_t i = 0; i < step_budget && !ee->halted; i++) {
        ee_core_step();
        uint32_t new_pc = ee->pc;

        /* Watched-address entry detection: new_pc is one of the 5
         * addresses, and this is a genuine transition (prev_pc != the
         * address immediately before it in linear program order,
         * i.e. new_pc != prev_pc+4 when prev_pc was already at that
         * exact watched address minus 4 - in practice just check
         * prev_pc != new_pc-4 to rule out ordinary fall-through). */
        if (is_watched(new_pc) && new_pc != prev_pc + 4u) {
            dump_context("ENTRY", prev_pc, new_pc, ee);
            entries++;
        }

        int new_in_loop = (new_pc >= LOOP_LO && new_pc < LOOP_HI);
        if (prev_in_loop && !new_in_loop) {
            dump_context("EXIT", prev_pc, new_pc, ee);
            exits++;
        }
        prev_in_loop = new_in_loop;
        prev_pc = new_pc;

        if (ee->instructions_executed - last_snapshot >= 5000000ull) {
            last_snapshot = ee->instructions_executed;
            int tc = ee_hle_thread_get_thread_count();
            printf("[R1133-SNAP] instr=%llu pc=0x%08x thread_count=%d entries_so_far=%llu exits_so_far=%llu\n",
                   (unsigned long long)ee->instructions_executed, ee->pc, tc,
                   (unsigned long long)entries, (unsigned long long)exits);
            for (int t = 1; t <= tc; t++) {
                printf("[R1133-TCB] tid=%d status=0x%x prio=%u wtype=%u wid=%u entry=0x%08x saved_pc=0x%08x wakeup_count=%u ra=0x%08x\n",
                       t, ee_hle_thread_get_status(t), ee_hle_thread_get_priority(t),
                       ee_hle_thread_get_wait_type(t), ee_hle_thread_get_wait_id(t),
                       ee_hle_thread_get_entry(t), ee_hle_thread_get_saved_pc(t),
                       ee_hle_thread_get_wakeup_count(t),
                       (uint32_t)ee_hle_thread_get_gpr(t, 31));
            }
        }
    }

    printf("[R1133-SUMMARY] final instr=%llu pc=0x%08x halted=%u entries=%llu exits=%llu\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted,
           (unsigned long long)entries, (unsigned long long)exits);
    return 0;
}
