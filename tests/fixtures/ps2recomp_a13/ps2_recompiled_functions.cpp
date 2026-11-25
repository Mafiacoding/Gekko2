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

// Function: a13_ret_nop
// Address: 0x100000 - 0x100010
void a13_ret_nop_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a13_ret_nop_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x24020007  addiu       $v0, $zero, 0x7
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 0), 7));
    // 0x100004: 0x851021  addu        $v0, $a0, $a1
    ctx->pc = 0x100004u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 4), GPR_U32(ctx, 5)));
    // 0x100008: 0x3e00008  jr          $ra
    ctx->pc = 0x100008u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100008u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100010u;
}


// Function: a13_ret_slot
// Address: 0x100010 - 0x10001c
void a13_ret_slot_0x100010(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a13_ret_slot_0x100010");
#endif

    ctx->pc = 0x100010u;

    // 0x100010: 0x851026  xor         $v0, $a0, $a1
    ctx->pc = 0x100010u;
    SET_GPR_U64(ctx, 2, GPR_U64(ctx, 4) ^ GPR_U64(ctx, 5));
    // 0x100014: 0x3e00008  jr          $ra
    ctx->pc = 0x100014u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100018u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100014u;
        // 0x100018: 0x24420001  addiu       $v0, $v0, 0x1 (Delay Slot)
        SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 1));
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100014u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10001Cu;
}


// Function: a13_ret_only
// Address: 0x10001c - 0x100024
void a13_ret_only_0x10001c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a13_ret_only_0x10001c");
#endif

    ctx->pc = 0x10001cu;

    // 0x10001c: 0x3e00008  jr          $ra
    ctx->pc = 0x10001Cu;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100020u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x10001Cu;
        // 0x100020: 0x3403beef  ori         $v1, $zero, 0xBEEF (Delay Slot)
        SET_GPR_U64(ctx, 3, GPR_U64(ctx, 0) | (uint64_t)(uint16_t)48879);
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x10001Cu, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100024u;
}


// Function: a13_plain_hex
// Address: 0x100024 - 0x10002c
void a13_plain_hex_0x100024(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a13_plain_hex_0x100024");
#endif

    ctx->pc = 0x100024u;

    // 0x100024: 0x24840001  addiu       $a0, $a0, 0x1
    ctx->pc = 0x100024u;
    SET_GPR_S32(ctx, 4, (int32_t)ADD32(GPR_U32(ctx, 4), 1));
    // 0x100028: 0x0  nop
    ctx->pc = 0x100028u;
    // NOP
    ctx->pc = 0x10002cu;
}


// Function: a13_ra_slot
// Address: 0x10002c - 0x100038
void a13_ra_slot_0x10002c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a13_ra_slot_0x10002c");
#endif

    ctx->pc = 0x10002cu;

    // 0x10002c: 0x27e20000  addiu       $v0, $ra, 0x0
    ctx->pc = 0x10002cu;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 31), 0));
    // 0x100030: 0x3e00008  jr          $ra
    ctx->pc = 0x100030u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100034u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100030u;
        // 0x100034: 0x341f1234  ori         $ra, $zero, 0x1234 (Delay Slot)
        SET_GPR_U64(ctx, 31, GPR_U64(ctx, 0) | (uint64_t)(uint16_t)4660);
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


// Function: a13_nop_alias_slot
// Address: 0x100038 - 0x100044
void a13_nop_alias_slot_0x100038(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a13_nop_alias_slot_0x100038");
#endif

    ctx->pc = 0x100038u;

    // 0x100038: 0x85182b  sltu        $v1, $a0, $a1
    ctx->pc = 0x100038u;
    SET_GPR_U64(ctx, 3, ((uint64_t)GPR_U64(ctx, 4) < (uint64_t)GPR_U64(ctx, 5)) ? 1 : 0);
    // 0x10003c: 0x3e00008  jr          $ra
    ctx->pc = 0x10003Cu;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x10003Cu, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100044u;
}


