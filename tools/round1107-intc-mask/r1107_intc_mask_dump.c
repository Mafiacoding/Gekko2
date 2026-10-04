/*
 * r1107_intc_mask_dump.c - Round 1107 (task #1033 continuation): now
 * that ee_intc_raise() is confirmed (via source grep, see STATUS.md
 * Round 1107 entry) to genuinely fire for GS/SBUS/VBLANK_START/
 * VBLANK_END/Timer0-3 every real frame/event, the next question is
 * whether those raises are actually UNMASKED (INTC_MASK bit set) and
 * whether they're accumulating real hit counts at the checkpoint
 * these 7 SEMA-parked threads are resting at.
 *
 * Dumps: INTC_STAT, INTC_MASK, (stat & mask) pending-or-not, and the
 * real per-cause raise-hit counter (ee_intc_get_raise_count(), Round
 * 716) for all 15 real EE_INTC causes (GS=0 .. VU0WATCHDOG=14).
 *
 * Read-only. Checkpoint/BIOS paths are CLI arguments (leak-check
 * discipline - nothing BIOS/checkpoint-derived hardcoded here).
 *
 * Usage: r1107_intc_mask_dump <bios_path> <ckpt_path>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/ee_intc.h"
#include "core/checkpoint.h"
#include "core/system.h"

static const char *cause_name(int irq)
{
    switch (irq) {
        case 0: return "GS";
        case 1: return "SBUS";
        case 2: return "VBLANK_START";
        case 3: return "VBLANK_END";
        case 4: return "VIF0";
        case 5: return "VIF1";
        case 6: return "VU0";
        case 7: return "VU1";
        case 8: return "IPU";
        case 9: return "TIMER0";
        case 10: return "TIMER1";
        case 11: return "TIMER2";
        case 12: return "TIMER3";
        case 13: return "SFIFO";
        case 14: return "VU0WATCHDOG";
        default: return "?";
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path>\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    const char *ckpt_path = argv[2];

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(ckpt_path, &bios, &bios, NULL) != 0) {
        fprintf(stderr, "checkpoint_load FAILED for %s\n", ckpt_path);
        return 1;
    }

    ee_state_t *ee = ee_core_get_state();
    fprintf(stderr, "[R1107-INTC] loaded %s: total_instr=%llu pc=0x%08x halted=%d\n",
            ckpt_path, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    /* Round 1107 fix (self-caught methodology gap): ee_intc_get_raise_count()
     * is a live runtime counter (g_intc_raise_count[] in ee_intc.c) that is
     * NOT part of the checkpointed EINT block and resets to 0 at program
     * start (ee_intc_init(), called by system_init() before checkpoint_load()
     * restores stat/mask). Dumping it immediately after checkpoint_load()
     * without ever stepping the emulator forward only proves "zero raises
     * happened in the 0 instructions this process has executed so far" -
     * NOT "VBLANK never fires at this resting state". Must actually run the
     * emulator past at least one real NTSC frame boundary
     * (EE_CYCLES_PER_FRAME_NTSC=4921488, matching ee_core.c's own citation)
     * before the raise counts mean anything. */
    uint64_t before_instr = ee->instructions_executed;
    int slices_to_run = (argc >= 4) ? atoi(argv[3]) : 12000000; /* ~2.4 frames by default */
    fprintf(stderr, "[R1107-INTC] stepping %d slices forward from the checkpoint before sampling raise counts...\n", slices_to_run);
    system_run_interleaved((uint64_t)slices_to_run);
    fprintf(stderr, "[R1107-INTC] after stepping: total_instr=%llu (+%llu) pc=0x%08x halted=%d\n",
            (unsigned long long)ee->instructions_executed,
            (unsigned long long)(ee->instructions_executed - before_instr),
            ee->pc, ee->halted);

    ee_intc_state_t *intc = ee_intc_get_state();
    fprintf(stderr, "[R1107-INTC] INTC_STAT=0x%08x INTC_MASK=0x%08x pending(stat&mask)=%s\n",
            intc->stat, intc->mask, (intc->stat & intc->mask) ? "YES" : "no");
    fprintf(stderr, "[R1107-INTC] Status.IE=%d Status.EIE=%d Status.EXL=%d Cause.IP2=%d\n",
            (int)((ee->cop0[12] >> 0) & 1),
            (int)((ee->cop0[12] >> 16) & 1),
            (int)((ee->cop0[12] >> 1) & 1),
            (int)((ee->cop0[13] >> 10) & 1));

    fprintf(stderr, "[R1107-INTC] per-cause: stat_bit mask_bit raise_count\n");
    for (int i = 0; i < 15; i++) {
        int stat_bit = (intc->stat >> i) & 1;
        int mask_bit = (intc->mask >> i) & 1;
        uint32_t rc = ee_intc_get_raise_count(i);
        fprintf(stderr, "[R1107-INTC]   [%2d] %-14s stat=%d mask=%d raise_count=%u %s\n",
                i, cause_name(i), stat_bit, mask_bit, rc,
                (mask_bit && rc == 0) ? "<- unmasked but NEVER raised" :
                (!mask_bit && rc > 0) ? "<- raised but MASKED OFF" : "");
    }

    return 0;
}
