/* GPL-3.0-or-later. Platform-independent run loop for the Wii U / Cemu test port.
 * Uses only the portable Gekko2 core (no libogc, no GX, no ARM/IOS). Interpreter only. */
#ifndef GEKKO2_WIIU_APP_H
#define GEKKO2_WIIU_APP_H
#include <stdint.h>

typedef struct {
    void (*log)(void *user, const char *line);
    /* xrgb is w*h pixels, 0x00RRGGBB. Return 0 on success. May be called every few batches. */
    int (*present)(void *user, const uint32_t *xrgb, unsigned w, unsigned h);
    int (*keep_running)(void *user);          /* 0 = leave the loop (HOME / close) */
    uint64_t (*now_ms)(void *user);
    /* Optional. Fills PS2 pressed-button bits (IOP_PAD_BTN_*, 1 = pressed) and the analog axes
     * rx, ry, lx, ly (0x80 = centre). Return 0 when no pad is connected. */
    int (*poll_pad)(void *user, uint16_t *pressed, uint8_t axes[4]);
    void *user;
} g2_platform;

typedef struct {
    const char *bios_path;                    /* raw 4 MiB PS2 BIOS dump, supplied by the user */
    const char *iso_path;                     /* optional disc image, NULL for BIOS only */
    unsigned batch_slices;                    /* scheduler slices per system_run_interleaved() call */
    uint64_t max_batches;                     /* 0 = run until keep_running() says stop */
    unsigned present_interval_ms;             /* minimum time between frames handed to present() */
    int use_jit;                              /* try the EE dynarec (only in GEKKO2_WIIU_JIT builds) */
} g2_config;

typedef struct {
    int status;                               /* 0 ok, otherwise a G2_ERR_* value */
    uint64_t batches, ee_instructions, iop_instructions;
    uint32_t ee_pc;
    unsigned frames_presented;
    unsigned last_w, last_h;
    int halted;
} g2_result;

enum { G2_OK = 0, G2_ERR_BIOS = 3, G2_ERR_INIT = 4, G2_ERR_DISC = 5 };

int g2_run(const g2_config *config, const g2_platform *platform, g2_result *result);
/* Reads the active GS display circuit into xrgb (capacity w_max*h_max). Returns 1 when a frame was read. */
int g2_read_display(uint32_t *xrgb, unsigned w_max, unsigned h_max, unsigned *w, unsigned *h);
#endif
