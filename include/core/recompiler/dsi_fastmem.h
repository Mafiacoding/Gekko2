/* GPL-3.0-or-later. Wii PPC MMU pilot; EE kernel-segment LW only. */
#ifndef GEKKO2_DSI_FASTMEM_H
#define GEKKO2_DSI_FASTMEM_H
#include <stdint.h>
#define GEKKO2_DSI_WINDOW 0x20000000u
#define GEKKO2_DSI_MARKER 0x47445349u
void dsi_fastmem_reserve(void);
void dsi_fastmem_init(void *ram,uint32_t bytes);
void dsi_fastmem_shutdown(void);
int dsi_fastmem_enabled(void);
int dsi_fastmem_status(void);
uint32_t dsi_fastmem_stat(unsigned n);
void dsi_fastmem_note_site(void);
void dsi_fastmem_note_block_site(void);
#if defined(GEKKO) && defined(GEKKO2_DSI_FASTMEM)
extern int g_gekko2_dsi_active;
extern uint32_t g_gekko2_dsi_block_prepares;
#endif
int gekko2_dsi_handle(uint32_t pc);
#endif
