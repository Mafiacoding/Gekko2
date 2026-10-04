/*
 * r1106_counter_writer_scan_v2.c - Round 1106 correction: the ORIGINAL
 * r1106_counter_writer_scan.c in this directory scanned for the WRONG
 * encoding (0x86B4 = -31052, a different nearby field) due to an
 * arithmetic slip earlier in this round. This is the CORRECTED scan,
 * for the real offset (-31084, encoding 0x8694) the SIGNAL/WAIT
 * function pair at 0x0020ee80-0x0020ef0c actually uses (confirmed by
 * direct disassembly - see STATUS.md Round 1106 correction entry).
 *
 * Result: exactly 2 hits, both already inside the SIGNAL/WAIT pair
 * itself (0x0020ee84, 0x0020eec8) - no external producer exists for
 * this counter; it is a self-contained generation-counter/condition-
 * variable reset pattern, not fed by any async IOP/VBLANK signal.
 * This structurally closes the WaitSema/SignalSema-mechanism-health
 * question for all 7 SEMA-parked threads, matching Round 1103's
 * direct-trace-based Fall B (non-bug) closure for semaphore 11
 * specifically.
 *
 * Read-only. Usage: r1106_counter_writer_scan_v2 <bios_path> <ckpt_path>
 */
#include <stdio.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/checkpoint.h"
int main(int argc, char **argv) {
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) return 1;
    if (system_init(&bios, &bios) != 0) return 1;
    if (checkpoint_load(argv[2], &bios, &bios, NULL) != 0) return 1;
    ee_state_t *ee = ee_core_get_state();
    int hits=0;
    for (uint32_t addr=0x00000000u; addr<0x02000000u; addr+=4) {
        uint32_t w = ee_mem_read32(ee, addr);
        if ((w & 0xFFE0FFFFu) == 0xAF808694u) {
            fprintf(stderr,"[SCAN-real-31084] HIT addr=0x%08x word=0x%08x -> sw $%d, -31084($gp)\n", addr, w, (int)((w>>16)&0x1F));
            hits++;
        }
    }
    fprintf(stderr, "[SCAN-real-31084] total hits across 0-0x02000000: %d\n", hits);
    return 0;
}
