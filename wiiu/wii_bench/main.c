/* GPL-3.0-or-later. Wii bench for the *portable* Gekko2 core (same code as the Wii U / Cemu test port:
 * no GEKKO, no GX, no ARM worker, interpreter only). Purpose: run identical code in Dolphin and in Cemu
 * to separate "emulator speed" from "Wii build extras". Not a release. No speed claim.
 * BIOS: sd:/pcsx2/bios/SCPH50004.bin | SCPH39001.bin | SCPH10000.bin | ps2bios.bin (user supplied). */
#include "g2_app.h"
#include "core/ee/ee_core.h"
#include "bench_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t t_start;
static unsigned frames;

static uint64_t now_ms(void *u) { (void)u; return bench_now_ms(); }
static void on_log(void *u, const char *s) { (void)u; if (s[0] == 't' || s[0] == 'B' || s[0] == 'l' || s[0] == 'd') puts(s); }
static int on_present(void *u, const uint32_t *p, unsigned w, unsigned h)
{
    (void)u; (void)p;
    if (!frames++) {
        ee_state_t *e = ee_core_get_state();
        printf("FIRST FRAME %ux%u at t=%llus EE instr=%llu\n", w, h,
               (unsigned long long)((now_ms(NULL) - t_start) / 1000u), (unsigned long long)e->instructions_executed);
    }
    return 0;
}
static int keep(void *u) { (void)u; return 1; }

static int readable(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return 0; fclose(f); return 1; }

int main(void)
{
    bench_video_init();
    puts("Gekko2 portable-core bench on Wii (interpreter, no GX/ARM). Not a release.");
    if (!bench_fat_init()) { puts("fatInitDefault failed"); for (;;) bench_wait(); }
    const char *cand[] = { "sd:/pcsx2/bios/SCPH50004.bin", "sd:/pcsx2/bios/SCPH39001.bin",
                           "sd:/pcsx2/bios/SCPH10000.bin", "sd:/pcsx2/bios/ps2bios.bin" };
    const char *bios = NULL;
    for (unsigned i = 0; i < 4 && !bios; i++) if (readable(cand[i])) bios = cand[i];
    if (!bios) { puts("No BIOS found in sd:/pcsx2/bios/"); for (;;) bench_wait(); }
    t_start = now_ms(NULL);
    g2_platform platform = { on_log, on_present, keep, now_ms, NULL, NULL };
    g2_config config = { bios, NULL, 2000u, 0, 1000u };
    g2_result result;
    g2_run(&config, &platform, &result);
    for (;;) bench_wait();
}
