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

// Function: a12_imm_leaf
// Address: 0x100000 - 0x100020
void a12_imm_leaf_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a12_imm_leaf_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x3c088001  lui         $t0, 0x8001
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 8, (int32_t)((uint32_t)32769 << 16));
    // 0x100004: 0x3508ffff  ori         $t0, $t0, 0xFFFF
    ctx->pc = 0x100004u;
    SET_GPR_U64(ctx, 8, GPR_U64(ctx, 8) | (uint64_t)(uint16_t)65535);
    // 0x100008: 0x3109ff00  andi        $t1, $t0, 0xFF00
    ctx->pc = 0x100008u;
    SET_GPR_U64(ctx, 9, GPR_U64(ctx, 8) & (uint64_t)(uint16_t)65280);
    // 0x10000c: 0x392a1234  xori        $t2, $t1, 0x1234
    ctx->pc = 0x10000cu;
    SET_GPR_U64(ctx, 10, GPR_U64(ctx, 9) ^ (uint64_t)(uint16_t)4660);
    // 0x100010: 0x294bffff  slti        $t3, $t2, -0x1
    ctx->pc = 0x100010u;
    SET_GPR_U64(ctx, 11, ((int64_t)GPR_S64(ctx, 10) < (int64_t)(int32_t)4294967295) ? 1 : 0);
    // 0x100014: 0x2d4cffff  sltiu       $t4, $t2, -0x1
    ctx->pc = 0x100014u;
    SET_GPR_U64(ctx, 12, ((uint64_t)GPR_U64(ctx, 10) < (uint64_t)(int64_t)(int32_t)4294967295) ? 1 : 0);
    // 0x100018: 0x290d7fff  slti        $t5, $t0, 0x7FFF
    ctx->pc = 0x100018u;
    SET_GPR_U64(ctx, 13, ((int64_t)GPR_S64(ctx, 8) < (int64_t)(int32_t)32767) ? 1 : 0);
    // 0x10001c: 0x2c0e0001  sltiu       $t6, $zero, 0x1
    ctx->pc = 0x10001cu;
    SET_GPR_U64(ctx, 14, ((uint64_t)GPR_U64(ctx, 0) < (uint64_t)(int64_t)(int32_t)1) ? 1 : 0);
    ctx->pc = 0x100020u;
}


// Function: a12_reg_leaf
// Address: 0x100020 - 0x100040
void a12_reg_leaf_0x100020(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a12_reg_leaf_0x100020");
#endif

    ctx->pc = 0x100020u;

    // 0x100020: 0x1095025  or          $t2, $t0, $t1
    ctx->pc = 0x100020u;
    SET_GPR_U64(ctx, 10, GPR_U64(ctx, 8) | GPR_U64(ctx, 9));
    // 0x100024: 0x1095824  and         $t3, $t0, $t1
    ctx->pc = 0x100024u;
    SET_GPR_U64(ctx, 11, GPR_U64(ctx, 8) & GPR_U64(ctx, 9));
    // 0x100028: 0x1096027  nor         $t4, $t0, $t1
    ctx->pc = 0x100028u;
    SET_GPR_U64(ctx, 12, ~(GPR_U64(ctx, 8) | GPR_U64(ctx, 9)));
    // 0x10002c: 0x109682a  slt         $t5, $t0, $t1
    ctx->pc = 0x10002cu;
    SET_GPR_U64(ctx, 13, ((int64_t)GPR_S64(ctx, 8) < (int64_t)GPR_S64(ctx, 9)) ? 1 : 0);
    // 0x100030: 0x109702b  sltu        $t6, $t0, $t1
    ctx->pc = 0x100030u;
    SET_GPR_U64(ctx, 14, ((uint64_t)GPR_U64(ctx, 8) < (uint64_t)GPR_U64(ctx, 9)) ? 1 : 0);
    // 0x100034: 0x1097823  subu        $t7, $t0, $t1
    ctx->pc = 0x100034u;
    SET_GPR_S32(ctx, 15, (int32_t)SUB32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));
    // 0x100038: 0x14a5023  subu        $t2, $t2, $t2
    ctx->pc = 0x100038u;
    SET_GPR_S32(ctx, 10, (int32_t)SUB32(GPR_U32(ctx, 10), GPR_U32(ctx, 10)));
    // 0x10003c: 0x128482a  slt         $t1, $t1, $t0
    ctx->pc = 0x10003cu;
    SET_GPR_U64(ctx, 9, ((int64_t)GPR_S64(ctx, 9) < (int64_t)GPR_S64(ctx, 8)) ? 1 : 0);
    ctx->pc = 0x100040u;
}


// Function: a12_zero_dest
// Address: 0x100040 - 0x100074
void a12_zero_dest_0x100040(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a12_zero_dest_0x100040");
#endif

    ctx->pc = 0x100040u;

    // 0x100040: 0x3c000001  lui         $zero, 0x1
    ctx->pc = 0x100040u;
    SET_GPR_S32(ctx, 0, (int32_t)((uint32_t)1 << 16));
    // 0x100044: 0x35000005  ori         $zero, $t0, 0x5
    ctx->pc = 0x100044u;
    SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) | (uint64_t)(uint16_t)5);
    // 0x100048: 0x3100ffff  andi        $zero, $t0, 0xFFFF
    ctx->pc = 0x100048u;
    SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) & (uint64_t)(uint16_t)65535);
    // 0x10004c: 0x39000003  xori        $zero, $t0, 0x3
    ctx->pc = 0x10004cu;
    SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) ^ (uint64_t)(uint16_t)3);
    // 0x100050: 0x2900fffe  slti        $zero, $t0, -0x2
    ctx->pc = 0x100050u;
    SET_GPR_U64(ctx, 0, ((int64_t)GPR_S64(ctx, 8) < (int64_t)(int32_t)4294967294) ? 1 : 0);
    // 0x100054: 0x2d00fffe  sltiu       $zero, $t0, -0x2
    ctx->pc = 0x100054u;
    SET_GPR_U64(ctx, 0, ((uint64_t)GPR_U64(ctx, 8) < (uint64_t)(int64_t)(int32_t)4294967294) ? 1 : 0);
    // 0x100058: 0x1090025  or          $zero, $t0, $t1
    ctx->pc = 0x100058u;
    SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) | GPR_U64(ctx, 9));
    // 0x10005c: 0x1090024  and         $zero, $t0, $t1
    ctx->pc = 0x10005cu;
    SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) & GPR_U64(ctx, 9));
    // 0x100060: 0x1090027  nor         $zero, $t0, $t1
    ctx->pc = 0x100060u;
    SET_GPR_U64(ctx, 0, ~(GPR_U64(ctx, 8) | GPR_U64(ctx, 9)));
    // 0x100064: 0x109002a  slt         $zero, $t0, $t1
    ctx->pc = 0x100064u;
    SET_GPR_U64(ctx, 0, ((int64_t)GPR_S64(ctx, 8) < (int64_t)GPR_S64(ctx, 9)) ? 1 : 0);
    // 0x100068: 0x109002b  sltu        $zero, $t0, $t1
    ctx->pc = 0x100068u;
    SET_GPR_U64(ctx, 0, ((uint64_t)GPR_U64(ctx, 8) < (uint64_t)GPR_U64(ctx, 9)) ? 1 : 0);
    // 0x10006c: 0x1090023  subu        $zero, $t0, $t1
    ctx->pc = 0x10006cu;
    SET_GPR_S32(ctx, 0, (int32_t)SUB32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));
    // 0x100070: 0x35080000  ori         $t0, $t0, 0x0
    ctx->pc = 0x100070u;
    SET_GPR_U64(ctx, 8, GPR_U64(ctx, 8) | (uint64_t)(uint16_t)0);
    ctx->pc = 0x100074u;
}


