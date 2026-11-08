#ifndef GEKKO2_PS2RECOMP_BRIDGE_H
#define GEKKO2_PS2RECOMP_BRIDGE_H
#include <stdint.h>
#include "core/ee/ee_core.h"
#define EE_RECOMP_BRIDGE_ABI_VERSION 1u
#define EE_RECOMP_BRIDGE_MAGIC 0x47524231u
/* Opaque in-process handle. ee_state_t remains owned by Gekko2. */
typedef struct { uint32_t magic, abi_version, struct_size; void *opaque_state; } ee_recomp_bridge_t;
typedef enum {
 EE_RECOMP_BRIDGE_OK=0, EE_RECOMP_BRIDGE_INVALID_ARGUMENT=1,
 EE_RECOMP_BRIDGE_BAD_HANDLE=2, EE_RECOMP_BRIDGE_GUEST_FAULT=3,
 EE_RECOMP_BRIDGE_STATE_NOT_CLEAN=4
} ee_recomp_bridge_result_t;
ee_recomp_bridge_result_t ee_recomp_bridge_bind(ee_recomp_bridge_t *, ee_state_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_validate(const ee_recomp_bridge_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_read8(ee_recomp_bridge_t *,uint32_t,uint8_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_read16(ee_recomp_bridge_t *,uint32_t,uint16_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_read32(ee_recomp_bridge_t *,uint32_t,uint32_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_read64(ee_recomp_bridge_t *,uint32_t,uint64_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_read128(ee_recomp_bridge_t *,uint32_t,ee_reg128_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_write8(ee_recomp_bridge_t *,uint32_t,uint8_t);
ee_recomp_bridge_result_t ee_recomp_bridge_write16(ee_recomp_bridge_t *,uint32_t,uint16_t);
ee_recomp_bridge_result_t ee_recomp_bridge_write32(ee_recomp_bridge_t *,uint32_t,uint32_t);
ee_recomp_bridge_result_t ee_recomp_bridge_write64(ee_recomp_bridge_t *,uint32_t,uint64_t);
ee_recomp_bridge_result_t ee_recomp_bridge_write128(ee_recomp_bridge_t *,uint32_t,const ee_reg128_t *);
#endif
