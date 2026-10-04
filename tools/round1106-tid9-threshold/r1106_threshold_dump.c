/*
 * r1106_threshold_dump.c - Round 1106 (task #1033, second half of the
 * user's "check SIF1, then continue with tid9" instruction): directly
 * read the real threshold each SEMA-parked thread is blocked on,
 * instead of trying to find its caller via a static byte-pattern scan
 * (Round 1104's scan for `jal 0x0020eecc` across the full 1MB OSDSYS
 * region came back with zero matches - this tool sidesteps that dead
 * end entirely).
 *
 * Round 1104's own disassembly of the wait-side function
 * (0x0020eecc-0x0020ef0c) showed it loads its caller-supplied
 * threshold from $a0 into $s0 at entry, then blocks in a
 * WaitSema/re-check loop with $s0 held live in a callee-saved
 * register for the whole duration of the wait. Since a parked
 * thread's blocked syscall preserves its full register context (this
 * project's own ee_hle_thread_get_gpr() accessor - the same TCB
 * save/restore machinery Round 824/826's save_context fix already
 * hardened), $s0 (MIPS register 16) is STILL the live threshold value
 * for any thread currently parked inside this exact wait-loop shape,
 * with no caller-disassembly step needed at all.
 *
 * This also directly tests Round 1104's "shared subroutine vs 7
 * separate per-thread instances" open question: if every SEMA-parked
 * thread's saved $ra falls inside the same narrow address range
 * (0x0020eecc-0x0020ef0c), it's one shared subroutine (matching the
 * zero-JAL-match finding would then mean an indirect jalr, not
 * per-thread duplication); if the saved $ra addresses differ widely,
 * that supports the separate-instances hypothesis instead.
 *
 * Read-only. Checkpoint path is a CLI argument (leak-check discipline
 * - nothing BIOS/checkpoint-derived hardcoded here).
 *
 * Usage: r1106_threshold_dump <bios_path> <ckpt_path>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/checkpoint.h"

/* MIPS register indices */
#define REG_A0 4
#define REG_S0 16
#define REG_GP 28
#define REG_RA 31

static const char *wait_type_name(uint32_t wt)
{
    switch (wt) {
        case 0: return "NONE";
        case 1: return "SLEEP";
        case 2: return "SEMA";
        default: return "OTHER";
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
    fprintf(stderr, "[R1106-THRESH] loaded %s: total_instr=%llu pc=0x%08x halted=%d\n",
            ckpt_path, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    int count = ee_hle_thread_get_thread_count();
    fprintf(stderr, "[R1106-THRESH] thread_count=%d\n", count);

    for (int t = 1; t <= count; t++) {
        uint32_t status = ee_hle_thread_get_status(t);
        uint32_t wtype  = ee_hle_thread_get_wait_type(t);
        uint32_t wid    = ee_hle_thread_get_wait_id(t);
        uint32_t saved_pc = ee_hle_thread_get_saved_pc(t);
        uint32_t prio   = ee_hle_thread_get_priority(t);

        if (wtype != 2 /* SEMA (EE_TSW_SEMA=2, per source/core/ee/ee_hle_thread.c) */) {
            fprintf(stderr, "[R1106-THRESH] tid=%d status=0x%x wait_type=%s wait_id=%u saved_pc=0x%08x prio=%u (not SEMA, skipping register dump)\n",
                    t, status, wait_type_name(wtype), wid, saved_pc, prio);
            continue;
        }

        uint32_t s0 = (uint32_t)ee_hle_thread_get_gpr(t, REG_S0);
        uint32_t a0 = (uint32_t)ee_hle_thread_get_gpr(t, REG_A0);
        uint32_t gp = (uint32_t)ee_hle_thread_get_gpr(t, REG_GP);
        uint32_t ra = (uint32_t)ee_hle_thread_get_gpr(t, REG_RA);

        int in_known_wait_fn = (ra >= 0x0020eecc && ra <= 0x0020ef10);

        /* Round 1104's disassembly: counter slot is at gp-31084 */
        uint32_t counter_addr = gp - 31084u; /* Round 1106 correction (see STATUS.md): -31084 IS correct - this round briefly, incorrectly, "corrected" it to -31052 based on misreading an unrelated nearby store's immediate field. Re-disassembly of the real SIGNAL/WAIT function pair (0x0020ee84/0x0020eec8) confirms -31084 (encoding 0x8694) is the real offset both functions use. */
        uint32_t counter_val = 0;
        int counter_readable = (counter_addr >= 0x00100000u && counter_addr < 0x02000000u);
        if (counter_readable) {
            counter_val = ee_mem_read32(ee, counter_addr);
        }

        fprintf(stderr, "[R1106-THRESH] tid=%d wait_id(semid)=%u saved_pc=0x%08x saved_ra=0x%08x %s\n",
                t, wid, saved_pc, ra, in_known_wait_fn ? "(RA INSIDE Round-1104 wait-fn range 0x0020eecc-0x0020ef10)" : "(RA OUTSIDE that range - separate instance or different code path)");
        fprintf(stderr, "[R1106-THRESH]   $s0(threshold)=0x%08x (%u)  $a0=0x%08x  $gp=0x%08x\n", s0, s0, a0, gp);
        if (counter_readable) {
            fprintf(stderr, "[R1106-THRESH]   counter@[gp-31084]=0x%08x => addr=0x%08x val=%u  %s threshold (%u)\n",
                    counter_addr, counter_addr, counter_val,
                    (counter_val >= s0) ? ">=" : "<", s0);
        } else {
            fprintf(stderr, "[R1106-THRESH]   counter@[gp-31084]=0x%08x (OUT OF EE RAM RANGE - not read)\n", counter_addr);
        }
    }

    return 0;
}
