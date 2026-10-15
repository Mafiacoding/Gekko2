#ifndef GEKKO2_CHD_READER_H
#define GEKKO2_CHD_READER_H
#include <stdint.h>
struct gekko2_chd;
int gekko2_chd_open(const char *path,struct gekko2_chd **out,uint32_t *sectors);
void gekko2_chd_close(struct gekko2_chd *disc);
int gekko2_chd_read(struct gekko2_chd *disc,uint32_t lba,uint8_t *sector);
uint64_t gekko2_chd_stat(unsigned index);
#endif
