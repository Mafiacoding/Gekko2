/*
 * Round 1126 (task #1036, user's detailed dual-hypothesis correlation
 * spec): fresh, disc-less cold boot of the CURRENT tree (SCPH-50004
 * BIOS, no checkpoint - per the user's explicit section-D warning
 * against trusting stale checkpoints), running with:
 *   - R933_RPCCALL_TRACE / R955_LOADFILE_NAME_TRACE / R815_HANDOFF_TRACE /
 *     R813_CDVDTRACE compiled in and unconditionally live for the WHOLE
 *     run (these only fire on genuine RPC/LOADFILE/CDVD events, so the
 *     volume stays low even across the full boot) - this is the IOP/
 *     SIF-RPC/mailbox side of the correlation (hypothesis B).
 *   - R812_EVENTLOG + R1126_ERET_TRACE compiled in but RUNTIME-DISABLED
 *     (ee_hle_thread_eventlog_set_enabled(0)) until the EE PC first
 *     reaches the known frontier (0x00257964, WaitSema's real syscall
 *     instruction), then enabled for a bounded multi-VBLANK-cycle
 *     window - this is the EE-side WAIT/SIGNAL/WAKE/RESCHEDULE/ERET
 *     side of the correlation (hypothesis A).
 *
 * This produces ONE interleaved stderr stream with both EE-scheduler
 * events ([R812EVT], [R1126ERET]) and IOP/SIF-RPC events ([R933EVT],
 * [R955EVT], [R815EVT], [R813EVT]) covering the same real wall-clock
 * window, so the two can be correlated by inspection - exactly what
 * the user's section (C) requested.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <bios_path> [pre_budget] [trace_window] [chunk]\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    /* how many instructions to run with EVT logging OFF, hunting for the
     * first arrival at pc==0x00257964 (WaitSema syscall) */
    uint64_t pre_budget = (argc >= 3) ? strtoull(argv[2], NULL, 10) : 30000000ull;
    /* once the frontier is first hit, how many further instructions to
     * run with EVT logging ON (should span several real VBLANK cycles -
     * real EE VBLANK cadence is roughly every ~294912 cycles at NTSC,
     * so a few million instructions covers several cycles) */
    uint64_t trace_window = (argc >= 4) ? strtoull(argv[3], NULL, 10) : 3000000ull;
    uint64_t chunk = (argc >= 5) ? strtoull(argv[4], NULL, 10) : 200000ull;

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_hle_thread_eventlog_set_enabled(0);
    ee_hle_thread_eventlog_set_filter(-1); /* log every thread, not just one */

    ee_state_t *ee = ee_core_get_state();

    fprintf(stderr, "[R1126] cold boot start, pre_budget=%llu trace_window=%llu chunk=%llu\n",
            (unsigned long long)pre_budget, (unsigned long long)trace_window, (unsigned long long)chunk);

    int frontier_hit = 0;
    uint64_t frontier_at = 0;
    uint64_t total = 0;

    /* Phase 1: run with EVT off, watching for the first pc==0x00257964 */
    while (total < pre_budget) {
        system_run_interleaved(chunk);
        total = ee->instructions_executed;
        if (ee->halted) {
            fprintf(stderr, "[R1126] EE halted during pre-phase at total_instr=%llu pc=0x%08x\n",
                    (unsigned long long)total, ee->pc);
            return 0;
        }
        if (ee->pc == 0x00257964u) {
            frontier_hit = 1;
            frontier_at = total;
            fprintf(stderr, "[R1126] FRONTIER HIT at total_instr=%llu pc=0x%08x - enabling EVT trace now\n",
                    (unsigned long long)total, ee->pc);
            break;
        }
    }

    if (!frontier_hit) {
        fprintf(stderr, "[R1126] frontier NOT reached within pre_budget=%llu (last pc=0x%08x, total=%llu) - aborting\n",
                (unsigned long long)pre_budget, ee->pc, (unsigned long long)total);
        return 1;
    }

    /* Round 1126 addendum: dump the FULL EE HLE thread table right at the
     * frontier, per-thread (status/wait_type/wait_id/priority), to answer
     * directly whether ANY thread is currently parked on semaphore 7 at
     * all (as opposed to inferring it indirectly from the WaitSema/Signal
     * event log, which only shows transitions, not steady per-thread
     * state). This is real ground truth, not inference. */
    {
        int n = ee_hle_thread_get_thread_count();
        fprintf(stderr, "[R1126TBL] === thread table snapshot at frontier_at=%llu (n_threads=%d) ===\n",
                (unsigned long long)frontier_at, n);
        for (int t = 0; t < n; t++) {
            uint32_t status = ee_hle_thread_get_status(t);
            uint32_t wtype = ee_hle_thread_get_wait_type(t);
            uint32_t wid = ee_hle_thread_get_wait_id(t);
            uint32_t prio = ee_hle_thread_get_priority(t);
            uint32_t saved_pc = ee_hle_thread_get_saved_pc(t);
            uint32_t entry = ee_hle_thread_get_entry(t);
            fprintf(stderr, "[R1126TBL] tid=%d status=0x%02x wait_type=%u wait_id=%u priority=%u saved_pc=0x%08x entry=0x%08x%s\n",
                    t, status, wtype, wid, prio, saved_pc, entry,
                    (wtype == 2 && wid == 7) ? "  <<<< PARKED ON SEMA 7" : "");
        }
    }

    /* Round 1126 addendum #2: direct ground-truth read of semaphore 7's
     * own live state (in_use/max_count/count/wait_threads) at the
     * frontier, via the existing accessor - not inferred from the EVT
     * log. This distinguishes two very different explanations for why
     * zero SignalSema(7) events were observed in the correlation window:
     *   (a) count==0: iSignalSema(7) genuinely never fired (the gating
     *       comparison inside the VBLANK-handler loop never lets the
     *       call through) - the producer side is the open question.
     *   (b) count>=1 with wait_threads>0: the semaphore DOES already
     *       hold a deliverable count, but tid=5's WaitSema block never
     *       consumed it - a genuine wakeup/WaitSema-side bug, not a
     *       producer gap. */
    for (int sid = 0; sid <= 15; sid++) {
        int in_use = 0; int32_t max_count = -1, count = -1, wait_threads = -1;
        if (ee_hle_thread_get_sema_state(sid, &in_use, &max_count, &count, &wait_threads) == 0 && in_use) {
            fprintf(stderr, "[R1126SEMA] semid=%d in_use=%d max_count=%d count=%d wait_threads=%d%s\n",
                    sid, in_use, max_count, count, wait_threads,
                    (sid == 7) ? "  <<<< SEMA 7" : "");
        }
    }

    /* Round 1126 addendum #3: dump the raw EE RAM struct that the VBLANK
     * handler's unrolled 3-block poll/relay sequence (0x00208088-0x00208174)
     * walks, at its computed base 0x0027B4E8 (s1=0x00280000, s0=s1-19224).
     * Each ~4-word entry holds the polled-upstream semid/handle fields
     * (offsets +0x30/+0x34/+0x38/+0x3C per block) - reading this directly
     * tells us which real upstream event source gates the sema-7 relay,
     * without further disassembly guesswork. */
    {
        uint32_t base = 0x0027B4E8u;
        fprintf(stderr, "[R1126STRUCT] dumping EE RAM 0x%08x..0x%08x (registration table walked by 0x00208088)\n",
                base, base + 0x80);
        for (uint32_t off = 0; off < 0x80; off += 4) {
            uint32_t v = ee_mem_read32(ee, base + off);
            fprintf(stderr, "[R1126STRUCT] [base+0x%02x] = 0x%08x (%u)\n", off, v, v);
        }
        uint32_t vblank_ctr1 = ee_mem_read32(ee, 0x0027B4A4u);
        uint32_t vblank_ctr2 = ee_mem_read32(ee, 0x002FCFE8u);
        fprintf(stderr, "[R1126STRUCT] vblank_ctr1@0x0027B4A4=%u vblank_ctr2@0x002FCFE8=%u\n",
                vblank_ctr1, vblank_ctr2);
    }

    /* Phase 2: enable EVT + ERET trace, run the bounded correlation window */
    ee_hle_thread_eventlog_set_enabled(1);
    uint64_t window_end = frontier_at + trace_window;
    uint64_t next_dump = frontier_at + 500000ull;
    while (ee->instructions_executed < window_end) {
        system_run_interleaved(chunk);
        if (ee->instructions_executed >= next_dump) {
            int n = ee_hle_thread_get_thread_count();
            fprintf(stderr, "[R1126TBL] === thread table snapshot at total_instr=%llu ===\n",
                    (unsigned long long)ee->instructions_executed);
            for (int t = 0; t < n; t++) {
                uint32_t status = ee_hle_thread_get_status(t);
                uint32_t wtype = ee_hle_thread_get_wait_type(t);
                uint32_t wid = ee_hle_thread_get_wait_id(t);
                uint32_t prio = ee_hle_thread_get_priority(t);
                fprintf(stderr, "[R1126TBL] tid=%d status=0x%02x wait_type=%u wait_id=%u priority=%u%s\n",
                        t, status, wtype, wid, prio,
                        (wtype == 2 && wid == 7) ? "  <<<< PARKED ON SEMA 7" : "");
            }
            next_dump += 500000ull;
        }
        if (ee->halted) {
            fprintf(stderr, "[R1126] EE halted during trace window at total_instr=%llu pc=0x%08x\n",
                    (unsigned long long)ee->instructions_executed, ee->pc);
            break;
        }
    }
    ee_hle_thread_eventlog_set_enabled(0);

    fprintf(stderr, "[R1126] trace window complete: frontier_at=%llu final_total=%llu final_pc=0x%08x\n",
            (unsigned long long)frontier_at, (unsigned long long)ee->instructions_executed, ee->pc);

    return 0;
}
