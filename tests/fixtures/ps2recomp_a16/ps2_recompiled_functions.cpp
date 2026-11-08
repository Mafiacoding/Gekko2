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

// Function: a16_byte
// Address: 0x100000 - 0x100020
void a16_byte_0x100000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a16_byte_0x100000");
#endif

    ctx->pc = 0x100000u;

    // 0x100000: 0x80880000  lb          $t0, 0x0($a0)
    ctx->pc = 0x100000u;
    SET_GPR_S32(ctx, 8, (int8_t)READ8(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100004: 0x90890001  lbu         $t1, 0x1($a0)
    ctx->pc = 0x100004u;
    SET_GPR_ZE32(ctx, 9, (uint8_t)READ8(ADD32(GPR_U32(ctx, 4), 1)));
    // 0x100008: 0xa0880002  sb          $t0, 0x2($a0)
    ctx->pc = 0x100008u;
    WRITE8(ADD32(GPR_U32(ctx, 4), 2), (uint8_t)GPR_U32(ctx, 8));
    // 0x10000c: 0x80800003  lb          $zero, 0x3($a0)
    ctx->pc = 0x10000cu;
    SET_GPR_S32(ctx, 0, (int8_t)READ8(ADD32(GPR_U32(ctx, 4), 3)));
    // 0x100010: 0x908affff  lbu         $t2, -0x1($a0)
    ctx->pc = 0x100010u;
    SET_GPR_ZE32(ctx, 10, (uint8_t)READ8(ADD32(GPR_U32(ctx, 4), 4294967295)));
    // 0x100014: 0xa0800005  sb          $zero, 0x5($a0)
    ctx->pc = 0x100014u;
    WRITE8(ADD32(GPR_U32(ctx, 4), 5), (uint8_t)GPR_U32(ctx, 0));
    // 0x100018: 0x808b7fff  lb          $t3, 0x7FFF($a0)
    ctx->pc = 0x100018u;
    SET_GPR_S32(ctx, 11, (int8_t)READ8(ADD32(GPR_U32(ctx, 4), 32767)));
    // 0x10001c: 0xa089fffe  sb          $t1, -0x2($a0)
    ctx->pc = 0x10001cu;
    WRITE8(ADD32(GPR_U32(ctx, 4), 4294967294), (uint8_t)GPR_U32(ctx, 9));
    ctx->pc = 0x100020u;
}


// Function: a16_half
// Address: 0x100020 - 0x100040
void a16_half_0x100020(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a16_half_0x100020");
#endif

    ctx->pc = 0x100020u;

    // 0x100020: 0x84880000  lh          $t0, 0x0($a0)
    ctx->pc = 0x100020u;
    SET_GPR_S32(ctx, 8, (int16_t)READ16(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100024: 0x94890002  lhu         $t1, 0x2($a0)
    ctx->pc = 0x100024u;
    SET_GPR_ZE32(ctx, 9, (uint16_t)READ16(ADD32(GPR_U32(ctx, 4), 2)));
    // 0x100028: 0xa4880004  sh          $t0, 0x4($a0)
    ctx->pc = 0x100028u;
    WRITE16(ADD32(GPR_U32(ctx, 4), 4), (uint16_t)GPR_U32(ctx, 8));
    // 0x10002c: 0x84800006  lh          $zero, 0x6($a0)
    ctx->pc = 0x10002cu;
    SET_GPR_S32(ctx, 0, (int16_t)READ16(ADD32(GPR_U32(ctx, 4), 6)));
    // 0x100030: 0x948afffe  lhu         $t2, -0x2($a0)
    ctx->pc = 0x100030u;
    SET_GPR_ZE32(ctx, 10, (uint16_t)READ16(ADD32(GPR_U32(ctx, 4), 4294967294)));
    // 0x100034: 0xa4800008  sh          $zero, 0x8($a0)
    ctx->pc = 0x100034u;
    WRITE16(ADD32(GPR_U32(ctx, 4), 8), (uint16_t)GPR_U32(ctx, 0));
    // 0x100038: 0x848b7ffe  lh          $t3, 0x7FFE($a0)
    ctx->pc = 0x100038u;
    SET_GPR_S32(ctx, 11, (int16_t)READ16(ADD32(GPR_U32(ctx, 4), 32766)));
    // 0x10003c: 0xa489fffc  sh          $t1, -0x4($a0)
    ctx->pc = 0x10003cu;
    WRITE16(ADD32(GPR_U32(ctx, 4), 4294967292), (uint16_t)GPR_U32(ctx, 9));
    ctx->pc = 0x100040u;
}


// Function: a16_alias
// Address: 0x100040 - 0x100050
void a16_alias_0x100040(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a16_alias_0x100040");
#endif

    ctx->pc = 0x100040u;

    // 0x100040: 0x90840000  lbu         $a0, 0x0($a0)
    ctx->pc = 0x100040u;
    SET_GPR_ZE32(ctx, 4, (uint8_t)READ8(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100044: 0x94a50000  lhu         $a1, 0x0($a1)
    ctx->pc = 0x100044u;
    SET_GPR_ZE32(ctx, 5, (uint16_t)READ16(ADD32(GPR_U32(ctx, 5), 0)));
    // 0x100048: 0xa0c60000  sb          $a2, 0x0($a2)
    ctx->pc = 0x100048u;
    WRITE8(ADD32(GPR_U32(ctx, 6), 0), (uint8_t)GPR_U32(ctx, 6));
    // 0x10004c: 0xa4c60002  sh          $a2, 0x2($a2)
    ctx->pc = 0x10004cu;
    WRITE16(ADD32(GPR_U32(ctx, 6), 2), (uint16_t)GPR_U32(ctx, 6));
    ctx->pc = 0x100050u;
}


// Function: a16_ret_store_slot
// Address: 0x100050 - 0x10005c
void a16_ret_store_slot_0x100050(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a16_ret_store_slot_0x100050");
#endif

    ctx->pc = 0x100050u;

    // 0x100050: 0x84820000  lh          $v0, 0x0($a0)
    ctx->pc = 0x100050u;
    SET_GPR_S32(ctx, 2, (int16_t)READ16(ADD32(GPR_U32(ctx, 4), 0)));
    // 0x100054: 0x3e00008  jr          $ra
    ctx->pc = 0x100054u;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100058u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x100054u;
        // 0x100058: 0xa0a20000  sb          $v0, 0x0($a1) (Delay Slot)
        WRITE8(ADD32(GPR_U32(ctx, 5), 0), (uint8_t)GPR_U32(ctx, 2));
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x100054u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x10005Cu;
}


// Function: a16_ret_load_slot
// Address: 0x10005c - 0x100064
void a16_ret_load_slot_0x10005c(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a16_ret_load_slot_0x10005c");
#endif

    ctx->pc = 0x10005cu;

    // 0x10005c: 0x3e00008  jr          $ra
    ctx->pc = 0x10005Cu;
    {
        const uint32_t jumpTarget = GPR_U32(ctx, 31);
        ctx->pc = 0x100060u;
        ctx->in_delay_slot = true;
        ctx->branch_pc = 0x10005Cu;
        // 0x100060: 0x94830002  lhu         $v1, 0x2($a0) (Delay Slot)
        SET_GPR_ZE32(ctx, 3, (uint16_t)READ16(ADD32(GPR_U32(ctx, 4), 2)));
        ctx->in_delay_slot = false;
        ctx->pc = jumpTarget;
        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x10005Cu, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");
        return;
        #else
        ctx->pc = jumpTarget;
        return;
        #endif
    }
    ctx->pc = 0x100064u;
}


// Function: a16_hinted
// Address: 0x100064 - 0x100074
void a16_hinted_0x100064(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("a16_hinted_0x100064");
#endif

    ctx->pc = 0x100064u;

    // 0x100064: 0x3c088030  lui         $t0, 0x8030
    ctx->pc = 0x100064u;
    SET_GPR_S32(ctx, 8, (int32_t)((uint32_t)32816 << 16));
    // 0x100068: 0x81090000  lb          $t1, 0x0($t0)
    ctx->pc = 0x100068u;
    SET_GPR_S32(ctx, 9, (int8_t)FAST_READ8(0x80300000u));
    // 0x10006c: 0xa5090002  sh          $t1, 0x2($t0)
    ctx->pc = 0x10006cu;
    do { uint16_t _value = static_cast<uint16_t>((uint16_t)GPR_U32(ctx, 9)); ps2TraceGuestWrite(rdram, 0x80300002u, 2u, _value, 0u, "WRITE16", ctx); FAST_WRITE16(0x80300002u, _value); } while (0);
    // 0x100070: 0x948a0000  lhu         $t2, 0x0($a0)
    ctx->pc = 0x100070u;
    SET_GPR_ZE32(ctx, 10, (uint16_t)READ16(ADD32(GPR_U32(ctx, 4), 0)));
    ctx->pc = 0x100074u;
}


