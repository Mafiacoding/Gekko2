#ifndef PCSX2WII_ISO_LOADER_H
#define PCSX2WII_ISO_LOADER_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ISO9660 game-disc image reader. Supports plain 2048-byte sectors,
 * raw 2352-byte Mode 1 (payload +16) and Mode 2 Form 1 (payload +24),
 * detected by the ISO9660 PVD at LBA 16. Directory traversal accepts
 * slash/backslash components plus cdrom:/cdrom0:/cdrom1: prefixes.
 * The leaf component gets a ;1 fallback when no explicit version is
 * supplied. Rock Ridge/Joliet, multi-session and multi-extent files
 * remain outside this module's scope. Live CDVD state is owned by
 * iop_cdvd.c; opening an image here never fabricates disc presence. */
#define ISO_SECTOR_SIZE 2048u
#define ISO_PVD_LBA 16u
#define ISO_RAW_SECTOR_SIZE 2352u
#define ISO_RAW_MODE1_DATA_OFFSET 16u
#define ISO_RAW_MODE2_DATA_OFFSET 24u

typedef struct {uint32_t lba;uint32_t size;uint8_t is_directory;char name[224];} iso_dirent_t;
typedef struct {FILE *fp;uint32_t root_lba;uint32_t root_size;uint8_t opened;uint32_t physical_stride;uint32_t data_offset;} iso_image_t;

int iso_open(const char *path,iso_image_t *out);
void iso_close(iso_image_t *img);
/* Returns -1 for invalid state, I/O failure, or an LBA whose complete
 * 2048-byte logical payload is outside the physical image. */
int iso_read_sector(iso_image_t *img,uint32_t lba,uint8_t *buf);
int iso_find_in_root(iso_image_t *img,const char *name,iso_dirent_t *out);
int iso_find_path(iso_image_t *img,const char *path,iso_dirent_t *out);
#endif
