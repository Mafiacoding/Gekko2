/*
 * Round 1134 (task: verify SIF0/RPC-END vs SIF2 hypothesis for the
 * long-standing GT3 disc-boot stall at 0x8000FC58/0x8000FA80).
 *
 * The user's directive asks for a "golden reference" comparison:
 * Round 1123 already proved, via a FRESH cold boot (no checkpoint
 * resume) with NO disc, that the current tree reaches deep, legitimate
 * OSDSYS internal state (pc=0x00257964, WaitSema on sema 7, 185 real
 * organic iSignalSema(7) calls from a VBLANK-driven producer) within
 * only ~130,000,000 instructions - i.e. it cleanly escapes the very
 * same shared 0x8000FA80-0x80012700 BIOS/OSDSYS idle-tick region that
 * Round 1131-1133 found GT3's disc-mounted boot permanently stuck
 * inside for 1,000,000,000+ instructions.
 *
 * This driver runs a FRESH cold boot (instr=0, no checkpoint resume,
 * matching Round 1123/1131's own methodology exactly) of GT3 with the
 * real disc mounted, checkpoint-chained across tool-call invocations,
 * logging:
 *   - every real LOADFILE RPC_CALL (devname/romname/result), via the
 *     existing R955_LOADFILE_NAME_TRACE hook
 *   - every rpc_bind_count/ncmd/scmd call-count change
 *   - the EE pc every 5,000,000 instructions
 *   - explicit flags the moment pc first enters the OSDSYS body range
 *     (0x00100000-0x00280000) and the moment (if ever) it reaches the
 *     already-known-legitimate frontier pc=0x00257964
 * so the exact instruction count and pc trajectory where GT3 diverges
 * from the successful diskless trajectory can be pinpointed.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/sif.h"
#include "core/checkpoint.h"

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *bios_path = argc > 1 ? argv[1] : "/tmp/r1131/bios.bin";
    const char *disc_path = argc > 2 ? argv[2] : "none";
    uint64_t target = argc > 3 ? strtoull(argv[3], NULL, 10) : 900000000ull;
    uint64_t slice  = argc > 4 ? strtoull(argv[4], NULL, 10) : 5000000ull;
    const char *ckpt_path = argc > 5 ? argv[5] : "/tmp/r1134/ckpt.bin";

    bios_image_t bios;
    if (bios_load(bios_path, &bios) != 0) { printf("[FAIL] could not load BIOS %s\n", bios_path); return 1; }
    int have_disc = (strcmp(disc_path, "none") != 0);

    FILE *probe = fopen(ckpt_path, "rb");
    int resumed = 0;
    if (probe) {
        fclose(probe);
        if (checkpoint_load(ckpt_path, &bios, &bios, have_disc ? disc_path : NULL) == 0) {
            resumed = 1;
            printf("[R1134] resumed from checkpoint %s\n", ckpt_path);
        } else {
            printf("[R1134] checkpoint_load(%s) FAILED, cold-booting\n", ckpt_path);
        }
    }
    if (!resumed) {
        if (system_init(&bios, &bios) != 0) { printf("[FAIL] system_init failed\n"); return 1; }
        if (have_disc) {
            if (iop_cdvd_mount_iso(disc_path) != 0) { printf("[FAIL] could not mount disc %s\n", disc_path); return 1; }
        }
        printf("[R1134] cold-booted fresh (disc=%s)\n", have_disc ? disc_path : "NONE");
    }

    ee_state_t *ee = ee_core_get_state();
    int seen_osdsys_body = 0, seen_frontier = 0;
    uint64_t last_ncmd = iop_cdvd_get_ncmd_call_count();
    uint64_t last_scmd = iop_cdvd_get_scmd_call_count();
    uint32_t last_bind_count = sif_cmd_iop_get_rpc_bind_count();

    for (uint64_t s = 0; ee->instructions_executed < target && !ee->halted; s++) {
        system_run_interleaved(slice / 8 + 1);
        uint64_t ncmd = iop_cdvd_get_ncmd_call_count();
        uint64_t scmd = iop_cdvd_get_scmd_call_count();
        uint32_t bindc = sif_cmd_iop_get_rpc_bind_count();
        if (ncmd != last_ncmd || scmd != last_scmd || bindc != last_bind_count) {
            printf("[R1134-EVT] instr=%llu ncmd=%llu(was %llu) scmd=%llu(was %llu) bind=%u(was %u)\n",
                   (unsigned long long)ee->instructions_executed,
                   (unsigned long long)ncmd, (unsigned long long)last_ncmd,
                   (unsigned long long)scmd, (unsigned long long)last_scmd,
                   bindc, last_bind_count);
            last_ncmd = ncmd; last_scmd = scmd; last_bind_count = bindc;
        }
        if (!seen_osdsys_body && ee->pc >= 0x00100000u && ee->pc < 0x00280000u) {
            seen_osdsys_body = 1;
            printf("[R1134-MILESTONE] first entry into OSDSYS body range at instr=%llu pc=0x%08x\n",
                   (unsigned long long)ee->instructions_executed, ee->pc);
        }
        if (!seen_frontier && ee->pc == 0x00257964u) {
            seen_frontier = 1;
            printf("[R1134-MILESTONE] REACHED KNOWN-GOOD FRONTIER pc=0x00257964 at instr=%llu\n",
                   (unsigned long long)ee->instructions_executed);
        }
        printf("[R1134-P] instr=%llu pc=0x%08x halted=%u\n",
               (unsigned long long)ee->instructions_executed, ee->pc, ee->halted);
        if (checkpoint_save(ckpt_path) != 0) {
            printf("[R1134] WARNING checkpoint_save failed at instr=%llu\n", (unsigned long long)ee->instructions_executed);
        }
    }
    printf("[R1134-SUMMARY] final instr=%llu pc=0x%08x halted=%u osdsys_body_seen=%d frontier_seen=%d\n",
           (unsigned long long)ee->instructions_executed, ee->pc, ee->halted, seen_osdsys_body, seen_frontier);
    return 0;
}
