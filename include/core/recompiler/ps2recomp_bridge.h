#ifndef GEKKO2_PS2RECOMP_BRIDGE_H
#define GEKKO2_PS2RECOMP_BRIDGE_H
#include <stdint.h>
#include "core/ee/ee_core.h"
#define EE_RECOMP_BRIDGE_ABI_VERSION 1u
#define EE_RECOMP_BRIDGE_MAGIC 0x47524231u
#define EE_RECOMP_EE_STATE_ABI_VERSION 1u
#define EE_RECOMP_EE_STATE_PROFILE_INTEGER_LEAF 1u
#define EE_RECOMP_EE_STATE_FLAG_BRANCH_PENDING 0x00000001u
/* Opaque in-process handle. ee_state_t remains owned by Gekko2. */
typedef struct { uint32_t magic, abi_version, struct_size; void *opaque_state; } ee_recomp_bridge_t;
typedef enum {
 EE_RECOMP_BRIDGE_OK=0, EE_RECOMP_BRIDGE_INVALID_ARGUMENT=1,
 EE_RECOMP_BRIDGE_BAD_HANDLE=2, EE_RECOMP_BRIDGE_GUEST_FAULT=3,
 EE_RECOMP_BRIDGE_STATE_NOT_CLEAN=4,
 EE_RECOMP_BRIDGE_UNSUPPORTED_STATE=5
} ee_recomp_bridge_result_t;
/* Fixed-width snapshot for a conservative straight-line integer leaf.
 * This is a read-only view, not ee_state_t serialization or an execution
 * lease. GPR/HI/LO lanes are [ud0,ud1], low 64 bits first. */
typedef struct {
 uint32_t abi_version, struct_size, profile, flags;
 uint32_t pc, next_pc, sa_reg, reserved;
 ee_reg128_t gpr[32], hi, lo;
} ee_recomp_ee_state_snapshot_t;
ee_recomp_bridge_result_t ee_recomp_bridge_bind(ee_recomp_bridge_t *, ee_state_t *);
ee_recomp_bridge_result_t ee_recomp_bridge_validate(const ee_recomp_bridge_t *);
/* Snapshot only. Rejects delay-slot, exception, halted/idle, misaligned-PC,
 * nonzero-$zero and active VU0 states; never mutates guest state. */
ee_recomp_bridge_result_t ee_recomp_bridge_snapshot_integer_leaf(
 ee_recomp_bridge_t *,ee_recomp_ee_state_snapshot_t *);
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
/* Reserved compatibility entry. Currently fail-closed (always returns 0):
 * the bridge is not integrated into ee_step_budget()/ee_core_step_n(), and
 * must not advance EE retirement/clocks outside scheduler-owned accounting.
 * Use the precise interpreter/JIT path until a scheduler-owned executor exists. */
unsigned ee_recomp_bridge_begin_native_window(ee_recomp_bridge_t *,uint32_t,
 const uint32_t *,unsigned,unsigned,uint32_t);
#endif
