#ifndef PCSX2WII_IOP_MODULE_METADATA_H
#define PCSX2WII_IOP_MODULE_METADATA_H
#include <stdint.h>
/* Read ELF32 little-endian SHT_MIPS_IOPMOD version/name without executing
 * the module. Fields match iop_elf.c: 6*u32 then u16 version then name. */
int iop_module_metadata(const uint8_t *image,uint32_t size,char name[64],uint16_t *version);
/* Resolve the literal returned by the supported legacy LOADFILE RPC 0xff
 * getter from its real ELF code/data. Never infer it from filenames. */
int iop_loadfile_protocol(const uint8_t *image,uint32_t size,uint32_t *version);
#endif
