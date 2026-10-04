/*
 * Round 1140 (next "short cut" per user's incremental clockwork-mapping
 * instruction): disassemble the two hottest call targets found by
 * Round 1139's call-frequency map (0x00271698 and 0x00216258, tied at
 * 70,204 calls each in a 15M-instruction steady-state window) to find
 * out what they actually ARE - named/identified, not just addresses.
 *
 * Loads the real BIOS, boots to the documented diskless steady state,
 * then statically disassembles a bounded window (up to next `jr $ra`
 * or 200 instructions, whichever first) starting at each target.
 * Read-only. No patch, no skip.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"

#define main disasm_tool_unused_main
#include "../round655-ee-disasm/disasm.c"
#undef main

static void dump_func(ee_state_t *ee, uint32_t start, const char *label)
{
    printf("\n[R1140] === %s @ 0x%08X ===\n", label, start);
    uint32_t pc = start;
    for (int i = 0; i < 220; i++) {
        uint32_t instr = ee_mem_read32(ee, pc);
        char line[160];
        disasm_one(instr, pc, line, sizeof(line));
        printf("  0x%08X: %08X  %s\n", pc, instr, line);
        uint32_t op = instr >> 26;
        int is_jr = (op == 0 && (instr & 0x3F) == 0x08); /* JR */
        pc += 4;
        if (is_jr) {
            /* print the delay slot too, then stop */
            uint32_t dslot = ee_mem_read32(ee, pc);
            char dline[160];
            disasm_one(dslot, pc, dline, sizeof(dline));
            printf("  0x%08X: %08X  %s  (delay slot)\n", pc, dslot, dline);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bios_path>\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();
    (void)iop;

    uint64_t warm = 0, warm_step = 5000000ull, warm_cap = 70000000ull;
    while (warm < warm_cap && !ee->halted) {
        system_run_interleaved(warm_step);
        warm += warm_step;
        fprintf(stderr, "[R1140] warmup instr=%llu\n", (unsigned long long)warm);
    }
    printf("[R1140] warmup done: instr_count=%llu pc=0x%08X\n",
           (unsigned long long)ee->instructions_executed, ee->pc);

    dump_func(ee, 0x00271698u, "hot target #1 (70204 calls)");
    dump_func(ee, 0x00216258u, "hot target #2 (70204 calls)");
    dump_func(ee, 0x00263FE0u, "hot target #3 (33748 calls)");
    dump_func(ee, 0x00263EB0u, "hot target #4 (21131 calls)");

    return 0;
}
