#!/usr/bin/env python3
from pathlib import Path

p=Path('source/core/recompiler/ppc_dynarec.c')
s=p.read_text()
macro='#define VU0_VF_OFF(reg, lane)  ((int16_t)(1728 + (reg) * 16 + (lane) * 4))\n'
if s.count(macro)!=1:
    raise SystemExit('VU0_VF_OFF guard mismatch')
s=s.replace(macro, macro + '#define VU0_MEM_OFFSET         ((int16_t)2240) /* ee_state_t::vu0_mem[0] */\n', 1)
marker='''                return -1; /* every other SPECIAL2 sub-opcode: not yet
                             * JIT-compiled, fall back to the interpreter. */'''
if s.count(marker)!=1:
    raise SystemExit('SPECIAL2 fallback guard mismatch')
block=r'''                /* R1322: close the remaining macro-mode SPECIAL2 forms already
                 * implemented by ee_core.c. VU0 local memory is a little-endian
                 * byte array inside ee_state_t, so dynamic 16/32-bit accesses use
                 * lhbrx/lwbrx/stwbrx on the big-endian Gekko. VI0/VF00 are
                 * canonicalized exactly like the scalar interpreter helpers. */
                if (idx == 52u || idx == 53u || idx == 54u || idx == 55u) {
                    int is_load = (idx == 52u || idx == 54u);
                    int predec = (idx == 54u || idx == 55u);
                    uint32_t vi = is_load ? fs : ft;
                    if (vi) emit(ctx, enc_lwz(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(vi)));
                    else emit(ctx, enc_addi(SCRATCH_A, 0, 0));
                    if (predec) {
                        emit(ctx, enc_addi(SCRATCH_A, SCRATCH_A, -1));
                        emit(ctx, enc_andi_dot(SCRATCH_A, SCRATCH_A, 0xFFFFu));
                        if (vi) emit(ctx, enc_stw(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(vi)));
                    }
                    emit(ctx, enc_andi_dot(SCRATCH_C, SCRATCH_A, 0x00FFu));
                    emit(ctx, enc_rlwinm(SCRATCH_C, SCRATCH_C, 4, 0, 27));
                    for (unsigned lane = 0; lane < 4; ++lane) {
                        if (!(destmask & (8u >> lane))) continue;
                        emit(ctx, enc_addi(SCRATCH_D, SCRATCH_C,
                                           (int16_t)(VU0_MEM_OFFSET + lane * 4u)));
                        if (is_load) {
                            if (ft) {
                                emit(ctx, enc_lwbrx(SCRATCH_E, CTX_REG, SCRATCH_D));
                                emit(ctx, enc_stw(SCRATCH_E, CTX_REG, VU0_VF_OFF(ft, lane)));
                            }
                        } else {
                            emit_vu0_raw_operand(ctx, SCRATCH_E, fs, lane);
                            emit(ctx, enc_stwbrx(SCRATCH_E, CTX_REG, SCRATCH_D));
                        }
                    }
                    if (!predec) {
                        emit(ctx, enc_addi(SCRATCH_A, SCRATCH_A, 1));
                        emit(ctx, enc_andi_dot(SCRATCH_A, SCRATCH_A, 0xFFFFu));
                        if (vi) emit(ctx, enc_stw(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(vi)));
                    }
                    return 0;
                }
                if (idx == 60u) {
                    if (ft) {
                        emit_vu0_raw_operand(ctx, SCRATCH_A, fs, destmask & 3u);
                        emit(ctx, enc_andi_dot(SCRATCH_A, SCRATCH_A, 0xFFFFu));
                        emit(ctx, enc_stw(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(ft)));
                    }
                    return 0;
                }
                if (idx == 61u) {
                    if (ft) {
                        if (fs) emit(ctx, enc_lwz(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(fs)));
                        else emit(ctx, enc_addi(SCRATCH_A, 0, 0));
                        emit(ctx, enc_extsh(SCRATCH_A, SCRATCH_A));
                        for (unsigned lane = 0; lane < 4; ++lane)
                            if (destmask & (8u >> lane))
                                emit(ctx, enc_stw(SCRATCH_A, CTX_REG, VU0_VF_OFF(ft, lane)));
                    }
                    return 0;
                }
                if (idx == 62u) {
                    if (ft) {
                        unsigned lane = destmask == 0x8u ? 0u : destmask == 0x4u ? 1u : destmask == 0x2u ? 2u : 3u;
                        if (fs) emit(ctx, enc_lwz(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(fs)));
                        else emit(ctx, enc_addi(SCRATCH_A, 0, 0));
                        emit(ctx, enc_andi_dot(SCRATCH_A, SCRATCH_A, 0x00FFu));
                        emit(ctx, enc_rlwinm(SCRATCH_A, SCRATCH_A, 4, 0, 27));
                        emit(ctx, enc_addi(SCRATCH_A, SCRATCH_A,
                                           (int16_t)(VU0_MEM_OFFSET + lane * 4u)));
                        emit(ctx, enc_lhbrx(SCRATCH_B, CTX_REG, SCRATCH_A));
                        emit(ctx, enc_stw(SCRATCH_B, CTX_REG, COP2_CTRL_OFF(ft)));
                    }
                    return 0;
                }
                if (idx == 63u) {
                    unsigned lane = destmask == 0x8u ? 0u : destmask == 0x4u ? 1u : destmask == 0x2u ? 2u : 3u;
                    if (fs) emit(ctx, enc_lwz(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(fs)));
                    else emit(ctx, enc_addi(SCRATCH_A, 0, 0));
                    emit(ctx, enc_andi_dot(SCRATCH_A, SCRATCH_A, 0x00FFu));
                    emit(ctx, enc_rlwinm(SCRATCH_A, SCRATCH_A, 4, 0, 27));
                    emit(ctx, enc_addi(SCRATCH_A, SCRATCH_A,
                                       (int16_t)(VU0_MEM_OFFSET + lane * 4u)));
                    if (ft) emit(ctx, enc_lwz(SCRATCH_B, CTX_REG, COP2_CTRL_OFF(ft)));
                    else emit(ctx, enc_addi(SCRATCH_B, 0, 0));
                    emit(ctx, enc_stwbrx(SCRATCH_B, CTX_REG, SCRATCH_A));
                    return 0;
                }
                if (idx == 64u || idx == 65u) {
                    emit(ctx, enc_lwz(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(20)));
                    if (idx == 64u) {
                        emit(ctx, enc_addi(SCRATCH_C, 0, 4));
                        emit(ctx, enc_srw(SCRATCH_B, SCRATCH_A, SCRATCH_C));
                        emit(ctx, enc_andi_dot(SCRATCH_B, SCRATCH_B, 1));
                        emit(ctx, enc_addi(SCRATCH_C, 0, 22));
                        emit(ctx, enc_srw(SCRATCH_D, SCRATCH_A, SCRATCH_C));
                        emit(ctx, enc_andi_dot(SCRATCH_D, SCRATCH_D, 1));
                        emit(ctx, enc_xor(SCRATCH_B, SCRATCH_B, SCRATCH_D));
                        emit(ctx, enc_addi(SCRATCH_C, 0, 1));
                        emit(ctx, enc_slw(SCRATCH_A, SCRATCH_A, SCRATCH_C));
                        emit(ctx, enc_xor(SCRATCH_A, SCRATCH_A, SCRATCH_B));
                        emit(ctx, enc_rlwinm(SCRATCH_A, SCRATCH_A, 0, 9, 31));
                        emit_load_const32(ctx, SCRATCH_D, 0x3F800000u);
                        emit(ctx, enc_or(SCRATCH_A, SCRATCH_A, SCRATCH_D));
                        emit(ctx, enc_stw(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(20)));
                    }
                    if (ft) for (unsigned lane = 0; lane < 4; ++lane)
                        if (destmask & (8u >> lane))
                            emit(ctx, enc_stw(SCRATCH_A, CTX_REG, VU0_VF_OFF(ft, lane)));
                    return 0;
                }
                if (idx == 66u || idx == 67u) {
                    emit_vu0_raw_operand(ctx, SCRATCH_A, fs, destmask & 3u);
                    if (idx == 67u) {
                        emit(ctx, enc_lwz(SCRATCH_B, CTX_REG, COP2_CTRL_OFF(20)));
                        emit(ctx, enc_xor(SCRATCH_A, SCRATCH_A, SCRATCH_B));
                    }
                    emit(ctx, enc_rlwinm(SCRATCH_A, SCRATCH_A, 0, 9, 31));
                    emit_load_const32(ctx, SCRATCH_B, 0x3F800000u);
                    emit(ctx, enc_or(SCRATCH_A, SCRATCH_A, SCRATCH_B));
                    emit(ctx, enc_stw(SCRATCH_A, CTX_REG, COP2_CTRL_OFF(20)));
                    return 0;
                }
'''
s=s.replace(marker, block+marker, 1)
p.write_text(s)

p=Path('source/core/recompiler/ee_jit.c')
s=p.read_text()
guard='''_Static_assert(offsetof(ee_state_t, vu0_vf) == 1728,
               "ppc_dynarec.c's VU0_VF_OFF() assumes vu0_vf[0][0] sits at this exact byte offset");'''
if s.count(guard)!=1:
    raise SystemExit('VU0 layout assert guard mismatch')
s=s.replace(guard, guard + '''\n_Static_assert(offsetof(ee_state_t, vu0_mem) == 2240,
               "ppc_dynarec.c's VU0_MEM_OFFSET assumes vu0_mem[0] sits at this exact byte offset");''', 1)
p.write_text(s)
