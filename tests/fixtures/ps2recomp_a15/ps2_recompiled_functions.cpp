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

// Function: a15_lw_sw
// Address: 0x100000 - 0x100020
void a15_lw_sw_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a15_lw_sw_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x8c880000  lw          $t0, 0x0($a0)
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 8, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100004: 0x8c890004  lw          $t1, 0x4($a0)
    ctx->pc = 0x100004u;
    SET_GPR_S32(ctx, 9, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 4)));
    // 0x100008: 0x1095021  addu        $t2, $t0, $t1
    ctx->pc = 0x100008u;
    SET_GPR_S32(ctx, 10, (int32_t)ADD32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));
    // 0x10000c: 0xac8a0008  sw          $t2, 0x8($a0)
    ctx->pc = 0x10000cu;
    WRITE32(ADD32(GPR_U32(ctx, 4), 8), GPR_U32(ctx, 10));
    // 0x100010: 0x8c80000c  lw          $zero, 0xC($a0)
    ctx->pc = 0x100010u;
    SET_GPR_S32(ctx, 0, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 12)));
    // 0x100014: 0xac8afffc  sw          $t2, -0x4($a0)
    ctx->pc = 0x100014u;
    WRITE32(ADD32(GPR_U32(ctx, 4), 4294967292), GPR_U32(ctx, 10));
    // 0x100018: 0x8c8b7ffc  lw          $t3, 0x7FFC($a0)
    ctx->pc = 0x100018u;
    SET_GPR_S32(ctx, 11, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 32764)));
    // 0x10001c: 0xac800010  sw          $zero, 0x10($a0)
    ctx->pc = 0x10001cu;
    WRITE32(ADD32(GPR_U32(ctx, 4), 16), GPR_U32(ctx, 0));
    ctx->pc = 0x100020u;
}


// Function: a15_alias
// Address: 0x100020 - 0x10002c
void a15_alias_0x100020(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a15_alias_0x100020");
#endif

    ctx->pc = 0x100020u;

    // 0x100020: 0x8c840000  lw          $a0, 0x0($a0)
    ctx->pc = 0x100020u;
    SET_GPR_S32(ctx, 4, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100024: 0xac840004  sw          $a0, 0x4($a0)
    ctx->pc = 0x100024u;
    WRITE32(ADD32(GPR_U32(ctx, 4), 4), GPR_U32(ctx, 4));
    // 0x100028: 0x8ca5fff8  lw          $a1, -0x8($a1)
    ctx->pc = 0x100028u;
    SET_GPR_S32(ctx, 5, (int32_t)READ32(ADD32(GPR_U32(ctx, 5), 4294967288)));
    ctx->pc = 0x10002cu;
}


// Function: a15_ret_store_slot
// Address: 0x10002c - 0x100038
void a15_ret_store_slot_0x10002c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a15_ret_store_slot_0x10002c");
#endif

    ctx->pc = 0x10002cu;

    // 0x10002c: 0x8c820000  lw          $v0, 0x0($a0)
    ctx->pc = 0x10002cu;
    SET_GPR_S32(ctx, 2, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100030: 0x3e00008  jr          $ra
    ctx->pc = 0x100030u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100034u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100030u;
        // 0x100034: 0xaca20000  sw          $v0, 0x0($a1) (Delay Slot)
        WRITE32(ADD32(GPR_U32(ctx, 5), 0), GPR_U32(ctx, 2));
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100030u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100038u;
}


// Function: a15_ret_load_slot
// Address: 0x100038 - 0x100040
void a15_ret_load_slot_0x100038(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a15_ret_load_slot_0x100038");
#endif

    ctx->pc = 0x100038u;

    // 0x100038: 0x3e00008  jr          $ra
    ctx->pc = 0x100038u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x10003Cu;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100038u;
        // 0x10003c: 0x8c830008  lw          $v1, 0x8($a0) (Delay Slot)
        SET_GPR_S32(ctx, 3, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 8)));
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100038u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100040u;
}


// Function: a15_hinted
// Address: 0x100040 - 0x100050
void a15_hinted_0x100040(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a15_hinted_0x100040");
#endif

    ctx->pc = 0x100040u;

    // 0x100040: 0x3c088030  lui         $t0, 0x8030
    ctx->pc = 0x100040u;
    SET_GPR_S32(ctx, 8, (int32_t)((uint32_t)32816 << 16));
    // 0x100044: 0x8d090000  lw          $t1, 0x0($t0)
    ctx->pc = 0x100044u;
    SET_GPR_S32(ctx, 9, (int32_t)FAST_READ32(0x80300000u));
    // 0x100048: 0xad090004  sw          $t1, 0x4($t0)
    ctx->pc = 0x100048u;
    do { uint32_t _value = static_cast<uint32_t>(GPR_U32(ctx, 9)); ps2TraceGuestWrite(rdram, 0x80300004u, 4u, _value, 0u, "WRITE32", ctx); FAST_WRITE32(0x80300004u, _value); } while (0);
    // 0x10004c: 0x8c8a0000  lw          $t2, 0x0($a0)
    ctx->pc = 0x10004cu;
    SET_GPR_S32(ctx, 10, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 0)));
    ctx->pc = 0x100050u;
}


