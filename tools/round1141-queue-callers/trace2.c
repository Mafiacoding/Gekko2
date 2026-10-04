/*
 * Round 1141 - two short cuts combined, per user's "both, and good
 * we're getting closer to the menu via VIF1 DMA" steer:
 *
 * (A) Find the real callers of the queue-append wrapper (0x00216258)
 *     and the float decompose/round routines (0x00263FE0/0x00263EB0),
 *     to learn WHAT data is being queued into the VIF1 packet list
 *     each tick (text glyphs? carousel-icon vertices? both?) and what
 *     the float routines actually feed.
 *
 * (B) Fix Round 1139's MMIO-filter bug (it aliased KUSEG scratchpad
 *     addresses like 0x70000000 onto the real 0x10000000-0x1000FFFF
 *     physical MMIO window via a naive top-3-bit mask) and re-run the
 *     same instrumented window for a CLEAN real hardware-register
 *     write map. Correct rule: only KSEG0 (0x8000_0000-0x9FFF_FFFF)
 *     and KSEG1 (0xA000_0000-0xBFFF_FFFF) forms get physically
 *     unmasked (top 3 addr bits == 0b100 or 0b101); everything else
 *     (KUSEG, including the 0x7000_0000 scratchpad window) is
 *     compared to the MMIO window as-is, with no aliasing.
 *
 * Read-only. No patch/skip/forced-write.
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

/* ---- Part A: static J/JAL caller scan ---- */
static void scan_for_calls(ee_state_t *ee, uint32_t lo, uint32_t hi, uint32_t target, const char *label)
{
    int found = 0;
    for (uint32_t pc = lo; pc < hi; pc += 4) {
        uint32_t instr = ee_mem_read32(ee, pc);
        uint32_t op = instr >> 26;
        if (op == 2 || op == 3) { /* J or JAL */
            uint32_t idx = instr & 0x03FFFFFFu;
            uint32_t tgt = (pc & 0xF0000000u) | (idx << 2);
            if (tgt == target) {
                char line[160];
                disasm_one(instr, pc, line, sizeof(line));
                printf("  CALL SITE for %s: pc=0x%08X instr=0x%08X  %s\n", label, pc, instr, line);
                found++;
            }
        }
    }
    if (!found) printf("  (no direct J/JAL callers found for %s in 0x%08X-0x%08X)\n", label, lo, hi);
    else printf("  -> %d total call site(s) for %s\n", found, label);
}

/* ---- Part B: corrected MMIO filter ---- */
#define MMIO_LOG_CAP 2000
typedef struct { uint32_t pc, addr, val; uint8_t width; } mmio_event_t;
static mmio_event_t g_mmio_log[MMIO_LOG_CAP];
static int g_mmio_log_n = 0;
static uint64_t g_mmio_total = 0;

static void maybe_log_mmio_fixed(uint32_t pc, uint32_t addr, uint32_t val, int width)
{
    uint32_t top3 = addr >> 29; /* top 3 bits of the 32-bit address */
    uint32_t phys;
    if (top3 == 0x4u || top3 == 0x5u) {
        /* KSEG0 (100) or KSEG1 (101) - real physical alias forms */
        phys = addr & 0x1FFFFFFFu;
    } else {
        /* KUSEG or anything else - no bitmask aliasing, compare raw */
        phys = addr;
    }
    if (phys >= 0x10000000u && phys < 0x10010000u) {
        g_mmio_total++;
        if (g_mmio_log_n < MMIO_LOG_CAP) {
            g_mmio_log[g_mmio_log_n].pc = pc;
            g_mmio_log[g_mmio_log_n].addr = addr;
            g_mmio_log[g_mmio_log_n].val = val;
            g_mmio_log[g_mmio_log_n].width = (uint8_t)width;
            g_mmio_log_n++;
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

    uint64_t warm = 0, warm_step = 5000000ull, warm_cap = 70000000ull;
    while (warm < warm_cap && !ee->halted) {
        system_run_interleaved(warm_step);
        warm += warm_step;
        fprintf(stderr, "[R1141] warmup instr=%llu\n", (unsigned long long)warm);
    }
    printf("[R1141] warmup done: instr_count=%llu pc=0x%08X\n",
           (unsigned long long)ee->instructions_executed, ee->pc);

    /* --- Part A --- */
    printf("\n[R1141] === Part A: static callers (OSDSYS ELF range 0x00200000-0x00280000) ===\n");
    scan_for_calls(ee, 0x00200000u, 0x00280000u, 0x00216258u, "queue-append wrapper 0x00216258");
    scan_for_calls(ee, 0x00200000u, 0x00280000u, 0x00271698u, "queue-append primitive 0x00271698 (direct callers, bypassing wrapper)");
    scan_for_calls(ee, 0x00200000u, 0x00280000u, 0x00263FE0u, "float-decompose 0x00263FE0");
    scan_for_calls(ee, 0x00200000u, 0x00280000u, 0x00263EB0u, "float-round 0x00263EB0");

    /* --- Part B: corrected MMIO trace over a fresh 15M-instruction window --- */
    printf("\n[R1141] === Part B: corrected MMIO write map (15M-instr window) ===\n");
    uint64_t fine_budget = 15000000ull;
    for (uint64_t i = 0; i < fine_budget && !ee->halted; i++) {
        uint32_t pc = ee->pc;
        if (pc < 0x02000000u && (pc & 3) == 0) {
            uint32_t instr = ee_mem_read32(ee, pc);
            uint32_t op = instr >> 26;
            uint32_t base = (instr >> 21) & 0x1Fu;
            int32_t simm = (int16_t)(instr & 0xFFFFu);
            uint32_t addr = (uint32_t)ee->gpr[base].ud0 + (uint32_t)simm;
            switch (op) {
                case 0x28: maybe_log_mmio_fixed(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0 & 0xFFu, 1); break;
                case 0x29: maybe_log_mmio_fixed(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0 & 0xFFFFu, 2); break;
                case 0x2B: maybe_log_mmio_fixed(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0, 4); break;
                case 0x3F: maybe_log_mmio_fixed(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0, 8); break;
                default: break;
            }
        }
        for (int k = 0; k < 8; k++) { if (!ee->halted) ee_core_step(); }
        if (!iop->halted) iop_core_step();
    }

    printf("[R1141] total REAL MMIO writes this window: %llu (logged %d)\n",
           (unsigned long long)g_mmio_total, g_mmio_log_n);
    printf("[R1141] unique real MMIO addresses touched:\n");
    for (int i = 0; i < g_mmio_log_n; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) if (g_mmio_log[j].addr == g_mmio_log[i].addr) { dup = 1; break; }
        if (!dup) {
            uint32_t cnt = 0;
            for (int j = 0; j < g_mmio_log_n; j++) if (g_mmio_log[j].addr == g_mmio_log[i].addr) cnt++;
            printf("  addr=0x%08X width=%d writes_in_window=%u  (last: pc=0x%08X val=0x%08X)\n",
                   g_mmio_log[i].addr, g_mmio_log[i].width, cnt, g_mmio_log[i].pc, g_mmio_log[i].val);
        }
    }
    printf("\n[R1141] chronological real MMIO events (up to 60):\n");
    for (int i = 0; i < g_mmio_log_n && i < 60; i++) {
        printf("  #%3d pc=0x%08X addr=0x%08X val=0x%08X width=%d\n",
               i, g_mmio_log[i].pc, g_mmio_log[i].addr, g_mmio_log[i].val, g_mmio_log[i].width);
    }

    printf("\n[R1141] final state: instr_count=%llu pc=0x%08X halted=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
    return 0;
}
