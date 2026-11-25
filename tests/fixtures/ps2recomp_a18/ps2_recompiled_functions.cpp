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

// Function: a18_callee
// Address: 0x100000 - 0x10000c
void a18_callee_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_callee_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x24820001  addiu       $v0, $a0, 0x1
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 4), 1));
    // 0x100004: 0x3e00008  jr          $ra
    ctx->pc = 0x100004u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100004u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10000Cu;
}


// Function: a18_caller
// Address: 0x10000c - 0x100028
void a18_caller_0x10000c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_caller_0x10000c");
#endif

    switch (ctx->pc) {
        case 0x10001cu: goto label_10001c;
        default: break;
    }

    ctx->pc = 0x10000cu;

    // 0x10000c: 0x27bdfff0  addiu       $sp, $sp, -0x10
    ctx->pc = 0x10000cu;
    SET_GPR_S32(ctx, 29, (int32_t)ADD32(GPR_U32(ctx, 29), 4294967280));
    // 0x100010: 0xafbf0000  sw          $ra, 0x0($sp)
    ctx->pc = 0x100010u;
    WRITE32(ADD32(GPR_U32(ctx, 29), 0), GPR_U32(ctx, 31));
    // 0x100014: 0xc040000  jal         func_100000
    ctx->pc = 0x100014u;
    SET_GPR_U32(ctx, 31, 0x10001Cu);
    ctx->pc = 0x100018u;
    ctx->in_delay_slot = true;
    ctx->branch_pc = 0x100014u;
    // 0x100018: 0x24040007  addiu       $a0, $zero, 0x7 (Delay Slot)
    SET_GPR_S32(ctx, 4, (int32_t)ADD32(GPR_U32(ctx, 0), 7));
    ctx->in_delay_slot = false;
    ctx->pc = 0x100000u;
    if (!runtime->dispatchGuestBranch(rdram, ctx, 0x100000u, 0x100014u, 0x10001Cu, PS2Runtime::GuestBranchKind::DirectCall, "JAL")) {
        return;
    }
    ctx->pc = 0x10001Cu;
label_10001c:
    // 0x10001c: 0x8fbf0000  lw          $ra, 0x0($sp)
    ctx->pc = 0x10001cu;
    SET_GPR_S32(ctx, 31, (int32_t)READ32(ADD32(GPR_U32(ctx, 29), 0)));
    // 0x100020: 0x3e00008  jr          $ra
    ctx->pc = 0x100020u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100024u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100020u;
        // 0x100024: 0x27bd0010  addiu       $sp, $sp, 0x10 (Delay Slot)
        SET_GPR_S32(ctx, 29, (int32_t)ADD32(GPR_U32(ctx, 29), 16));
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100020u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100028u;
}


// Function: a18_caller2
// Address: 0x100028 - 0x100040
void a18_caller2_0x100028(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_caller2_0x100028");
#endif

    switch (ctx->pc) {
        case 0x100034u: goto label_100034;
        default: break;
    }

    ctx->pc = 0x100028u;

    // 0x100028: 0xafbffffc  sw          $ra, -0x4($sp)
    ctx->pc = 0x100028u;
    WRITE32(ADD32(GPR_U32(ctx, 29), 4294967292), GPR_U32(ctx, 31));
    // 0x10002c: 0xc040000  jal         func_100000
    ctx->pc = 0x10002Cu;
    SET_GPR_U32(ctx, 31, 0x100034u);
    ctx->pc = 0x100000u;
    if (!runtime->dispatchGuestBranch(rdram, ctx, 0x100000u, 0x10002Cu, 0x100034u, PS2Runtime::GuestBranchKind::DirectCall, "JAL")) {
        return;
    }
    ctx->pc = 0x100034u;
label_100034:
    // 0x100034: 0x8fbffffc  lw          $ra, -0x4($sp)
    ctx->pc = 0x100034u;
    SET_GPR_S32(ctx, 31, (int32_t)READ32(ADD32(GPR_U32(ctx, 29), 4294967292)));
    // 0x100038: 0x3e00008  jr          $ra
    ctx->pc = 0x100038u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
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


// Function: a18_callee2
// Address: 0x100040 - 0x10004c
void a18_callee2_0x100040(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_callee2_0x100040");
#endif

    ctx->pc = 0x100040u;

    // 0x100040: 0x24830002  addiu       $v1, $a0, 0x2
    ctx->pc = 0x100040u;
    SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 4), 2));
    // 0x100044: 0x3e00008  jr          $ra
    ctx->pc = 0x100044u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100044u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10004Cu;
}


// Function: a18_callee3
// Address: 0x10004c - 0x100058
void a18_callee3_0x10004c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_callee3_0x10004c");
#endif

    ctx->pc = 0x10004cu;

    // 0x10004c: 0x24830003  addiu       $v1, $a0, 0x3
    ctx->pc = 0x10004cu;
    SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 4), 3));
    // 0x100050: 0x3e00008  jr          $ra
    ctx->pc = 0x100050u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100050u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100058u;
}


// Function: a18_caller3
// Address: 0x100058 - 0x100070
void a18_caller3_0x100058(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_caller3_0x100058");
#endif

    switch (ctx->pc) {
        case 0x100064u: goto label_100064;
        default: break;
    }

    ctx->pc = 0x100058u;

    // 0x100058: 0xafbffffc  sw          $ra, -0x4($sp)
    ctx->pc = 0x100058u;
    WRITE32(ADD32(GPR_U32(ctx, 29), 4294967292), GPR_U32(ctx, 31));
    // 0x10005c: 0xc040013  jal         func_10004C
    ctx->pc = 0x10005Cu;
    SET_GPR_U32(ctx, 31, 0x100064u);
    ctx->pc = 0x100060u;
    ctx->in_delay_slot = true;
    ctx->branch_pc = 0x10005Cu;
    // 0x100060: 0x24840005  addiu       $a0, $a0, 0x5 (Delay Slot)
    SET_GPR_S32(ctx, 4, (int32_t)ADD32(GPR_U32(ctx, 4), 5));
    ctx->in_delay_slot = false;
    ctx->pc = 0x10004Cu;
    if (!runtime->dispatchGuestBranch(rdram, ctx, 0x10004Cu, 0x10005Cu, 0x100064u, PS2Runtime::GuestBranchKind::DirectCall, "JAL")) {
        return;
    }
    ctx->pc = 0x100064u;
label_100064:
    // 0x100064: 0x8fbffffc  lw          $ra, -0x4($sp)
    ctx->pc = 0x100064u;
    SET_GPR_S32(ctx, 31, (int32_t)READ32(ADD32(GPR_U32(ctx, 29), 4294967292)));
    // 0x100068: 0x3e00008  jr          $ra
    ctx->pc = 0x100068u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100068u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100070u;
}


// Function: a18_tail
// Address: 0x100070 - 0x100078
void a18_tail_0x100070(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_tail_0x100070");
#endif

    ctx->pc = 0x100070u;

    // 0x100070: 0x8040000  j           func_100000
    ctx->pc = 0x100070u;
    ctx->pc = 0x100000u;
    a18_callee_0x100000(rdram, ctx, runtime); return;
    ctx->pc = 0x100078u;
}


// Function: a18_jalr
// Address: 0x100078 - 0x100088
void a18_jalr_0x100078(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a18_jalr_0x100078");
#endif

    switch (ctx->pc) {
        case 0x100078u: goto label_100078;
        case 0x10007cu: goto label_10007c;
        case 0x100080u: goto label_100080;
        case 0x100084u: goto label_100084;
        default: break;
    }

    ctx->pc = 0x100078u;

label_100078:
    // 0x100078: 0x320f809  jalr        $t9
label_10007c:
    if (ctx->pc == 0x10007Cu) {
        ctx->pc = 0x100080u;
        goto label_100080;
    }
    ctx->pc = 0x100078u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 25);
        SET_GPR_U32(ctx, 31, 0x100080u);
        ctx->pc = jumpTarget;
        if (!runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100078u, 0x100080u, PS2Runtime::GuestBranchKind::IndirectCall, "JALR")) {
            return;
        }
    }
    ctx->pc = 0x100080u;
label_100080:
    // 0x100080: 0x3e00008  jr          $ra
label_100084:
    if (ctx->pc == 0x100084u) {
        ctx->pc = 0x100088u;
        goto label_fallthrough_0x100080;
    }
    ctx->pc = 0x100080u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100080u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
label_fallthrough_0x100080:
    ctx->pc = 0x100088u;
}


