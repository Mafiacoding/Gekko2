#ifndef GEKKO2_PS2_MC_FILES_H
#define GEKKO2_PS2_MC_FILES_H
#include <stdint.h>
/* Standard 8 MiB PS2 FAT HLE, port 0/slot 0. Negative libmc errors;
 * caller validates port, slot and all EE DMA destinations before mutation. */
void ps2_mc_fs_reset(void);
int ps2_mc_fs_format(int unformat);
int ps2_mc_fs_open(const char *name,unsigned flags);
int ps2_mc_fs_close(int fd);
int ps2_mc_fs_seek(int fd,int32_t offset,unsigned origin);
int ps2_mc_fs_read_size(int fd,unsigned bytes);
int ps2_mc_fs_read(int fd,void *dest,unsigned bytes);
int ps2_mc_fs_write(int fd,const void *src,unsigned bytes);
int ps2_mc_fs_delete(const char *name);
int ps2_mc_fs_chdir(const char *name,char out[1024]);
int ps2_mc_fs_getdir(const char *pattern,unsigned flags,unsigned count,uint8_t *table);
int ps2_mc_fs_setinfo(const char *name,unsigned flags,const uint8_t table[64]);
int ps2_mc_fs_flush(int fd);
int ps2_mc_fs_getentspace(const char *name);
const void *ps2_mc_fs_snapshot(uint32_t *bytes);
int ps2_mc_fs_snapshot_valid(const void *data,uint32_t bytes);
int ps2_mc_fs_snapshot_load(const void *data,uint32_t bytes);
int ps2_mc_fs_busy(void);
#endif
