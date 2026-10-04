/*
 * Round 1104 (task #1033, direct follow-up to the user's redirect after
 * Round 1103 closed the sema-11 starvation hypothesis): the user's own
 * framing is now "what does tid9 do AFTER a successful WaitSema(11),
 * and why doesn't that lead to further shared-BIOS-path progress?" -
 * not another investigation of the WaitSema(11) mechanics themselves
 * (Round 1103 settled those with direct trace evidence).
 *
 * This is a read-only checkpoint-load RAM dump tool, following the
 * Round 819 r819_ckpt_disasm.c precedent exactly, but adapted for the
 * DISKLESS SCPH-50004 boot path (no disc mount - this project's own
 * driver.c convention for the diskless survey family, e.g. Round 1100's
 * tools/round1100-park-correlate/driver.c) and for the specific
 * addresses this round needs: the WaitSema/SignalSema syscall wrapper
 * body around pc=0x00257954/0x00257964 (Round 1102/1103's park/producer
 * site) and tid9's own caller code around ra=0x0020eef0/0x0020eeac
 * (Round 1103's captured caller addresses for WAIT/SIGNAL respectively)
 * - to see what tid9's code actually does with the semaphore result
 * once WaitSema returns, and what tid2's code does right after its
 * SignalSema call.
 *
 * Never touches tracked source; purely a read-only diagnostic driver.
 * Checkpoint path is a command-line argument so nothing BIOS/checkpoint-
 * derived is hardcoded into this file itself (leak-check discipline).
 *
 * Usage: r1104_ckpt_dump <bios_path> <ckpt_path> <start_addr_hex> <len_hex> <out_file>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/checkpoint.h"

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path> <start_addr_hex> <len_hex> <out_file>\n", argv[0]);
        return 1;
    }
    const char *bios_path = argv[1];
    const char *ckpt_path = argv[2];
    uint32_t start_addr = (uint32_t)strtoul(argv[3], NULL, 16);
    uint32_t len = (uint32_t)strtoul(argv[4], NULL, 16);
    const char *out_file = argv[5];

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }

    if (checkpoint_load(ckpt_path, &bios, &bios, NULL) != 0) {
        fprintf(stderr, "checkpoint_load FAILED for %s\n", ckpt_path);
        return 1;
    }

    ee_state_t *ee = ee_core_get_state();
    fprintf(stderr, "[R1104-CKPT] loaded %s: total_instr=%llu pc=0x%08x halted=%d\n",
            ckpt_path, (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);

    for (int r = 0; r < 32; r++) {
        fprintf(stderr, "[R1104-CKPT] gpr[%d]=0x%08x", r, (uint32_t)ee->gpr[r].ud0);
        if ((r % 4) == 3) fprintf(stderr, "\n"); else fprintf(stderr, "  ");
    }

    fprintf(stderr, "[R1104-CKPT] current_thread_id=%d thread_count=%d\n",
            ee_hle_thread_get_current_thread_id(), ee_hle_thread_get_thread_count());
    int count = ee_hle_thread_get_thread_count();
    for (int t = 1; t <= count; t++) {
        fprintf(stderr, "[R1104-CKPT] tid=%d status=0x%x wait_type=%u wait_id=%u saved_pc=0x%08x prio=%u\n",
                t, ee_hle_thread_get_status(t), ee_hle_thread_get_wait_type(t),
                ee_hle_thread_get_wait_id(t), ee_hle_thread_get_saved_pc(t), ee_hle_thread_get_priority(t));
    }

    fprintf(stderr, "[R1104-CKPT] dumping 0x%08x len=0x%x to %s\n", start_addr, len, out_file);
    FILE *f = fopen(out_file, "wb");
    if (!f) { fprintf(stderr, "open fail\n"); return 1; }
    for (uint32_t i = 0; i < len; i++) {
        uint8_t b = ee_mem_read8(ee, start_addr + i);
        fputc(b, f);
    }
    fclose(f);
    return 0;
}
