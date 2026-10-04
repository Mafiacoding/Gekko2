/*
 * r1106_counter_writer_scan.c - Round 1106 (task #1033 continuation):
 * static byte-pattern scan for any `sw $rt, -31084($gp)`-style store
 * instruction across the OSDSYS module region (0x00200000-0x00300000,
 * same window Round 1104's failed JAL scan covered), targeting the
 * exact shared counter slot this round's r1106_threshold_dump.c
 * concretely measured at EE vaddr 0x002c5004 (== $gp(0x002cc970) -
 * 31084, confirmed identical for all 7 real SEMA-parked threads at
 * the r1103_seq.ckpt resting point).
 *
 * Encoding: MIPS SW opcode=0x2B(101011), rs=28($gp), rt=variable,
 * imm=-31084 (0x86B4 as a 16-bit two's-complement immediate). Full
 * word = 0xAF800000 | (rt<<16) | 0x86B4 for rt=0; masking off the
 * 5-bit rt field (bits 20-16) gives the match test
 * (word & 0xFFE0FFFF) == 0xAF8086B4, independent of which specific
 * register is being stored.
 *
 * Read-only. Checkpoint/BIOS paths are CLI arguments.
 * Usage: r1106_counter_writer_scan <bios_path> <ckpt_path>
 */
#include <stdio.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/checkpoint.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path>\n", argv[0]);
        return 1;
    }
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(argv[2], &bios, &bios, NULL) != 0) {
        fprintf(stderr, "checkpoint_load FAILED for %s\n", argv[2]);
        return 1;
    }
    ee_state_t *ee = ee_core_get_state();

    uint32_t start = 0x00200000u, end = 0x00300000u;
    int hits = 0;
    for (uint32_t addr = start; addr < end; addr += 4) {
        uint32_t w = ee_mem_read32(ee, addr);
        if ((w & 0xFFE0FFFFu) == 0xAF8086B4u) {
            int rt = (int)((w >> 16) & 0x1Fu);
            fprintf(stderr, "[R1106-SCAN] HIT addr=0x%08x word=0x%08x -> sw $%d, -31084($gp)\n", addr, w, rt);
            hits++;
        }
    }
    fprintf(stderr, "[R1106-SCAN] total hits in 0x%08x-0x%08x: %d\n", start, end, hits);

    /* Widen if nothing found in the primary module window - check the
     * full addressable low 32MB EE RAM range too, since this counter
     * slot's writer might live outside the specific 1MB OSDSYS window
     * Round 1104 originally scanned (e.g. in a different, earlier-
     * loaded module). */
    if (hits == 0) {
        fprintf(stderr, "[R1106-SCAN] zero hits in primary window - widening to 0x00000000-0x02000000\n");
        int wide_hits = 0;
        for (uint32_t addr = 0x00000000u; addr < 0x02000000u; addr += 4) {
            uint32_t w = ee_mem_read32(ee, addr);
            if ((w & 0xFFE0FFFFu) == 0xAF8086B4u) {
                int rt = (int)((w >> 16) & 0x1Fu);
                fprintf(stderr, "[R1106-SCAN-WIDE] HIT addr=0x%08x word=0x%08x -> sw $%d, -31084($gp)\n", addr, w, rt);
                wide_hits++;
            }
        }
        fprintf(stderr, "[R1106-SCAN] total wide hits: %d\n", wide_hits);
    }
    return 0;
}
