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

// Function: a19_jfwd
// Address: 0x100000 - 0x100018
void a19_jfwd_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a19_jfwd_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x24420001  addiu       $v0, $v0, 0x1
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 1));
    // 0x100004: 0x8040004  j           func_100010
    ctx->pc = 0x100004u;
    ctx->pc = 0x100008u;
    ctx->in_delay_slot = true;
    ctx->branch_pc = 0x100004u;
    // 0x100008: 0x24630001  addiu       $v1, $v1, 0x1 (Delay Slot)
    SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 3), 1));
    ctx->in_delay_slot = false;
    ctx->pc = 0x100010u;
    goto label_100010;
    ctx->pc = 0x10000Cu;
    // 0x10000c: 0x24420064  addiu       $v0, $v0, 0x64
    ctx->pc = 0x10000cu;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 100));
label_100010:
    // 0x100010: 0x3e00008  jr          $ra
    ctx->pc = 0x100010u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100010u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100018u;
}


// Function: a19_jnop
// Address: 0x100018 - 0x100030
void a19_jnop_0x100018(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a19_jnop_0x100018");
#endif

    ctx->pc = 0x100018u;

    // 0x100018: 0x24420001  addiu       $v0, $v0, 0x1
    ctx->pc = 0x100018u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 1));
    // 0x10001c: 0x804000a  j           func_100028
    ctx->pc = 0x10001Cu;
    ctx->pc = 0x100028u;
    goto label_100028;
    ctx->pc = 0x100024u;
    // 0x100024: 0x24420064  addiu       $v0, $v0, 0x64
    ctx->pc = 0x100024u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 100));
label_100028:
    // 0x100028: 0x3e00008  jr          $ra
    ctx->pc = 0x100028u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100028u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100030u;
}


// Function: a19_jback
// Address: 0x100030 - 0x10004c
void a19_jback_0x100030(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a19_jback_0x100030");
#endif

    ctx->pc = 0x100030u;

label_100030:
    // 0x100030: 0x25290001  addiu       $t1, $t1, 0x1
    ctx->pc = 0x100030u;
    SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 9), 1));
    // 0x100034: 0x152a0003  bne         $t1, $t2, . + 4 + (0x3 << 2)
    ctx->pc = 0x100034u;
    {
        const bool branch_taken_0x100034 = (GPR_U64(ctx, 9) != GPR_U64(ctx, 10));
        if (branch_taken_0x100034) {
            ctx->pc = 0x100044u;
            goto label_100044;
        }
    }
    ctx->pc = 0x10003Cu;
    // 0x10003c: 0x804000c  j           func_100030
    ctx->pc = 0x10003Cu;
    ctx->pc = 0x100030u;
    if (runtime->eeCheckpointDue()) {
        return;
    }
    goto label_100030;
    ctx->pc = 0x100044u;
label_100044:
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


// Function: a19_jback2
// Address: 0x10004c - 0x10006c
void a19_jback2_0x10004c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a19_jback2_0x10004c");
#endif

    switch (ctx->pc) {
        case 0x100050u: goto label_100050;
        default: break;
    }

    ctx->pc = 0x10004cu;

    // 0x10004c: 0x25290000  addiu       $t1, $t1, 0x0
    ctx->pc = 0x10004cu;
    SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 9), 0));
label_100050:
    // 0x100050: 0x25290001  addiu       $t1, $t1, 0x1
    ctx->pc = 0x100050u;
    SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 9), 1));
    // 0x100054: 0x152a0003  bne         $t1, $t2, . + 4 + (0x3 << 2)
    ctx->pc = 0x100054u;
    {
        const bool branch_taken_0x100054 = (GPR_U64(ctx, 9) != GPR_U64(ctx, 10));
        ctx->pc = 0x100058u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100054u;
        // 0x100058: 0x256b0001  addiu       $t3, $t3, 0x1 (Delay Slot)
        SET_GPR_S32(ctx, 11, (int32_t)ADD32(GPR_U32(ctx, 11), 1));
        ctx->in_delay_slot = false;
        if (branch_taken_0x100054) {
            ctx->pc = 0x100064u;
            goto label_100064;
        }
    }
    ctx->pc = 0x10005Cu;
    // 0x10005c: 0x8040014  j           func_100050
    ctx->pc = 0x10005Cu;
    ctx->pc = 0x100060u;
    ctx->in_delay_slot = true;
    ctx->branch_pc = 0x10005Cu;
    // 0x100060: 0x258c0001  addiu       $t4, $t4, 0x1 (Delay Slot)
    SET_GPR_S32(ctx, 12, (int32_t)ADD32(GPR_U32(ctx, 12), 1));
    ctx->in_delay_slot = false;
    ctx->pc = 0x100050u;
    if (runtime->eeCheckpointDue()) {
        return;
    }
    goto label_100050;
    ctx->pc = 0x100064u;
label_100064:
    // 0x100064: 0x3e00008  jr          $ra
    ctx->pc = 0x100064u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100064u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10006Cu;
}


