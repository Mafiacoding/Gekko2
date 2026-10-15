#ifndef GEKKO2_GX_DUMP_H
#define GEKKO2_GX_DUMP_H
#include <stdint.h>
void gs_gx_dump_record(const char *stage,uint32_t a,uint32_t b,uint32_t c,uint32_t d);
int gs_gx_dump_flush(const char *path);
void gs_gx_dump_reset(void);
#endif
