#ifndef GEKKO2_PS2_MEMORY_CARD_H
#define GEKKO2_PS2_MEMORY_CARD_H
#include <stdint.h>
#define PS2_MC_PAGES 16384u
#define PS2_MC_PAGE_BYTES 528u
#define PS2_MC_IMAGE_BYTES (PS2_MC_PAGES*PS2_MC_PAGE_BYTES)
/* Port 0, slot 0; no implicit creation or formatting of user cards. */
int ps2_mc_mount(const char *path,int read_only);
int ps2_mc_flush(void);
int ps2_mc_eject(void);
int ps2_mc_present(void);
void ps2_mc_reset_protocol(void);
/* Full duplex wire transaction including address byte 81h. Missing cards,
 * unsupported commands and malformed transactions return High-Z (FF). */
void ps2_mc_transfer(const uint8_t *in,uint8_t *out,unsigned bytes);
void ps2_mc_ecc128(const uint8_t in[128],uint8_t out[3]);
/* Return 0 formatted, -2 unformatted, -11 absent, -13 malformed image. */
int ps2_mc_get_info(unsigned *formatted,unsigned *free_clusters);
const void *ps2_mc_snapshot(uint32_t *bytes);
int ps2_mc_snapshot_valid(const void *data,uint32_t bytes);
int ps2_mc_snapshot_load(const void *data,uint32_t bytes);
#endif
