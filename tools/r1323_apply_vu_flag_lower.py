#!/usr/bin/env python3
from pathlib import Path

p=Path('source/core/recompiler/ppc_dynarec.c')
s=p.read_text()
marker='''    } else if(op==0x40u && fn>=0x30u && fn<=0x34u) {\n'''
if s.count(marker)!=1:
    raise SystemExit(f'lower SPECIAL ALU guard mismatch: {s.count(marker)}')
block=r'''    } else if ((op>=0x10u && op<=0x18u) || (op>=0x1au && op<=0x1cu)) {
        /* R1323: native VU micro flag/control family already implemented by
         * vu_exec_lower().  The lower-word ABI supplies VI/control state in
         * r4; VI16 writes still alias indices to 0..15 and discard VI0.
         * CLIP/MAC/STATUS are the raw special slots vi[18]/vi[17]/vi[16]. */
        if (op == 0x11u) { /* FCSET: CLIP = imm24 */
            emit_load_const32(ctx, 6, w & 0x00ffffffu);
            emit(ctx, enc_stw(6, 4, 18 * 4));
            return 0;
        }
        if (op == 0x15u) { /* FSSET: preserve low status flags, replace sticky bits. */
            uint32_t imm=((w>>10)&0x800u)|(w&0x7ffu);
            emit(ctx, enc_lwz(6, 4, 16 * 4));
            emit(ctx, enc_andi_dot(6, 6, 0x003fu));
            emit(ctx, enc_ori(6, 6, (uint16_t)(imm & 0x0fc0u)));
            emit(ctx, enc_stw(6, 4, 16 * 4));
            return 0;
        }
        if (op == 0x10u || op == 0x12u || op == 0x13u) {
            /* FCEQ/FCAND/FCOR operate on the low 24 CLIP bits and always
             * write their boolean result to VI1. */
            emit(ctx, enc_lwz(6, 4, 18 * 4));
            emit(ctx, enc_rlwinm(6, 6, 0, 8, 31));
            emit_load_const32(ctx, 7, w & 0x00ffffffu);
            if (op == 0x10u) {
                emit(ctx, enc_cmplw(6, 7));
                emit(ctx, enc_mfcr(6));
                emit(ctx, enc_rlwinm(6, 6, 3, 31, 31)); /* CR0.EQ -> bit0 */
            } else if (op == 0x12u) {
                emit(ctx, enc_and(6, 6, 7));
                emit(ctx, (11u<<26)|(6u<<16)); /* cmpwi cr0,r6,0 */
                emit(ctx, enc_mfcr(6));
                emit(ctx, enc_rlwinm(6, 6, 3, 31, 31));
                emit(ctx, enc_xori(6, 6, 1)); /* nonzero */
            } else {
                emit(ctx, enc_or(6, 6, 7));
                emit_load_const32(ctx, 7, 0x00ffffffu);
                emit(ctx, enc_cmplw(6, 7));
                emit(ctx, enc_mfcr(6));
                emit(ctx, enc_rlwinm(6, 6, 3, 31, 31));
            }
            emit(ctx, enc_stw(6, 4, 1 * 4));
            return 0;
        }
        if (op >= 0x14u && op <= 0x17u) {
            uint32_t imm=((w>>10)&0x800u)|(w&0x7ffu);
            dst=rt&15u;
            if(!dst)return 0;
            emit(ctx, enc_lwz(6, 4, 16 * 4));
            if (op == 0x14u) {
                emit(ctx, enc_andi_dot(6, 6, 0x0fffu));
                emit(ctx, enc_addi(7, 0, (int16_t)imm));
                emit(ctx, enc_cmplw(6, 7));
                emit(ctx, enc_mfcr(6));
                emit(ctx, enc_rlwinm(6, 6, 3, 31, 31));
            } else if (op == 0x16u) {
                emit(ctx, enc_andi_dot(6, 6, (uint16_t)imm));
            } else { /* FSOR */
                emit(ctx, enc_andi_dot(6, 6, 0x0fffu));
                emit(ctx, enc_ori(6, 6, (uint16_t)imm));
            }
        } else if (op == 0x18u || op == 0x1au || op == 0x1bu) {
            dst=rt&15u;if(!dst)return 0;
            emit(ctx, enc_lwz(6, 4, 17 * 4));
            emit(ctx, enc_andi_dot(6, 6, 0xffffu));
            emit_micro_vi_read(ctx, 7, rs);
            if (op == 0x18u) {
                emit(ctx, enc_cmplw(6, 7));
                emit(ctx, enc_mfcr(6));
                emit(ctx, enc_rlwinm(6, 6, 3, 31, 31));
            } else if (op == 0x1au) emit(ctx, enc_and(6, 6, 7));
            else emit(ctx, enc_or(6, 6, 7));
        } else { /* FCGET: It = CLIP low 12 bits. */
            dst=rt&15u;if(!dst)return 0;
            emit(ctx, enc_lwz(6, 4, 18 * 4));
            emit(ctx, enc_andi_dot(6, 6, 0x0fffu));
        }
'''
s=s.replace(marker, block+marker, 1)
p.write_text(s)
print('R1323 VU lower flag-family patch staged successfully')
