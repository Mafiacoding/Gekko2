/*
 * Round 1142 - user: "keep pushing, figure out why PMODE/DISP can't
 * call the VIF1" (i.e. why the real, confirmed-active VIF1/GIF DMA
 * traffic from Round 1141 never results in PMODE/DISPFB2/DISPLAY2
 * (the real GS-privileged display-config registers, 0x12000000-
 * 0x120000A0 - a DIFFERENT MMIO window than the 0x10000000-based DMA
 * channel registers traced last round) getting configured.
 *
 * ee_core.c's own Round 321 comment already documents that a real
 * PMODE/DISPFB2/DISPLAY2 write DOES exist in real PS2 code, at EE PC
 * 0x0050b420-0x0050b45c - but that address was observed on a DISC-BOOT
 * (Tekken) path, not the diskless SCPH-50004 OSDSYS path this project
 * has been tracing since Round 950. This tool checks, for THIS
 * diskless path specifically: (1) does PMODE/DISPFB2/DISPLAY2 ever get
 * written during an extended steady-state window; if not, (2) what is
 * the current live value of PMODE (to see if it's genuinely all-zero/
 * unconfigured, or something non-obvious); (3) a static scan for
 * direct GS-privileged-register store instructions (immediate-offset
 * stores whose base+imm could resolve near 0x12000000) anywhere in
 * the loaded OSDSYS ELF range, as a way to find OSDSYS's OWN
 * display-setup routine (distinct from the Tekken one already cited).
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
#include "core/hw/gs.h"

#define main disasm_tool_unused_main
#include "../round655-ee-disasm/disasm.c"
#undef main

#define GSLOG_CAP 200
typedef struct { uint64_t instr_at; uint32_t pc, addr; uint64_t val; } gs_event_t;
static gs_event_t g_gslog[GSLOG_CAP];
static int g_gslog_n = 0;

static void maybe_log_gs(ee_state_t *ee, uint32_t pc, uint32_t addr, uint64_t val)
{
    uint32_t top3 = addr >> 29;
    uint32_t phys = (top3 == 0x4u || top3 == 0x5u) ? (addr & 0x1FFFFFFFu) : addr;
    if (phys >= 0x12000000u && phys < 0x12002000u) {
        if (g_gslog_n < GSLOG_CAP) {
            g_gslog[g_gslog_n].instr_at = ee->instructions_executed;
            g_gslog[g_gslog_n].pc = pc;
            g_gslog[g_gslog_n].addr = phys;
            g_gslog[g_gslog_n].val = val;
            g_gslog_n++;
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
        fprintf(stderr, "[R1142] warmup instr=%llu\n", (unsigned long long)warm);
    }
    printf("[R1142] warmup done: instr_count=%llu pc=0x%08X\n",
           (unsigned long long)ee->instructions_executed, ee->pc);

    gs_state_t *gs = gs_get_state();
    printf("[R1142] LIVE GS state BEFORE extended window: pmode=0x%llX dispfb1=0x%llX display1=0x%llX dispfb2=0x%llX display2=0x%llX csr=0x%llX imr=0x%llX\n",
           (unsigned long long)gs->pmode, (unsigned long long)gs->dispfb1, (unsigned long long)gs->display1,
           (unsigned long long)gs->dispfb2, (unsigned long long)gs->display2,
           (unsigned long long)gs->csr, (unsigned long long)gs->imr);

    /* Extended instrumented window - longer than Round 1141's 15M, to
     * give a real PMODE-config routine (if reached only occasionally)
     * more chance to fire. */
    uint64_t fine_budget = 25000000ull;
    for (uint64_t i = 0; i < fine_budget && !ee->halted; i++) {
        uint32_t pc = ee->pc;
        if (pc < 0x02000000u && (pc & 3) == 0) {
            uint32_t instr = ee_mem_read32(ee, pc);
            uint32_t op = instr >> 26;
            uint32_t base = (instr >> 21) & 0x1Fu;
            int32_t simm = (int16_t)(instr & 0xFFFFu);
            uint32_t addr = (uint32_t)ee->gpr[base].ud0 + (uint32_t)simm;
            uint64_t rtval = ee->gpr[(instr>>16)&0x1F].ud0;
            switch (op) {
                case 0x28: maybe_log_gs(ee, pc, addr, rtval & 0xFFu); break;
                case 0x29: maybe_log_gs(ee, pc, addr, rtval & 0xFFFFu); break;
                case 0x2B: maybe_log_gs(ee, pc, addr, rtval & 0xFFFFFFFFu); break;
                case 0x3F: maybe_log_gs(ee, pc, addr, rtval); break;
                case 0x1F: /* SQ (quadword store) - GS regs are commonly written via SQ too */
                    maybe_log_gs(ee, pc, addr, ee->gpr[(instr>>16)&0x1F].ud0);
                    break;
                default: break;
            }
        }
        for (int k = 0; k < 8; k++) { if (!ee->halted) ee_core_step(); }
        if (!iop->halted) iop_core_step();
    }

    printf("[R1142] LIVE GS state AFTER extended window (instr=%llu, pc=0x%08X): pmode=0x%llX dispfb1=0x%llX display1=0x%llX dispfb2=0x%llX display2=0x%llX csr=0x%llX imr=0x%llX\n",
           (unsigned long long)ee->instructions_executed, ee->pc,
           (unsigned long long)gs->pmode, (unsigned long long)gs->dispfb1, (unsigned long long)gs->display1,
           (unsigned long long)gs->dispfb2, (unsigned long long)gs->display2,
           (unsigned long long)gs->csr, (unsigned long long)gs->imr);

    printf("[R1142] real GS-privileged-register writes observed this window: %d\n", g_gslog_n);
    for (int i = 0; i < g_gslog_n; i++) {
        printf("  #%3d instr=%llu pc=0x%08X addr=0x%08X val=0x%llX\n",
               i, (unsigned long long)g_gslog[i].instr_at, g_gslog[i].pc,
               g_gslog[i].addr, (unsigned long long)g_gslog[i].val);
    }

    return 0;
}
