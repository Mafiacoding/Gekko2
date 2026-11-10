#ifndef PCSX2WII_GS_H
#define PCSX2WII_GS_H

#include <stdint.h>

/* GS privileged state. Rasterization/VRAM live in gif.c/gs_mem.c and GX.
 * Access widths and pending-event IMR transitions follow PCSX2 GS.cpp
 * 091ab68708a555198d80a7216965481c7ddd0d98. */

typedef struct {
    uint64_t pmode;
    uint64_t smode1;
    uint64_t smode2;
    uint64_t srfsh;
    uint64_t synch1;
    uint64_t synch2;
    uint64_t syncv;
    uint64_t dispfb1;
    uint64_t display1;
    uint64_t dispfb2;
    uint64_t display2;
    uint64_t extbuf;
    uint64_t extdata;
    uint64_t extwrite;
    uint64_t bgcolor;
    uint64_t csr;
    uint64_t imr;
    uint64_t busdir;
    uint64_t siglblid;
} gs_state_t;

/* GS IMR masks live at bits 8..12; reset masks all five CSR events. */
#define GS_IMR_RESET_ALL_MASKED 0x1F00ull

void gs_init(void);
gs_state_t *gs_get_state(void);

/* Returns 1 if addr is one of the known GS privileged registers and
 * handles the read/write; 0 otherwise. */
int gs_mmio_read64(uint32_t addr, uint64_t *out_val);
int gs_mmio_write64(uint32_t addr, uint64_t val);
int gs_raise_event(unsigned event);
int gs_mmio_read(uint32_t addr,unsigned bytes,uint64_t *value);
int gs_mmio_write(uint32_t addr,unsigned bytes,uint64_t value);

#endif