/* GPL-3.0-or-later. Shared scalar/AOT EE memory semantics. */
    case 0x22: /* LWL */ {
        static const uint32_t LWL_MASK[4]  = { 0xffffffu, 0x0000ffffu, 0x000000ffu, 0x00000000u };
        static const uint8_t  LWL_SHIFT[4] = { 24, 16, 8, 0 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 3;
        uint32_t mem = ee_mem_read32(st, addr & ~3u);
        if (rt) GPR(rt) = sext32((rt32 & LWL_MASK[shift]) | (mem << LWL_SHIFT[shift]));
    } break;
    case 0x26: /* LWR */ {
        static const uint32_t LWR_MASK[4]  = { 0x00000000u, 0xff000000u, 0xffff0000u, 0xffffff00u };
        static const uint8_t  LWR_SHIFT[4] = { 0, 8, 16, 24 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 3;
        uint32_t mem = ee_mem_read32(st, addr & ~3u);
        uint32_t result = (rt32 & LWR_MASK[shift]) | (mem >> LWR_SHIFT[shift]);
        if (rt) {
            if (shift == 0)
                GPR(rt) = sext32(result); /* full 64-bit sign extend, matches PCSX2 */
            else
                GPR(rt) = (GPR(rt) & 0xFFFFFFFF00000000ULL) | result; /* upper 32 bits preserved */
        }
    } break;

    case 0x20: /* LB */  if (rt) GPR(rt) = (uint64_t)(int64_t)(int8_t)ee_mem_read8(st, rs32 + imm); else ee_mem_read8(st, rs32 + imm); break;
    case 0x21: /* LH */  if (rt) GPR(rt) = (uint64_t)(int64_t)(int16_t)ee_mem_read16(st, rs32 + imm); else ee_mem_read16(st, rs32 + imm); break;
    case 0x23: /* LW */  if (rt) GPR(rt) = sext32(ee_mem_read32(st, rs32 + imm)); else ee_mem_read32(st, rs32 + imm); break;
    case 0x24: /* LBU */ if (rt) GPR(rt) = ee_mem_read8(st, rs32 + imm); else ee_mem_read8(st, rs32 + imm); break;
    case 0x25: /* LHU */ if (rt) GPR(rt) = ee_mem_read16(st, rs32 + imm); else ee_mem_read16(st, rs32 + imm); break;
    case 0x27: /* LWU */ if (rt) GPR(rt) = ee_mem_read32(st, rs32 + imm); else ee_mem_read32(st, rs32 + imm); break;
    case 0x37: /* LD */ {
        uint32_t ld_addr = rs32 + imm;
        uint64_t ld_val = ee_mem_read64(st, ld_addr);
        if (rt) GPR(rt) = ld_val;
    } break;

    case 0x2C: /* SDL - Store Doubleword Left. Standard MIPS III
                * counterpart to SWL, scaled from 4 bytes to 8 (3
                * shift bits instead of 2) - the store-side mirror of
                * round 13's LDL. Not PS2-specific, implemented
                * directly from the well-documented MIPS III ISA. */
    {
        static const uint64_t SDL_MASK[8] = {
            0xffffffffffffff00ULL, 0xffffffffffff0000ULL, 0xffffffffff000000ULL, 0xffffffff00000000ULL,
            0xffffff0000000000ULL, 0xffff000000000000ULL, 0xff00000000000000ULL, 0x0000000000000000ULL
        };
        static const uint8_t SDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 7u;
        uint64_t mem = ee_mem_read64(st, addr & ~7u);
        ee_mem_write64(st, addr & ~7u, (GPR(rt) >> SDL_SHIFT[shift]) | (mem & SDL_MASK[shift]));
    } break;
    case 0x2D: /* SDR - Store Doubleword Right (mirror of SDL, same
                * relationship SWR has to SWL but for doublewords). */
    {
        static const uint64_t SDR_MASK[8] = {
            0x0000000000000000ULL, 0x00000000000000ffULL, 0x000000000000ffffULL, 0x0000000000ffffffULL,
            0x00000000ffffffffULL, 0x000000ffffffffffULL, 0x0000ffffffffffffULL, 0x00ffffffffffffffULL
        };
        static const uint8_t SDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 7u;
        uint64_t mem = ee_mem_read64(st, addr & ~7u);
        ee_mem_write64(st, addr & ~7u, (GPR(rt) << SDR_SHIFT[shift]) | (mem & SDR_MASK[shift]));
    } break;

    case 0x2A: /* SWL */ {
        static const uint32_t SWL_MASK[4]  = { 0xffffff00u, 0xffff0000u, 0xff000000u, 0x00000000u };
        static const uint8_t  SWL_SHIFT[4] = { 24, 16, 8, 0 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 3;
        uint32_t mem = ee_mem_read32(st, addr & ~3u);
        ee_mem_write32(st, addr & ~3u, (rt32 >> SWL_SHIFT[shift]) | (mem & SWL_MASK[shift]));
    } break;
    case 0x2E: /* SWR */ {
        static const uint32_t SWR_MASK[4]  = { 0x00000000u, 0x000000ffu, 0x0000ffffu, 0x00ffffffu };
        static const uint8_t  SWR_SHIFT[4] = { 0, 8, 16, 24 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 3;
        uint32_t mem = ee_mem_read32(st, addr & ~3u);
        ee_mem_write32(st, addr & ~3u, (rt32 << SWR_SHIFT[shift]) | (mem & SWR_MASK[shift]));
    } break;

    case 0x28: /* SB */ ee_mem_write8(st, rs32 + imm, (uint8_t)GPR(rt)); break;
    case 0x29: /* SH */ ee_mem_write16(st, rs32 + imm, (uint16_t)GPR(rt)); break;
    case 0x2B: /* SW */ ee_mem_write32(st, rs32 + imm, (uint32_t)GPR(rt)); break;
    case 0x2F: /* CACHE */ break; /* no-op: no cache model */
    case 0x33: /* PREF */  break; /* no-op: prefetch hint */
    case 0x3F: /* SD */ ee_mem_write64(st, rs32 + imm, GPR(rt)); break;

    /* LWC1/SWC1 - direct FPR<->memory word transfer (as opposed to
     * MFC1/MTC1, which move a raw 32-bit value between an FPR and a
     * GPR). Found missing once real BIOS boot got far enough to need
     * it (see docs/STATUS.md's "round 5"/COP0 PRId fix) - halted
     * cleanly on "unimplemented primary opcode 0x39" (SWC1). Standard
     * MIPS I FPU load/store, `rt` selects the FPR (not a GPR) here. */
    case 0x31: /* LWC1 */ st->fpr[rt] = ee_mem_read32(st, rs32 + imm); break;
    case 0x39: /* SWC1 */ ee_mem_write32(st, rs32 + imm, st->fpr[rt]); break;

    case 0x1A: /* LDL - Load Doubleword Left. Standard MIPS III 64-bit
                * unaligned-load counterpart to LWL (already
                * implemented) - same byte-merge idea scaled from a
                * 4-byte word to an 8-byte doubleword (3 shift bits
                * instead of 2). Found missing once round 13's VU0
                * fixes let real BIOS boot advance past the VU0 init/
                * self-test sequence into new code. Not PS2-specific,
                * so implemented directly from the well-documented
                * MIPS III ISA rather than needing live verification. */
    {
        static const uint64_t LDL_MASK[8] = {
            0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
            0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
        };
        static const uint8_t LDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 7u;
        uint64_t mem = ee_mem_read64(st, addr & ~7u);
        if (rt) GPR(rt) = (GPR(rt) & LDL_MASK[shift]) | (mem << LDL_SHIFT[shift]);
    } break;
    case 0x1B: /* LDR - Load Doubleword Right (mirror of LDL, same
                * relationship LWR has to LWL but for doublewords). */
    {
        static const uint64_t LDR_MASK[8] = {
            0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL,
            0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL
        };
        static const uint8_t LDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
        uint32_t addr = rs32 + imm;
        uint32_t shift = addr & 7u;
        uint64_t mem = ee_mem_read64(st, addr & ~7u);
        if (rt) GPR(rt) = (GPR(rt) & LDR_MASK[shift]) | (mem >> LDR_SHIFT[shift]);
    } break;

    case 0x36: /* LQC2 - Load Quadword Coprocessor 2 (round 444, task
                * #228). This is the memory-transfer half of the VU0
                * macro-mode datapath: the CO-format arithmetic ops
                * (VADD/VSUB/VMUL/VMADD/VMADDA-broadcast-row/VOPMSUB/
                * etc, all already implemented above under case 0x12)
                * operate purely on vu0_vf[]/vu0_acc[] register state
                * and never touch EE main memory themselves - LQC2/
                * SQC2 are what actually move VU0 vector registers to
                * and from RAM, exactly like LWC1/SWC1 do for the FPU.
                * Real encoding is the same I-type layout as LWC1/LQ:
                * opcode(6) base=rs(5) ft=rt(5) offset=16 - so this
                * decoder's existing rs32/rt/imm extraction (shared
                * with every other I-type case in this switch) applies
                * unchanged; "rt" here names the VU0 vf register index,
                * not a GPR. Ported from PCSX2's R5900OpcodeImpl.cpp
                * LQC2(): reads 4 consecutive 32-bit words starting at
                * GPR[rs]+imm (UNALIGNED - unlike LQ/SQ, real hardware
                * does NOT mask the low 4 bits for LQC2/SQC2, confirmed
                * against PCSX2's own implementation which calls plain
                * memRead32 at +0/+4/+8/+12 with no address masking),
                * into VF[rt].xyzw in order. VF0 discards writes but
                * still performs all memory reads (PCSX2 VU0.cpp LQC2
                * explicitly reads a temporary when _Ft_==0). */
        {
            uint32_t addr = rs32 + imm;
            vu0_vf_write_lane(st, rt, 0, ee_mem_read32(st, addr));
            vu0_vf_write_lane(st, rt, 1, ee_mem_read32(st, addr + 4));
            vu0_vf_write_lane(st, rt, 2, ee_mem_read32(st, addr + 8));
            vu0_vf_write_lane(st, rt, 3, ee_mem_read32(st, addr + 12));
        }
        break;
    case 0x1E: /* LQ - 128-bit load, ported from PCSX2's R5900OpcodeImpl.cpp.
                * Address is masked to 16-byte alignment (real hardware
                * ignores the low 4 bits rather than faulting on
                * unaligned access, unlike LW/LD). Matches PCSX2's own
                * interpreter exactly in skipping the read entirely
                * when rt==$0 (unlike LW/LH/etc elsewhere in this
                * file, which still perform the read for its memory
                * side effects even when the destination is
                * discarded) - LQ has no such side-effect-only path in
                * real PCSX2, so neither does this. */
        if (rt) {
            uint32_t addr = (rs32 + imm) & ~0xFu;
            ee_mem_read128(st,addr,&st->gpr[rt]);
        }
        break;
    case 0x1F: /* SQ - 128-bit store, ported from PCSX2's R5900OpcodeImpl.cpp.
                * Same 16-byte alignment masking as LQ. Always writes
                * both halves, including when rt==$0 (whose value is
                * always zero) - matches real hardware/PCSX2, no
                * special-case needed. */
    {
        uint32_t addr = (rs32 + imm) & ~0xFu;
        ee_mem_write128(st,addr,&st->gpr[rt]);
    } break;

    case 0x3E: /* SQC2 - Store Quadword Coprocessor 2 (round 444, task
                * #228). The store-side mirror of LQC2 (case 0x36
                * above) - writes VF[rt].xyzw (4 consecutive 32-bit
                * words) to EE memory at GPR[rs]+imm, UNALIGNED (no
                * 16-byte masking, same real-hardware distinction from
                * LQ/SQ as LQC2 has). Ported from PCSX2's
                * R5900OpcodeImpl.cpp SQC2(): unlike LQC2, there is no
                * rt==0 guard - VF00 always reads back its hardwired
                * (0,0,0,1.0) constant (vu0_vf_read_lane already
                * special-cases reg==0 for exactly this), so "sqc2
                * $vf0,addr" is a well-defined real op that stores
                * (0,0,0,1.0), not a no-op. This is the exact opcode
                * this project's Round-443 host-native boot trace
                * first reached and halted on
                * (pc=0x0050DD8C/0x0050DD90, inside a real VU0 4x4
                * matrix_multiply()-shaped routine confirmed against
                * ps2sdk's ee/math3d/src/math3d.c in Round 444) -
                * closing this gap, together with LQC2 above, is the
                * complete fix: every arithmetic op that routine uses
                * (vmulax/vmadday/vmaddaz - the ACC-writing broadcast
                * row - and the final vmaddw reading ACC - VMADD funct
                * 0x29) was already implemented in the COP2 CO-format
                * dispatch; only the memory-transfer opcodes themselves
                * were missing. */
    {
        uint32_t addr = rs32 + imm;
        ee_mem_write32(st, addr,      vu0_vf_read_lane(st, rt, 0));
        ee_mem_write32(st, addr + 4,  vu0_vf_read_lane(st, rt, 1));
        ee_mem_write32(st, addr + 8,  vu0_vf_read_lane(st, rt, 2));
        ee_mem_write32(st, addr + 12, vu0_vf_read_lane(st, rt, 3));
    } break;
