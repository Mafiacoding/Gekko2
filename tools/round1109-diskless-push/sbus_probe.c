/* Round 1109 follow-up (user's explicit request): reconstruct the
 * 0xB000F000/SBUS gate state at the exact checkpoint where Round 1109's
 * push settled (EE pc creeping 0x8000FAE0-0x8000FE44, matching the
 * EE_SBUS_WAIT_LOOP_PC3=0x8000FE2C site already documented in ee_core.c
 * since Round 1076). Dumps live ee_intc_state_t.{stat,mask} and
 * dma_state_t.d_stat to determine whether the existing
 * ee_check_boot_unblock_sbus_wait() shortcut (fires on every visit to
 * any of the 3 known poll sites unless INTC_STAT bit1 is already set OR
 * DMAC_STAT bit 0x80 is already set) is actually firing here, or is
 * blocked by one of its own two guards. Read-only, no state mutation. */
#include <stdio.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/checkpoint.h"
#include "core/hw/ee_intc.h"
#include "core/hw/dma.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path>\n", argv[0]);
        return 1;
    }
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(argv[2], &bios, &bios, NULL) != 0) { fprintf(stderr, "ckpt load fail\n"); return 1; }

    ee_state_t *ee = ee_core_get_state();
    ee_intc_state_t *intc = ee_intc_get_state();
    dma_state_t *dma = dma_get_state();

    printf("total_instr=%llu pc=0x%08x halted=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
    printf("EE_INTC_STAT (0xB000F000) = 0x%08x   (bit1/SBUS = %d)\n",
           intc->stat, (intc->stat >> 1) & 1);
    printf("EE_INTC_MASK (0xB000F010) = 0x%08x   (bit1/SBUS = %d)\n",
           intc->mask, (intc->mask >> 1) & 1);
    printf("DMAC_STAT.d_stat = 0x%08x   (bit 0x80/SIF2 = %d)\n",
           dma->d_stat, (dma->d_stat & 0x80) ? 1 : 0);
    printf("---\n");
    printf("shortcut guard 1 (INTC_STAT bit1 already set) -> %s\n",
           (intc->stat & (1u << 1)) ? "BLOCKS shortcut (already set)" : "would NOT block");
    printf("shortcut guard 2 (DMAC_STAT bit 0x80 already set) -> %s\n",
           (dma->d_stat & 0x80u) ? "BLOCKS shortcut (already set)" : "would NOT block");
    if (!(intc->stat & (1u<<1)) && !(dma->d_stat & 0x80u))
        printf("=> shortcut SHOULD fire next time pc hits 0x8000CFCC/0x8000FD74/0x8000FE2C\n");
    else
        printf("=> shortcut is BLOCKED by a guard - INTC bit1 or DMAC bit0x80 already real-set\n");
    return 0;
}
