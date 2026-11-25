/* GPL-3.0-or-later. Host smoke driver for g2_app.c: runs the portable run loop with a synthetic,
 * hand-built ROMDIR BIOS image (no Sony firmware). It proves the loop, init and display path link
 * and run; it says nothing about real BIOS behaviour or about Wii U / Cemu. */
#include "g2_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned log_lines, presents;
static void on_log(void *u, const char *s) { (void)u; log_lines++; printf("LOG %s\n", s); }
static int on_present(void *u, const uint32_t *p, unsigned w, unsigned h) { (void)u; (void)p; presents++; printf("PRESENT %ux%u\n", w, h); return 0; }
static int keep(void *u) { (void)u; return 1; }
static uint64_t now(void *u) { (void)u; struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000u + t.tv_nsec / 1000000u; }

static void entry(uint8_t *buf, uint32_t off, const char *name, uint32_t size)
{
    uint8_t *e = buf + off;
    memset(e, 0, 16);
    memcpy(e, name, strlen(name));
    e[12] = size & 0xff; e[13] = (size >> 8) & 0xff; e[14] = (size >> 16) & 0xff; e[15] = (size >> 24) & 0xff;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "synthetic-bios.bin";
    uint8_t *buf = calloc(1, 4u * 1024u * 1024u);
    if (!buf) return 1;
    entry(buf, 48, "RESET", 48); entry(buf, 64, "ROMDIR", 64); entry(buf, 80, "EXTINFO", 20); entry(buf, 96, "ROMVER", 16);
    memcpy(buf + 144, "0099ZZ99999999\n", 15);
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(buf, 1, 4u * 1024u * 1024u, f) != 4u * 1024u * 1024u) return 2;
    fclose(f);
    free(buf);
    g2_platform p = { on_log, on_present, keep, now, NULL };
    g2_config c = { path, NULL, 200, 5, 0 };
    g2_result r;
    int rc = g2_run(&c, &p, &r);
    printf("RESULT rc=%d status=%d batches=%llu ee_instr=%llu iop_instr=%llu frames=%u logs=%u\n", rc, r.status,
           (unsigned long long)r.batches, (unsigned long long)r.ee_instructions,
           (unsigned long long)r.iop_instructions, r.frames_presented, log_lines);
    int ok = rc == 0 && r.batches == 5 && r.ee_instructions > 0 && r.iop_instructions > 0;
    /* A missing BIOS must fail cleanly. */
    g2_config bad = { "/nonexistent/bios.bin", NULL, 200, 1, 0 };
    ok = ok && g2_run(&bad, &p, &r) == G2_ERR_BIOS;
    puts(ok ? "PASS g2_app host smoke" : "FAIL g2_app host smoke");
    return ok ? 0 : 1;
}
