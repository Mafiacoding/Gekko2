/* Round 1109: dump raw EE words around a given address from a checkpoint,
 * for feeding into tools/round655-ee-disasm/disasm.c. Read-only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/checkpoint.h"

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "usage: %s <bios_path> <ckpt_path> <start_addr_hex> <count> <out_bin>\n", argv[0]);
        return 1;
    }
    bios_image_t bios;
    if (bios_load(argv[1], &bios) != 0) { fprintf(stderr, "bios load fail\n"); return 1; }
    if (system_init(&bios, &bios) != 0) { fprintf(stderr, "system_init fail\n"); return 1; }
    if (checkpoint_load(argv[2], &bios, &bios, NULL) != 0) { fprintf(stderr, "ckpt load fail\n"); return 1; }
    ee_state_t *ee = ee_core_get_state();
    uint32_t start = (uint32_t)strtoul(argv[3], NULL, 16);
    long count = strtol(argv[4], NULL, 10);
    FILE *f = fopen(argv[5], "wb");
    if (!f) { fprintf(stderr, "open out fail\n"); return 1; }
    for (long i = 0; i < count; i++) {
        uint32_t addr = start + (uint32_t)(i * 4);
        uint32_t w = ee_mem_read32(ee, addr);
        fwrite(&w, 4, 1, f);
    }
    fclose(f);
    fprintf(stderr, "dumped %ld words from 0x%08x\n", count, start);
    return 0;
}
