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

// Function: a17_loop
// Address: 0x100000 - 0x10001c
void a17_loop_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a17_loop_0x100000");
#endif

    switch (ctx->pc) {
        case 0x100004u: goto label_100004;
        default: break;
    }

    ctx->pc = 0x100000u;

    // 0x100000: 0x24080005  addiu       $t0, $zero, 0x5
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 8, (int32_t)ADD32(GPR_U32(ctx, 0), 5));
label_100004:
    // 0x100004: 0x2508ffff  addiu       $t0, $t0, -0x1
    ctx->pc = 0x100004u;
    SET_GPR_S32(ctx, 8, (int32_t)ADD32(GPR_U32(ctx, 8), 4294967295));
    // 0x100008: 0x25290001  addiu       $t1, $t1, 0x1
    ctx->pc = 0x100008u;
    SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 9), 1));
    // 0x10000c: 0x1500fffd  bnez        $t0, . + 4 + (-0x3 << 2)
    ctx->pc = 0x10000Cu;
    {
        const bool branch_taken_0x10000c = (GPR_U64(ctx, 8) != GPR_U64(ctx, 0));
        if (branch_taken_0x10000c) {
            ctx->pc = 0x100004u;
            if (runtime->eeCheckpointDue()) {
                return;
            }
            goto label_100004;
        }
    }
    ctx->pc = 0x100014u;
    // 0x100014: 0x3e00008  jr          $ra
    ctx->pc = 0x100014u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
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


// Function: a17_skip_slot
// Address: 0x10001c - 0x100034
void a17_skip_slot_0x10001c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a17_skip_slot_0x10001c");
#endif

    ctx->pc = 0x10001cu;

    // 0x10001c: 0x10850002  beq         $a0, $a1, . + 4 + (0x2 << 2)
    ctx->pc = 0x10001Cu;
    {
        const bool branch_taken_0x10001c = (GPR_U64(ctx, 4) == GPR_U64(ctx, 5));
        ctx->pc = 0x100020u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x10001Cu;
        // 0x100020: 0x24420001  addiu       $v0, $v0, 0x1 (Delay Slot)
        SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 1));
        ctx->in_delay_slot = false;
        if (branch_taken_0x10001c) {
            ctx->pc = 0x100028u;
            goto label_100028;
        }
    }
    ctx->pc = 0x100024u;
    // 0x100024: 0x24630007  addiu       $v1, $v1, 0x7
    ctx->pc = 0x100024u;
    SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 3), 7));
label_100028:
    // 0x100028: 0x24630009  addiu       $v1, $v1, 0x9
    ctx->pc = 0x100028u;
    SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 3), 9));
    // 0x10002c: 0x3e00008  jr          $ra
    ctx->pc = 0x10002Cu;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x10002Cu, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100034u;
}


// Function: a17_blez_bgtz
// Address: 0x100034 - 0x100054
void a17_blez_bgtz_0x100034(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a17_blez_bgtz_0x100034");
#endif

    ctx->pc = 0x100034u;

    // 0x100034: 0x18800002  blez        $a0, . + 4 + (0x2 << 2)
    ctx->pc = 0x100034u;
    {
        const bool branch_taken_0x100034 = (GPR_S32(ctx, 4) <= 0);
        if (branch_taken_0x100034) {
            ctx->pc = 0x100040u;
            goto label_100040;
        }
    }
    ctx->pc = 0x10003Cu;
    // 0x10003c: 0x24420064  addiu       $v0, $v0, 0x64
    ctx->pc = 0x10003cu;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 100));
label_100040:
    // 0x100040: 0x1ca00002  bgtz        $a1, . + 4 + (0x2 << 2)
    ctx->pc = 0x100040u;
    {
        const bool branch_taken_0x100040 = (GPR_S32(ctx, 5) > 0);
        ctx->pc = 0x100044u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100040u;
        // 0x100044: 0x24630001  addiu       $v1, $v1, 0x1 (Delay Slot)
        SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 3), 1));
        ctx->in_delay_slot = false;
        if (branch_taken_0x100040) {
            ctx->pc = 0x10004Cu;
            goto label_10004c;
        }
    }
    ctx->pc = 0x100048u;
    // 0x100048: 0x24630032  addiu       $v1, $v1, 0x32
    ctx->pc = 0x100048u;
    SET_GPR_S32(ctx, 3, (int32_t)ADD32(GPR_U32(ctx, 3), 50));
label_10004c:
    // 0x10004c: 0x3e00008  jr          $ra
    ctx->pc = 0x10004Cu;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x10004Cu, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100054u;
}


// Function: a17_back_slot
// Address: 0x100054 - 0x100068
void a17_back_slot_0x100054(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a17_back_slot_0x100054");
#endif

    ctx->pc = 0x100054u;

label_100054:
    // 0x100054: 0x2508ffff  addiu       $t0, $t0, -0x1
    ctx->pc = 0x100054u;
    SET_GPR_S32(ctx, 8, (int32_t)ADD32(GPR_U32(ctx, 8), 4294967295));
    // 0x100058: 0x1500fffe  bnez        $t0, . + 4 + (-0x2 << 2)
    ctx->pc = 0x100058u;
    {
        const bool branch_taken_0x100058 = (GPR_U64(ctx, 8) != GPR_U64(ctx, 0));
        ctx->pc = 0x10005Cu;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100058u;
        // 0x10005c: 0x25290001  addiu       $t1, $t1, 0x1 (Delay Slot)
        SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 9), 1));
        ctx->in_delay_slot = false;
        if (branch_taken_0x100058) {
            ctx->pc = 0x100054u;
            if (runtime->eeCheckpointDue()) {
                return;
            }
            goto label_100054;
        }
    }
    ctx->pc = 0x100060u;
    // 0x100060: 0x3e00008  jr          $ra
    ctx->pc = 0x100060u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100060u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100068u;
}


// Function: a17_uncond
// Address: 0x100068 - 0x10007c
void a17_uncond_0x100068(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a17_uncond_0x100068");
#endif

    ctx->pc = 0x100068u;

    // 0x100068: 0x10000002  b           . + 4 + (0x2 << 2)
    ctx->pc = 0x100068u;
    {
        const bool branch_taken_0x100068 = (GPR_U64(ctx, 0) == GPR_U64(ctx, 0));
        if (branch_taken_0x100068) {
            ctx->pc = 0x100074u;
            goto label_100074;
        }
    }
    ctx->pc = 0x100070u;
    // 0x100070: 0x24420001  addiu       $v0, $v0, 0x1
    ctx->pc = 0x100070u;
    SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 2), 1));
label_100074:
    // 0x100074: 0x3e00008  jr          $ra
    ctx->pc = 0x100074u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100074u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10007Cu;
}


// Function: a17_external
// Address: 0x10007c - 0x10008c
void a17_external_0x10007c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a17_external_0x10007c");
#endif

    ctx->pc = 0x10007cu;

    // 0x10007c: 0x14800030  bnez        $a0, . + 4 + (0x30 << 2)
    ctx->pc = 0x10007Cu;
    {
        const bool branch_taken_0x10007c = (GPR_U64(ctx, 4) != GPR_U64(ctx, 0));
        if (branch_taken_0x10007c) {
            ctx->pc = 0x100140u;
            return;
        }
    }
    ctx->pc = 0x100084u;
    // 0x100084: 0x3e00008  jr          $ra
    ctx->pc = 0x100084u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100084u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10008Cu;
}


