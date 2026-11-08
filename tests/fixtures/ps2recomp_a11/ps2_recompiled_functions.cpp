#include <stdexcept>
#include <ps2_recompiled_functions.h>

#include "ps2_runtime_macros.h"
#include "ps2_runtime.h"
#include <ps2_recompiled_stubs.h>
#include "ps2_syscalls.h"
#include "ps2_stubs.h"
#ifdef _DEBUG
#include "ps2_log.h"
#endif

// Function: a11_int_leaf
// Address: 0x100000 - 0x100018
void a11_int_leaf_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a11_int_leaf_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x24081234  addiu       $t0, $zero, 0x1234
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 8, (int32_t)ADD32(GPR_U32(ctx, 0), 4660));
    // 0x100004: 0x2509ffff  addiu       $t1, $t0, -0x1
    ctx->pc = 0x100004u;
    SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 8), 4294967295));
    // 0x100008: 0x1095021  addu        $t2, $t0, $t1
    ctx->pc = 0x100008u;
    SET_GPR_S32(ctx, 10, (int32_t)ADD32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));
    // 0x10000c: 0x1485826  xor         $t3, $t2, $t0
    ctx->pc = 0x10000cu;
    SET_GPR_U64(ctx, 11, GPR_U64(ctx, 10) ^ GPR_U64(ctx, 8));
    // 0x100010: 0xb6100  sll         $t4, $t3, 4
    ctx->pc = 0x100010u;
    SET_GPR_S32(ctx, 12, (int32_t)SLL32(GPR_U32(ctx, 11), 4));
    // 0x100014: 0x0  nop
    ctx->pc = 0x100014u;
    // NOP
    ctx->pc = 0x100018u;
}


// Function: a11_zero_dest
// Address: 0x100018 - 0x100034
void a11_zero_dest_0x100018(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a11_zero_dest_0x100018");
#endif

    ctx->pc = 0x100018u;

    // 0x100018: 0x1090021  addu        $zero, $t0, $t1
    ctx->pc = 0x100018u;
    SET_GPR_S32(ctx, 0, (int32_t)ADD32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));
    // 0x10001c: 0x1090026  xor         $zero, $t0, $t1
    ctx->pc = 0x10001cu;
    SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) ^ GPR_U64(ctx, 9));
    // 0x100020: 0x25088000  addiu       $t0, $t0, -0x8000
    ctx->pc = 0x100020u;
    SET_GPR_S32(ctx, 8, (int32_t)ADD32(GPR_U32(ctx, 8), 4294934528));
    // 0x100024: 0x94fc0  sll         $t1, $t1, 31
    ctx->pc = 0x100024u;
    SET_GPR_S32(ctx, 9, (int32_t)SLL32(GPR_U32(ctx, 9), 31));
    // 0x100028: 0x25297fff  addiu       $t1, $t1, 0x7FFF
    ctx->pc = 0x100028u;
    SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 9), 32767));
    // 0x10002c: 0x14a5021  addu        $t2, $t2, $t2
    ctx->pc = 0x10002cu;
    SET_GPR_S32(ctx, 10, (int32_t)ADD32(GPR_U32(ctx, 10), GPR_U32(ctx, 10)));
    // 0x100030: 0x16b5826  xor         $t3, $t3, $t3
    ctx->pc = 0x100030u;
    SET_GPR_U64(ctx, 11, GPR_U64(ctx, 11) ^ GPR_U64(ctx, 11));
    ctx->pc = 0x100034u;
}


