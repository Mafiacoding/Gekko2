/*
 * Round 1139 (user-directed, short incremental "clockwork" mapping
 * passes): "do everything in multiple short cuts ... we need to know
 * what is called where ... sometimes we need to do the work ourself."
 *
 * First short pass: a real, evidenced call-frequency + MMIO-write map
 * of steady-state diskless OSDSYS execution - not a guess, a direct
 * per-instruction trace over a real window late in the boot (matching
 * the already-documented ~385M-instruction resting state). This is
 * intentionally narrow in scope (one tool, one focused question) so it
 * can be verified and built on incrementally rather than attempting the
 * whole BIOS in one shot.
 *
 * What this pass answers:
 *  1. Which functions (JAL/JALR targets) are actually hot in steady
 *     state - a real call-frequency histogram, not the PC histogram
 *     Round 1136b already did (that counted every instruction; this
 *     counts only call targets, which is a much more useful "what does
 *     this loop actually DO" map).
 *  2. Every real MMIO write (0x1000xxxx / 0xB000xxxx sourced physical
 *     range) that happens in the window, with PC, address, and value -
 *     i.e. exactly what hardware registers OSDSYS's idle loop is
 *     genuinely touching every tick, which is the concrete "what does
 *     it write to IOP/EE RAM/hardware, and when" the user asked for.
 *
 * Read-only diagnostic. No patch, no skip, no forced write.
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

#define CALL_HIST_SIZE (0x02000000u / 4u)
static uint32_t *g_call_hist;

#define MMIO_LOG_CAP 4000
typedef struct { uint32_t pc, addr, val; uint8_t width; } mmio_event_t;
static mmio_event_t g_mmio_log[MMIO_LOG_CAP];
static int g_mmio_log_n = 0;
static uint64_t g_mmio_total = 0;

static void maybe_log_mmio(uint32_t pc, uint32_t addr, uint32_t val, int width)
{
    uint32_t phys = addr & 0x1FFFFFFFu;
    /* real PS2 hardware register window is 0x10000000-0x1000FFFF
     * physical (EE-side MMIO), per this project's own Memory_Map
     * convention already used throughout ee_core.c/dma.c/sif.c. */
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

    g_call_hist = calloc(CALL_HIST_SIZE, sizeof(uint32_t));
    if (!g_call_hist) { fprintf(stderr, "OOM\n"); return 1; }

    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    ee_state_t  *ee  = ee_core_get_state();
    iop_state_t *iop = iop_core_get_state();

    /* Warm up to steady state (matches Round 1136/1136b's own documented
     * ~385M-instruction resting point) - bulk speed via the real
     * scheduler, same convention as every prior survey driver. */
    uint64_t warm = 0, warm_step = 5000000ull, warm_cap = 70000000ull;
    while (warm < warm_cap && !ee->halted) {
        system_run_interleaved(warm_step);
        warm += warm_step;
        fprintf(stderr, "[R1139] warmup instr=%llu\n", (unsigned long long)warm);
    }

    printf("[R1139] warmup done: instr_count=%llu pc=0x%08X\n",
           (unsigned long long)ee->instructions_executed, ee->pc);

    /* Fine-grained instrumented window: log every JAL/JALR target
     * (call-frequency map) and every genuine MMIO write (address,
     * value, width, calling pc). */
    uint64_t fine_budget = 15000000ull;
    for (uint64_t i = 0; i < fine_budget && !ee->halted; i++) {
        uint32_t pc = ee->pc;
        if (pc < 0x02000000u && (pc & 3) == 0) {
            uint32_t instr = ee_mem_read32(ee, pc);
            uint32_t op = instr >> 26;
            uint32_t target = 0xFFFFFFFFu;
            if (op == 3) { /* JAL */
                uint32_t idx = instr & 0x03FFFFFFu;
                target = (pc & 0xF0000000u) | (idx << 2);
            } else if (op == 0 && (instr & 0x3F) == 0x09) { /* JALR */
                uint32_t rs = (instr >> 21) & 0x1Fu;
                target = (uint32_t)ee->gpr[rs].ud0;
            }
            if (target != 0xFFFFFFFFu) {
                uint32_t tp = target & 0x1FFFFFFFu;
                if (tp < 0x02000000u && (tp & 3) == 0) g_call_hist[tp / 4]++;
            }
            /* detect store instructions targeting MMIO before they
             * execute, by decoding opcode + base register + imm (this
             * mirrors what ee_mem_write* would receive) */
            uint32_t base = (instr >> 21) & 0x1Fu;
            int32_t simm = (int16_t)(instr & 0xFFFFu);
            uint32_t addr = (uint32_t)ee->gpr[base].ud0 + (uint32_t)simm;
            switch (op) {
                case 0x28: /* SB */ maybe_log_mmio(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0 & 0xFFu, 1); break;
                case 0x29: /* SH */ maybe_log_mmio(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0 & 0xFFFFu, 2); break;
                case 0x2B: /* SW */ maybe_log_mmio(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0, 4); break;
                case 0x3F: /* SD */ maybe_log_mmio(pc, addr, (uint32_t)ee->gpr[(instr>>16)&0x1F].ud0, 8); break;
                default: break;
            }
        }
        for (int k = 0; k < 8; k++) { if (!ee->halted) ee_core_step(); }
        if (!iop->halted) iop_core_step();
    }

    printf("\n[R1139] === call-frequency map (top 40 JAL/JALR targets over %llu-instruction window) ===\n",
           (unsigned long long)fine_budget);
    typedef struct { uint32_t pc; uint32_t cnt; } ent_t;
    int topn = 40;
    ent_t *top = calloc((size_t)topn, sizeof(ent_t));
    uint64_t unique = 0;
    for (uint32_t i = 0; i < CALL_HIST_SIZE; i++) {
        uint32_t cnt = g_call_hist[i];
        if (!cnt) continue;
        unique++;
        uint32_t pc = i * 4;
        int min_idx = -1; uint32_t min_cnt = 0xFFFFFFFFu;
        for (int j = 0; j < topn; j++) {
            if (top[j].cnt == 0) { min_idx = j; min_cnt = 0; break; }
            if (top[j].cnt < min_cnt) { min_cnt = top[j].cnt; min_idx = j; }
        }
        if (cnt > min_cnt) { top[min_idx].pc = pc; top[min_idx].cnt = cnt; }
    }
    for (int i = 1; i < topn; i++) {
        ent_t key = top[i]; int j = i - 1;
        while (j >= 0 && top[j].cnt < key.cnt) { top[j+1] = top[j]; j--; }
        top[j+1] = key;
    }
    printf("[R1139] unique call targets this window: %llu\n", (unsigned long long)unique);
    for (int i = 0; i < topn; i++) {
        if (!top[i].cnt) break;
        printf("  #%2d  target=0x%08X  calls=%u\n", i + 1, top[i].pc, top[i].cnt);
    }
    free(top);

    printf("\n[R1139] === MMIO write log (0x10000000-0x1000FFFF physical) ===\n");
    printf("[R1139] total MMIO writes this window: %llu (logged %d, capped at %d)\n",
           (unsigned long long)g_mmio_total, g_mmio_log_n, MMIO_LOG_CAP);
    /* print a compact unique (addr,width) summary first */
    printf("[R1139] unique MMIO addresses touched:\n");
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

    printf("\n[R1139] first 30 raw MMIO events (chronological):\n");
    for (int i = 0; i < g_mmio_log_n && i < 30; i++) {
        printf("  #%3d pc=0x%08X addr=0x%08X val=0x%08X width=%d\n",
               i, g_mmio_log[i].pc, g_mmio_log[i].addr, g_mmio_log[i].val, g_mmio_log[i].width);
    }

    printf("\n[R1139] final state: instr_count=%llu pc=0x%08X halted=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    return 0;
}
