#ifndef PCSX2WII_CDVD_CONFIG_H
#define PCSX2WII_CDVD_CONFIG_H
#include <stdint.h>
#define CDVD_CONFIG_SNAPSHOT_SIZE 344u
/* Configuration-bank subset of Mechacon NVRAM, not a complete NVM dump.
 * Three banks: 4, 2, 7 sixteen-byte blocks. Session/data have no host pointers. */
void cdvd_config_reset(void);
void cdvd_config_reset_session(void);
uint8_t cdvd_config_open(unsigned mode,unsigned bank,unsigned count);
uint8_t cdvd_config_close(void);
uint8_t cdvd_config_read(uint8_t out[16]);
uint8_t cdvd_config_write(const uint8_t in[16]);
unsigned cdvd_config_count(void);
/* 0 loaded, 1 new file, -1 invalid/unreadable. Invalid loads disable saving. */
int cdvd_config_bind_file(const char *path);
void cdvd_config_snapshot_save(uint8_t out[CDVD_CONFIG_SNAPSHOT_SIZE]);
int cdvd_config_snapshot_valid(const uint8_t in[CDVD_CONFIG_SNAPSHOT_SIZE]);
void cdvd_config_snapshot_load(const uint8_t in[CDVD_CONFIG_SNAPSHOT_SIZE]);
#endif
