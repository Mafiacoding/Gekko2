/* GPL-3.0-or-later. Shared scalar/AOT semantics from ee_core.c. */
    case 0x12: /* COP2 (VU0 macro mode). Round 12 added the 32-bit
                * control-register transfers (MFC2/CFC2/MTC2/CTC2).
                * Round 13 adds the actual VU0 vector datapath: 128-bit
                * QMFC2/QMTC2 transfers, plus the specific "CO"-format
                * (rs bit 0x10 set) vector ops a real BIOS VU0 init/
                * self-test sequence needs (VSUB, VISWR, VSQI). Round
                * 29 continued (10th change) adds VADD/VMUL (same
                * 3-operand full-vector shape as VSUB) and VIADDI
                * (closing a previously-flagged gap next to VIADD/
                * VISUB/VIAND/VIOR) - not yet exercised by the current
                * boot trace (the EE is steady-state polling SIF, not
                * halted on a missing COP2 op - see docs/STATUS.md's
                * "Round 29 continued (10th change)" section), but
                * real, tested, roadmap-directed forward progress on
                * docs/ROADMAP.md section 5 item 3. Field
                * encodings (rs=destmask|0x10, FT/FS/FD positions, the
                * SPECIAL2 sub-index formula) were derived by decoding
                * the exact raw instruction words from a live PCSX2
                * disassembly and cross-checked against PCSX2's own
                * R5900OpcodeTables.cpp Int_COP2PrintTable/
                * Int_COP2SPECIAL1PrintTable/Int_COP2SPECIAL2PrintTable
                * decode tables - see docs/STATUS.md's "round 13"
                * section for the full derivation. Any other CO-format
                * op (the rest of the VU macro arithmetic family, and
                * the rest of the memory-access family - VILWR/VLQI/
                * VLQD/VSQD/VDIV/etc) halts honestly rather than
                * silently doing nothing. */
        vu0_sync_macro(st);
        if (rs < 0x10) {
            switch (rs) {
            case 0x00: /* MFC2 */ if (rt) GPR(rt) = sext32(vu0_vi_read(st, rd)); break;
            case 0x01: /* QMFC2 - 128-bit GPR <- VF, raw bit copy (no
                        * float conversion): lanes x,y pack into the
                        * low 64 bits (ud0), z,w into the high 64 bits
                        * (ud1) - matches PCSX2's VECTOR union layout
                        * (pcsx2/VU.h). */
                if (rt) {
                    uint64_t x = vu0_vf_read_lane(st, rd, 0);
                    uint64_t y = vu0_vf_read_lane(st, rd, 1);
                    uint64_t z = vu0_vf_read_lane(st, rd, 2);
                    uint64_t w = vu0_vf_read_lane(st, rd, 3);
                    st->gpr[rt].ud0 = x | (y << 32);
                    st->gpr[rt].ud1 = z | (w << 32);
                }
                break;
            case 0x02: /* CFC2 */ if (rt) GPR(rt) = sext32(vu0_vi_read(st, rd)); break;
            case 0x04: /* MTC2 */ vu0_vi_write(st, rd, rt32); break;
            case 0x05: /* QMTC2 - 128-bit VF <- GPR, raw bit copy. */
                vu0_vf_write_lane(st, rd, 0, (uint32_t)(st->gpr[rt].ud0 & 0xFFFFFFFFu));
                vu0_vf_write_lane(st, rd, 1, (uint32_t)(st->gpr[rt].ud0 >> 32));
                vu0_vf_write_lane(st, rd, 2, (uint32_t)(st->gpr[rt].ud1 & 0xFFFFFFFFu));
                vu0_vf_write_lane(st, rd, 3, (uint32_t)(st->gpr[rt].ud1 >> 32));
                break;
            case 0x06: /* CTC2 */
                /* Real FBRST (control reg 28) semantics - ported from
                 * PCSX2's own VU0.cpp CTC2(): bit 0x1 = VU0 force-break,
                 * bit 0x2 = VU0 reset, bit 0x100 = VU1 force-break,
                 * bit 0x200 = VU1 reset. Not modeled beyond plain
                 * storage - no VU0/VU1 execution state exists yet for
                 * these bits to actually act on. */
                vu0_vi_write(st, rd, rt32);
                break;
            case 0x08: { /* BC2F/T/FL/TL: condition is VPU_STAT bit 8. */
                if(rt>3u) {
                    halt("reserved BC2 selector");
                    return 1;
                }
                unsigned condition=(st->cop2_ctrl[29]>>8)&1u;
                if(condition==(rt&1u))
                    BRANCH_TO(this_pc+4u+(uint32_t)(imm*4));
                else if(rt&2u) {
                    st->pc=fallthrough_pc+4u;
                    st->next_pc=st->pc+4u;
                } else st->branch_pending=1u;
                break;
            }
            default:
                halt("unimplemented COP2 sub-opcode (VU0 vector datapath not implemented)");
                return 1;
            }
        } else {
            /* CO-format: rs = 0x10 | destmask (destmask bit3=X,
             * bit2=Y, bit1=Z, bit0=W - confirmed via viswr's rs=0x18,
             * mask 0x8 = X only, matching its ".x" mnemonic suffix;
             * and vsub.xyzw/vsqi's rs=0x1F, mask 0xF = all lanes).
             * 3-operand arithmetic field layout: bits 20-16=FT (2nd
             * source), bits 15-11=FS (1st source), bits 10-6=FD
             * (dest) - confirmed against vsub.xyzw vf01,vf00,vf00's
             * raw fields (FT=FS=0=vf00, FD=1=vf01). */
            uint32_t destmask = rs & 0xFu;
            uint32_t ft = (instr >> 16) & 0x1Fu;
            uint32_t fs = (instr >> 11) & 0x1Fu;
            uint32_t fd = (instr >> 6) & 0x1Fu;

            /* R1356: real VCALLMS/VCALLMSR entry into the existing VU0
             * micro executor. PCSX2 COP2.cpp; VU.h CMSAR0=27.
             * micro entry is in 8-byte instruction units, not bytes. */
            if (funct == 0x38 || funct == 0x39) {
                if (st->vu0_running) vu0_exec_micro_continue(st);
                vu0_exec_micro(st, funct == 0x38 ? ((instr >> 6) & 0x7fffu)
                                                : (st->cop2_ctrl[27] & 0xffffu));
            } else if (funct == 0x28 || funct == 0x2A || funct == 0x2B || funct == 0x2C || funct == 0x2F) {
                /* VADD(0x28)/VMUL(0x2A)/VMAX(0x2B)/VSUB(0x2C)/
                 * VMINI(0x2F): FD[lane] = FS[lane] OP FT[lane], real
                 * float arithmetic/comparison on the reinterpreted
                 * bit patterns, for each lane selected by destmask.
                 * Round 13 implemented VSUB only; VADD/VMUL (Round 29
                 * continued, 10th change) and VMAX/VMINI (Round 29
                 * continued, 16th change) are the same 3-operand
                 * full-vector shape (confirmed against PCSX2's own
                 * R5900OpcodeTables.cpp SPECIAL1 table row: funct
                 * 0x28=VADD, 0x2A=VMUL, 0x2B=VMAX, 0x2C=VSUB,
                 * 0x2F=VMINI - VMAX/VMINI ported from PCSX2's own
                 * VUops.cpp _vuMAX/_vuMINI, which use a plain fp_max/
                 * fp_min comparison - a straightforward C ternary
                 * comparison here, consistent with this project not
                 * modeling any NaN/signed-zero edge cases anywhere
                 * else in its float datapath either). VMADD(0x29)/
                 * VMSUB(0x2D)/VOPMSUB(0x2E), this same row's three
                 * accumulator-based siblings, are handled separately
                 * below (Round 29 continued, 17th change). */
                for (int lane = 0; lane < 4; lane++) {
                    if (!(destmask & (0x8u >> lane))) continue;
                    uint32_t ua = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                    uint32_t ub = vu0_vf_read_lane(st, ft, (uint32_t)lane);
                    if(funct==0x2bu||funct==0x2fu) {
                        vu0_vf_write_lane(st,fd,(uint32_t)lane,vu_minmax_bits(ua,ub,funct==0x2fu));
                        continue;
                    }
                    float a, b, r; uint32_t ur;
                    memcpy(&a, &ua, 4);
                    memcpy(&b, &ub, 4);
                    if (funct == 0x28) r = a + b;
                    else if (funct == 0x2A) r = a * b;
                    else if (funct == 0x2B) r = (a > b) ? a : b;
                    else if (funct == 0x2F) r = (a < b) ? a : b;
                    else r = a - b;
                    memcpy(&ur, &r, 4);
                    vu0_vf_write_lane(st, fd, (uint32_t)lane, ur);
                }
            } else if (funct <= 0x1F) {
                /* Full broadcast row (funct 0x00-0x1F), completed
                 * across two rounds (Round 29 continued's 18th
                 * change added 0x00-0x07/0x10-0x1B; this 19th change
                 * adds the remaining 0x08-0x0F and 0x1C-0x1F).
                 * Confirmed against PCSX2's R5900OpcodeTables.cpp
                 * SPECIAL1 table's first 4 rows (funct 0-31 laid out
                 * as 8 columns x 4 rows):
                 *   0x00-0x03 VADDx/y/z/w    0x04-0x07 VSUBx/y/z/w
                 *   0x08-0x0B VMADDx/y/z/w   0x0C-0x0F VMSUBx/y/z/w
                 *   0x10-0x13 VMAXx/y/z/w    0x14-0x17 VMINIx/y/z/w
                 *   0x18-0x1B VMULx/y/z/w    0x1C VMULq, 0x1D VMAXi,
                 *   0x1E VMULi, 0x1F VMINIi
                 * VADDx/y/z/w/VSUBx/y/z/w/VMAXx/y/z/w/VMINIx/y/z/w/
                 * VMULx/y/z/w: FD[lane] = FS[lane] OP FT.<bc-lane>,
                 * where <bc-lane> is fixed by the opcode (funct&0x3),
                 * per PCSX2's VUops.cpp applyBinaryMACOpBroadcast.
                 * VMADDx/y/z/w/VMSUBx/y/z/w: same shape but reads a
                 * third operand from the VU0 macro-mode accumulator
                 * (vu0_acc[4]), per applyTernaryMACOpBroadcast:
                 * FD[lane] = ACC[lane] +- FS[lane]*FT.<bc-lane>.
                 * VMULq/VMAXi/VMULi/VMINIi (0x1C-0x1F): no FT operand
                 * at all (confirmed against DisR5900asm.cpp's
                 * P_VMULq/P_VMAXi/P_VMULi/P_VMINIi, which only print
                 * FD/FS - "vmulq.xyzw vf1,vf2,Q" etc) - instead
                 * broadcast the scalar Q or I control register
                 * (cop2_ctrl[22]/cop2_ctrl[21] per PCSX2's VU.h
                 * REG_Q=22/REG_I=21) to every destmask lane:
                 * FD[lane] = FS[lane] OP scalar. */
                uint32_t bc_lane = funct & 0x3u;
                uint32_t base_op = (funct >> 2) & 0x7u; /* 0=VADD,1=VSUB,2=VMADD,3=VMSUB,4=VMAX,5=VMINI,6=VMUL,7=Q/I-row */
                /* op_kind: 0=ADD,1=SUB,2=MADD(acc),3=MSUB(acc),
                 * 4=MAX,5=MINI,6=MUL. For base_op 0-6 this is just
                 * base_op itself; for base_op==7 (the Q/I row) the
                 * real operator is selected by bc_lane instead:
                 * bc_lane 0=VMULq(MUL), 1=VMAXi(MAX), 2=VMULi(MUL),
                 * 3=VMINIi(MINI). */
                uint32_t op_kind;
                float b; uint32_t ub;
                if (base_op == 7) {
                    if (bc_lane == 0) { op_kind = 6; ub = vu0_vi_read(st, 22); }      /* VMULq: Q = cop2_ctrl[22] */
                    else if (bc_lane == 1) { op_kind = 4; ub = vu0_vi_read(st, 21); } /* VMAXi: I = cop2_ctrl[21] */
                    else if (bc_lane == 2) { op_kind = 6; ub = vu0_vi_read(st, 21); } /* VMULi */
                    else { op_kind = 5; ub = vu0_vi_read(st, 21); }                    /* VMINIi */
                } else {
                    op_kind = base_op;
                    ub = vu0_vf_read_lane(st, ft, bc_lane);
                }
                memcpy(&b, &ub, 4);
                for (int lane = 0; lane < 4; lane++) {
                    if (!(destmask & (0x8u >> lane))) continue;
                    uint32_t ua = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                    if(op_kind==4u||op_kind==5u) {
                        vu0_vf_write_lane(st,fd,(uint32_t)lane,vu_minmax_bits(ua,ub,op_kind==5u));
                        continue;
                    }
                    float a, r; uint32_t ur;
                    memcpy(&a, &ua, 4);
                    if (op_kind == 0) r = a + b;                     /* VADDx/y/z/w */
                    else if (op_kind == 1) r = a - b;                 /* VSUBx/y/z/w */
                    else if (op_kind == 2) {                          /* VMADDx/y/z/w */
                        uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4);
                        r = acc + a * b;
                    } else if (op_kind == 3) {                        /* VMSUBx/y/z/w */
                        uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4);
                        r = acc - a * b;
                    } else if (op_kind == 4) r = (a > b) ? a : b; /* raw MIN/MAX handled before FP below */
                    else if (op_kind == 5) r = (a < b) ? a : b;      /* VMINIx/y/z/w or VMINIi */
                    else r = a * b;                                  /* VMULx/y/z/w or VMULq/VMULi */
                    memcpy(&ur, &r, 4);
                    vu0_vf_write_lane(st, fd, (uint32_t)lane, ur);
                }
            } else if (funct == 0x29 || funct == 0x2D) {
                /* VMADD(0x29)/VMSUB(0x2D): FD[lane] = ACC[lane] +-
                 * FS[lane]*FT[lane], per destmask lane - the same
                 * SPECIAL1 row as VADD/VMUL/VMAX/VSUB/VMINI above
                 * (confirmed against PCSX2's R5900OpcodeTables.cpp:
                 * ..., VADD, VMADD, VMUL, VMAX, VSUB, VMSUB, VOPMSUB,
                 * VMINI, ... = funct 0x28..0x2F sequential), but
                 * reading a third operand from the VU0 macro-mode
                 * accumulator (st->vu0_acc[4], lane order x=0/y=1/
                 * z=2/w=3 - the same convention source/hw/vu.c's own
                 * ACC handling already uses for VU microcode).
                 * Ported from PCSX2's own VUops.cpp _vuOpMADD/
                 * _vuOpMSUB (applyTernaryMACOp<..., MACOpDst::Fd>):
                 * writes FD, does NOT write back into ACC (that's the
                 * separate VMADDA/VMSUBA accumulator-dest opcodes,
                 * not modeled here - not seen in the traced boot path,
                 * a scoped future gap like the broadcast/VMADDbc
                 * forms). No MAC-flag modeling, consistent with this
                 * project's existing float datapath elsewhere. */
                for (int lane = 0; lane < 4; lane++) {
                    if (!(destmask & (0x8u >> lane))) continue;
                    uint32_t uacc = st->vu0_acc[lane];
                    uint32_t ua = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                    uint32_t ub = vu0_vf_read_lane(st, ft, (uint32_t)lane);
                    float acc, a, b, r; uint32_t ur;
                    memcpy(&acc, &uacc, 4);
                    memcpy(&a, &ua, 4);
                    memcpy(&b, &ub, 4);
                    if (funct == 0x29) r = acc + a * b;
                    else r = acc - a * b;
                    memcpy(&ur, &r, 4);
                    vu0_vf_write_lane(st, fd, (uint32_t)lane, ur);
                }
            } else if (funct == 0x2E) {
                /* VOPMSUB: outer-product multiply-subtract, the
                 * cross-product-shaped sibling in the same row -
                 * ALWAYS writes exactly xyz (never w - there is no
                 * destmask field for this op; confirmed against
                 * PCSX2's own VUops.cpp _vuOPMSUB, which has no _W
                 * branch at all): FD.x=ACC.x-FS.y*FT.z,
                 * FD.y=ACC.y-FS.z*FT.x, FD.z=ACC.z-FS.x*FT.y. */
                uint32_t uaccx = st->vu0_acc[0], uaccy = st->vu0_acc[1], uaccz = st->vu0_acc[2];
                uint32_t ufsx = vu0_vf_read_lane(st, fs, 0), ufsy = vu0_vf_read_lane(st, fs, 1), ufsz = vu0_vf_read_lane(st, fs, 2);
                uint32_t uftx = vu0_vf_read_lane(st, ft, 0), ufty = vu0_vf_read_lane(st, ft, 1), uftz = vu0_vf_read_lane(st, ft, 2);
                float accx, accy, accz, fsx, fsy, fsz, ftx, fty, ftz;
                memcpy(&accx, &uaccx, 4); memcpy(&accy, &uaccy, 4); memcpy(&accz, &uaccz, 4);
                memcpy(&fsx, &ufsx, 4); memcpy(&fsy, &ufsy, 4); memcpy(&fsz, &ufsz, 4);
                memcpy(&ftx, &uftx, 4); memcpy(&fty, &ufty, 4); memcpy(&ftz, &uftz, 4);
                float rx = accx - fsy * ftz;
                float ry = accy - fsz * ftx;
                float rz = accz - fsx * fty;
                uint32_t urx, ury, urz;
                memcpy(&urx, &rx, 4); memcpy(&ury, &ry, 4); memcpy(&urz, &rz, 4);
                vu0_vf_write_lane(st, fd, 0, urx);
                vu0_vf_write_lane(st, fd, 1, ury);
                vu0_vf_write_lane(st, fd, 2, urz);
            } else if ((funct & 0x3Cu) == 0x3Cu) {
                /* SPECIAL2 sub-dispatch - index formula confirmed
                 * against PCSX2's R5900OpcodeTables.cpp comment:
                 * (code & 0x3) | ((code >> 4) & 0x7c). */
                uint32_t idx = (instr & 0x3u) | ((instr >> 4) & 0x7Cu);
                if (idx == 63) {
                    /* VISWR: store VI[ft] (data source, "is") into VU0
                     * mem at quadword index VI[fs] (address, "it"),
                     * single lane selected by destmask (a single bit
                     * for this op - ".x"/".y"/".z"/".w" only). */
                    int lane = (destmask == 0x8u) ? 0 : (destmask == 0x4u) ? 1 : (destmask == 0x2u) ? 2 : 3;
                    uint32_t addr_vi = vu0_vi_read(st, fs);
                    uint32_t data = vu0_vi_read(st, ft);
                    uint32_t off = vu0_mem_addr(addr_vi, (uint32_t)lane);
                    vu0_mem_write32(st, off, data);
                } else if (idx == 53) {
                    /* VSQI: store VF[fs] (data source) lanes selected
                     * by destmask to VU0 mem at quadword index VI[ft]
                     * (address), then post-increment VI[ft] by 1
                     * quadword. BUGFIX (Round 29 continued, 22nd
                     * change): this previously ignored destmask and
                     * always stored all 4 lanes - confirmed via a real
                     * PCSX2 upstream reference clone's
                     * DisR5900asm.cpp (`P_VSQI` prints `dest_string()`,
                     * proving VSQI genuinely has an xyzw suffix like
                     * every other CO-format op) that this was a real
                     * gap, not a simplification; fixed here alongside
                     * VLQI/VLQD/VSQD, which share the exact same
                     * destmask-respecting shape. */
                    uint32_t addr_vi = vu0_vi_read(st, ft);
                    for (int lane = 0; lane < 4; lane++) {
                        if (!(destmask & (0x8u >> lane))) continue;
                        uint32_t val = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                        uint32_t off = vu0_mem_addr(addr_vi, (uint32_t)lane);
                        vu0_mem_write32(st, off, val);
                    }
                    vu0_vi_write(st, ft, addr_vi + 1);
                } else if (idx == 52) {
                    /* VLQI (Round 29 continued, 22nd change): load all
                     * destmask-selected lanes of VF[ft] (data dest)
                     * from VU0 mem at quadword index VI[fs] (address,
                     * using the CURRENT value), then post-increment
                     * VI[fs] by 1 quadword. Confirmed against PCSX2's
                     * own VUops.cpp _vuLQI - the address register
                     * lives in the same field position ("Is"/"Fs" in
                     * PCSX2's naming) this decoder already calls
                     * `fs`; the dest VF register is the same field
                     * this decoder calls `ft` (matching VLQD/VSQD/
                     * VSQI's shared convention: address reg = fs
                     * field for loads/ft field for stores, dest/data
                     * reg = the other field). */
                    uint32_t addr_vi = vu0_vi_read(st, fs);
                    for (int lane = 0; lane < 4; lane++) {
                        if (!(destmask & (0x8u >> lane))) continue;
                        uint32_t off = vu0_mem_addr(addr_vi, (uint32_t)lane);
                        uint32_t val = vu0_mem_read32(st, off);
                        vu0_vf_write_lane(st, ft, (uint32_t)lane, val);
                    }
                    vu0_vi_write(st, fs, (addr_vi + 1) & 0xFFFFu);
                } else if (idx == 54) {
                    /* VLQD: pre-decrement VI[fs] by 1 quadword FIRST,
                     * then load all destmask-selected lanes of VF[ft]
                     * from VU0 mem at the NEW (decremented) VI[fs]
                     * index - ported from PCSX2's _vuLQD. VI is a
                     * real 16-bit register (matching VIADD/VISUB's
                     * existing 0xFFFF masking), so decrementing from 0
                     * wraps to 0xFFFF rather than underflowing. */
                    uint32_t addr_vi = (vu0_vi_read(st, fs) - 1u) & 0xFFFFu;
                    vu0_vi_write(st, fs, addr_vi);
                    for (int lane = 0; lane < 4; lane++) {
                        if (!(destmask & (0x8u >> lane))) continue;
                        uint32_t off = vu0_mem_addr(addr_vi, (uint32_t)lane);
                        uint32_t val = vu0_mem_read32(st, off);
                        vu0_vf_write_lane(st, ft, (uint32_t)lane, val);
                    }
                } else if (idx == 55) {
                    /* VSQD: pre-decrement VI[ft] (address reg, the
                     * SAME field VSQI/VISWR already use for their own
                     * address) by 1 quadword FIRST, then store all
                     * destmask-selected lanes of VF[fs] (data source)
                     * to VU0 mem at the NEW (decremented) index -
                     * ported from PCSX2's _vuSQD, the pre-decrement
                     * mirror image of VSQI's post-increment. */
                    uint32_t addr_vi = (vu0_vi_read(st, ft) - 1u) & 0xFFFFu;
                    vu0_vi_write(st, ft, addr_vi);
                    for (int lane = 0; lane < 4; lane++) {
                        if (!(destmask & (0x8u >> lane))) continue;
                        uint32_t val = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                        uint32_t off = vu0_mem_addr(addr_vi, (uint32_t)lane);
                        vu0_mem_write32(st, off, val);
                    }
                } else if (idx == 29 || (idx >= 16 && idx <= 23) || idx == 48 || idx == 49) {
                    /* Unary/data-movement cluster (Round 29 continued,
                     * 20th change): VABS(idx=29), VITOF0/4/12/15
                     * (idx=16-19), VFTOI0/4/12/15(idx=20-23),
                     * VMOVE(idx=48), VMR32(idx=49). IMPORTANT: unlike
                     * every arithmetic op above (dest=FD), a real
                     * PCSX2 upstream reference clone's DisR5900asm.cpp
                     * disassembly formatters (P_VABS/P_VITOF0/
                     * P_VFTOI0/etc: "vabs.%s FT, FS") confirm these
                     * ops encode the DESTINATION in the FT field
                     * position and the SOURCE in FS - the opposite of
                     * the arithmetic row's FD/FS/FT roles (VUops.cpp's
                     * _vuABS, _vuITOFn, _vuFTOIn, _vuMOVE, _vuMR32 all
                     * write VU->VF[_Ft_] from VU->VF[_Fs_], guarded by
                     * "if (_Ft_ == 0) return"). This decoder already
                     * extracts ft/fs at the same bit positions for
                     * every CO-format instruction, so no new field
                     * extraction was needed - just using them with
                     * their roles swapped for this cluster. fd (the
                     * bits-6-10 field) is unused/ignored here, per
                     * real hardware. */
                    if (ft == 0) {
                        /* writes to VF00 are discarded, same rule as
                         * every other VF write in this file */
                    } else if (idx == 29) {
                        /* VABS: FT[lane] = |FS[lane]| (bit-level abs -
                         * clear the sign bit - ported from PCSX2's
                         * vuOpABS: "fs & 0x7fffffff"), per destmask
                         * lane. */
                        for (int lane = 0; lane < 4; lane++) {
                            if (!(destmask & (0x8u >> lane))) continue;
                            uint32_t v = vu0_vf_read_lane(st, fs, (uint32_t)lane) & 0x7FFFFFFFu;
                            vu0_vf_write_lane(st, ft, (uint32_t)lane, v);
                        }
                    } else if (idx == 48) {
                        /* VMOVE: FT[lane] = FS[lane], plain per-lane
                         * copy, per destmask lane. */
                        for (int lane = 0; lane < 4; lane++) {
                            if (!(destmask & (0x8u >> lane))) continue;
                            vu0_vf_write_lane(st, ft, (uint32_t)lane, vu0_vf_read_lane(st, fs, (uint32_t)lane));
                        }
                    } else if (idx == 49) {
                        /* VMR32: 32-bit lane rotate - FT.x=FS.y,
                         * FT.y=FS.z, FT.z=FS.w, FT.w=FS.x (ported from
                         * PCSX2's _vuMR32, which reads FS.x into a
                         * temporary FIRST so a VMOVE-to-self (Ft==Fs)
                         * still rotates correctly even though the
                         * lanes are written in x/y/z/w order). Per
                         * destmask lane, same as every other op. */
                        uint32_t tx = vu0_vf_read_lane(st, fs, 0);
                        uint32_t ty = vu0_vf_read_lane(st, fs, 1);
                        uint32_t tz = vu0_vf_read_lane(st, fs, 2);
                        uint32_t tw = vu0_vf_read_lane(st, fs, 3);
                        if (destmask & 0x8u) vu0_vf_write_lane(st, ft, 0, ty);
                        if (destmask & 0x4u) vu0_vf_write_lane(st, ft, 1, tz);
                        if (destmask & 0x2u) vu0_vf_write_lane(st, ft, 2, tw);
                        if (destmask & 0x1u) vu0_vf_write_lane(st, ft, 3, tx);
                    } else {
                        /* VITOF0/4/12/15(idx16-19)/VFTOI0/4/12/15
                         * (idx20-23) - fixed-point <-> float
                         * conversion, ported bit-exact from PCSX2's
                         * VUops.cpp intToFloat<Offset>/
                         * floatToInt<Offset> templates (including the
                         * denormal-range saturation floatToInt
                         * applies) rather than a plain C cast, since
                         * the real hardware quirk (scaling by a
                         * bit-constructed power-of-two float constant
                         * before/after the int<->float conversion,
                         * and saturating to INT32_MIN/MAX above a
                         * fixed exponent threshold) is directly
                         * portable and not worth re-deriving. */
                        int is_ftoi = (idx >= 20);
                        uint32_t offset_n = (idx & 0x3u) == 0 ? 0u : (idx & 0x3u) == 1 ? 4u : (idx & 0x3u) == 2 ? 12u : 15u;
                        for (int lane = 0; lane < 4; lane++) {
                            if (!(destmask & (0x8u >> lane))) continue;
                            uint32_t uval = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                            uint32_t uresult;
                            if (!is_ftoi) {
                                /* intToFloat<Offset> */
                                int32_t ival; memcpy(&ival, &uval, 4);
                                float fval = (float)ival;
                                if (offset_n) {
                                    uint32_t scale_bits = 0x3F800000u - (offset_n << 23);
                                    float scale; memcpy(&scale, &scale_bits, 4);
                                    fval *= scale;
                                }
                                memcpy(&uresult, &fval, 4);
                            } else {
                                /* floatToInt<Offset> */
                                float fval; memcpy(&fval, &uval, 4);
                                if (offset_n) {
                                    uint32_t scale_bits = 0x3F800000u + (offset_n << 23);
                                    float scale; memcpy(&scale, &scale_bits, 4);
                                    fval *= scale;
                                }
                                uint32_t fbits; memcpy(&fbits, &fval, 4);
                                if ((fbits & 0x7F800000u) >= 0x4F000000u) {
                                    uresult = (fbits & 0x80000000u) ? 0x80000000u : 0x7FFFFFFFu;
                                } else {
                                    int32_t iv = (int32_t)fval;
                                    memcpy(&uresult, &iv, 4);
                                }
                            }
                            vu0_vf_write_lane(st, ft, (uint32_t)lane, uresult);
                        }
                    }
                } else if (idx <= 15 || (idx >= 24 && idx <= 28) || idx == 30
                           || (idx >= 32 && idx <= 42) || idx == 44 || idx == 45
                           || idx == 46 || idx == 47) {
                    /* Accumulator-writing family (Round 29 continued,
                     * 21st change) - every one of these ops writes
                     * VU->ACC (this project's vu0_acc[4]) instead of
                     * VF[fd], confirmed against a real PCSX2 upstream
                     * reference clone's VUops.cpp (applyBinaryMACOp/
                     * applyTernaryMACOp/their Broadcast variants, all
                     * templated on MACOpDst::Acc instead of ::Fd - the
                     * exact same underlying arithmetic already
                     * implemented for the FD-writing rows above, just
                     * redirected). Full-vector forms (idx 40=VADDA,
                     * 41=VMADDA, 42=VMULA, 44=VSUBA, 45=VMSUBA) read
                     * both operands from FS/FT directly, same shape
                     * as VADD/VMADD/VMUL/VSUB/VMSUB. Broadcast forms
                     * (idx 0-15, 24-28, 30, 32-39) read the second
                     * operand from a single FT lane, or from Q
                     * (cop2_ctrl[22]) / I (cop2_ctrl[21]), exactly
                     * like the funct<=0x1F broadcast row. idx 46
                     * (VOPMULA) is the outer-product multiply variant
                     * of VOPMSUB - writes ACC directly (no existing-
                     * ACC read, no destmask, xyz only, w untouched),
                     * ported from PCSX2's _vuOPMULA. idx 47 (VNOP) is
                     * a true no-op. idx 43 is COP2_Unknown on real
                     * hardware and correctly falls through to the
                     * halt below. */
                    if (idx == 47) {
                        /* VNOP: no operation. */
                    } else if (idx == 46) {
                        uint32_t ufsx = vu0_vf_read_lane(st, fs, 0), ufsy = vu0_vf_read_lane(st, fs, 1), ufsz = vu0_vf_read_lane(st, fs, 2);
                        uint32_t uftx = vu0_vf_read_lane(st, ft, 0), ufty = vu0_vf_read_lane(st, ft, 1), uftz = vu0_vf_read_lane(st, ft, 2);
                        float fsx, fsy, fsz, ftx, fty, ftz;
                        memcpy(&fsx, &ufsx, 4); memcpy(&fsy, &ufsy, 4); memcpy(&fsz, &ufsz, 4);
                        memcpy(&ftx, &uftx, 4); memcpy(&fty, &ufty, 4); memcpy(&ftz, &uftz, 4);
                        float rx = fsy * ftz, ry = fsz * ftx, rz = fsx * fty;
                        uint32_t urx, ury, urz;
                        memcpy(&urx, &rx, 4); memcpy(&ury, &ry, 4); memcpy(&urz, &rz, 4);
                        st->vu0_acc[0] = urx; st->vu0_acc[1] = ury; st->vu0_acc[2] = urz;
                    } else {
                        int is_broadcast = idx <= 15 || (idx >= 24 && idx <= 28) || idx == 30 || (idx >= 32 && idx <= 39);
                        uint32_t op_kind = 0; /* 0=ADD,1=SUB,2=MADD(acc read+write),3=MSUB,6=MUL */
                        if (is_broadcast) {
                            float b; uint32_t ub;
                            if (idx <= 7) { op_kind = (idx >= 4) ? 1u : 0u; ub = vu0_vf_read_lane(st, ft, idx & 0x3u); }
                            else if (idx <= 15) { op_kind = (idx >= 12) ? 3u : 2u; ub = vu0_vf_read_lane(st, ft, idx & 0x3u); }
                            else if (idx <= 27) { op_kind = 6u; ub = vu0_vf_read_lane(st, ft, idx & 0x3u); } /* VMULAx/y/z/w */
                            else if (idx == 28) { op_kind = 6u; ub = vu0_vi_read(st, 22); } /* VMULAq */
                            else if (idx == 30) { op_kind = 6u; ub = vu0_vi_read(st, 21); } /* VMULAi */
                            else {
                                switch (idx) {
                                case 32: op_kind = 0u; ub = vu0_vi_read(st, 22); break; /* VADDAq */
                                case 33: op_kind = 2u; ub = vu0_vi_read(st, 22); break; /* VMADDAq */
                                case 34: op_kind = 0u; ub = vu0_vi_read(st, 21); break; /* VADDAi */
                                case 35: op_kind = 2u; ub = vu0_vi_read(st, 21); break; /* VMADDAi */
                                case 36: op_kind = 1u; ub = vu0_vi_read(st, 22); break; /* VSUBAq */
                                case 37: op_kind = 3u; ub = vu0_vi_read(st, 22); break; /* VMSUBAq */
                                case 38: op_kind = 1u; ub = vu0_vi_read(st, 21); break; /* VSUBAi */
                                default: op_kind = 3u; ub = vu0_vi_read(st, 21); break; /* VMSUBAi (idx=39) */
                                }
                            }
                            memcpy(&b, &ub, 4);
                            for (int lane = 0; lane < 4; lane++) {
                                if (!(destmask & (0x8u >> lane))) continue;
                                uint32_t ua = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                                float a, r; uint32_t ur;
                                memcpy(&a, &ua, 4);
                                if (op_kind == 0) r = a + b;
                                else if (op_kind == 1) r = a - b;
                                else if (op_kind == 2) { uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4); r = acc + a * b; }
                                else if (op_kind == 3) { uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4); r = acc - a * b; }
                                else r = a * b;
                                memcpy(&ur, &r, 4);
                                st->vu0_acc[lane] = ur;
                            }
                        } else {
                            switch (idx) {
                            case 40: op_kind = 0u; break; /* VADDA */
                            case 41: op_kind = 2u; break; /* VMADDA */
                            case 42: op_kind = 6u; break; /* VMULA */
                            case 44: op_kind = 1u; break; /* VSUBA */
                            default: op_kind = 3u; break; /* VMSUBA (idx=45) */
                            }
                            for (int lane = 0; lane < 4; lane++) {
                                if (!(destmask & (0x8u >> lane))) continue;
                                uint32_t ua = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                                uint32_t ub2 = vu0_vf_read_lane(st, ft, (uint32_t)lane);
                                float a, bb, r; uint32_t ur;
                                memcpy(&a, &ua, 4); memcpy(&bb, &ub2, 4);
                                if (op_kind == 0) r = a + bb;
                                else if (op_kind == 1) r = a - bb;
                                else if (op_kind == 2) { uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4); r = acc + a * bb; }
                                else if (op_kind == 3) { uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4); r = acc - a * bb; }
                                else r = a * bb;
                                memcpy(&ur, &r, 4);
                                st->vu0_acc[lane] = ur;
                            }
                        }
                    }
                } else if (idx == 60) {
                    /* VMTIR (Round 29 continued, 23rd change): VI[ft]
                     * = low 16 bits of the RAW bit pattern of
                     * VF[fs][Fsf] (a plain truncation of the 32-bit
                     * float's bit pattern, NOT a numeric conversion -
                     * ported from PCSX2's own VUops.cpp _vuMTIR:
                     * "VI[_It_].US[0] = *(u16*)&VF[_Fs_].F[_Fsf_]",
                     * which reads the low 16 bits of the selected
                     * lane's 32-bit storage on a little-endian
                     * machine). Fsf is NOT a new field - confirmed
                     * against a real PCSX2 upstream reference clone's
                     * DisR5900asm.cpp dest_fsf() macro
                     * ("(disasmOpcode>>21)&3"), Fsf lives in the exact
                     * same two bits as this decoder's own destmask
                     * value's low 2 bits, just reinterpreted as a lane
                     * INDEX here instead of a per-lane bitmask. */
                    if (ft != 0) {
                        uint32_t fsf_lane = destmask & 0x3u;
                        uint32_t raw = vu0_vf_read_lane(st, fs, fsf_lane);
                        vu0_vi_write(st, ft, raw & 0xFFFFu);
                    }
                } else if (idx == 61) {
                    /* VMFIR: broadcasts the sign-extended 16-bit
                     * VI[fs] value (raw bit pattern, not a float
                     * conversion) into every destmask-selected lane of
                     * VF[ft] - ported from PCSX2's _vuMFIR
                     * ("VF[_Ft_].SL[i] = (s32)VI[_Is_].SS[0]" - a
                     * signed 16-to-32 sign-extension of the RAW
                     * bits). */
                    if (ft != 0) {
                        int16_t vi16 = (int16_t)(vu0_vi_read(st, fs) & 0xFFFFu);
                        uint32_t sval = (uint32_t)(int32_t)vi16;
                        for (int lane = 0; lane < 4; lane++) {
                            if (!(destmask & (0x8u >> lane))) continue;
                            vu0_vf_write_lane(st, ft, (uint32_t)lane, sval);
                        }
                    }
                } else if (idx == 62) {
                    /* VILWR: VI[ft] = the low 16 bits of VU0 mem at
                     * quadword index VI[fs], single lane selected by
                     * destmask (the same single-bit-only convention
                     * VISWR already uses - X=0x8,Y=0x4,Z=0x2,W=0x1) -
                     * ported from PCSX2's _vuILWR (reads a u16* at
                     * byte offsets 0/4/8/12, i.e. the low half-word of
                     * each 32-bit lane). */
                    if (ft != 0) {
                        int lane = (destmask == 0x8u) ? 0 : (destmask == 0x4u) ? 1 : (destmask == 0x2u) ? 2 : 3;
                        uint32_t addr_vi = vu0_vi_read(st, fs);
                        uint32_t off = vu0_mem_addr(addr_vi, (uint32_t)lane);
                        uint32_t lo16 = (uint32_t)st->vu0_mem[off] | ((uint32_t)st->vu0_mem[off + 1] << 8);
                        vu0_vi_write(st, ft, lo16);
                    }
                } else if (idx == 56 || idx == 57 || idx == 58) {
                    /* VDIV(56)/VSQRT(57)/VRSQRT(58) (Round 29
                     * continued, 25th change): the division/sqrt
                     * family that produces the Q register value.
                     * Fsf/Ftf are independent 2-bit lane selectors
                     * living in destmask's low/high 2 bits
                     * respectively (destmask&3 = Fsf, (destmask>>2)&3
                     * = Ftf - confirmed against PCSX2's own
                     * DisR5900asm.cpp dest_fsf()/dest_ftf() macros,
                     * "(disasmOpcode>>21)&3" / "(disasmOpcode>>23)&3"
                     * - the same 4-bit field this decoder already
                     * calls destmask, just split in half and
                     * reinterpreted as two lane indices here instead
                     * of a per-lane bitmask). VSQRT only uses Ftf (no
                     * FS operand at all - confirmed via
                     * DisR5900asm.cpp's P_VSQRT, which prints only
                     * FT). Divide-by-zero produces a signed FLT_MAX
                     * bit pattern (real PS2 hardware has no true IEEE
                     * infinity) rather than a real float inf/nan,
                     * ported from PCSX2's own VUops.cpp _vuDIV/
                     * _vuRSQRT (the sign is the XOR of FT's and FS's
                     * sign bits). Result is written directly into
                     * cop2_ctrl[22] - this project's single source of
                     * truth for Q (already read by VMULq/VADDq/etc
                     * above); real PCSX2 keeps a separate VU->q field
                     * and mirrors it into VI[22] via its own
                     * SYNCFDIV() after every Q-producing op purely for
                     * its broadcast helpers' convenience, so no
                     * second piece of state is needed in this
                     * project's simpler model. No status-flag
                     * modeling, consistent with this project's
                     * existing float datapath not tracking MAC/status
                     * flags anywhere else either. */
                    uint32_t fsf_lane = destmask & 0x3u;
                    uint32_t ftf_lane = (destmask >> 2) & 0x3u;
                    uint32_t uft = vu0_vf_read_lane(st, ft, ftf_lane);
                    float ftv; memcpy(&ftv, &uft, 4);
                    uint32_t qbits;
                    if (idx == 57) {
                        /* VSQRT: Q = sqrt(|FT|), no FS operand. */
                        float r = sqrtf(fabsf(ftv));
                        memcpy(&qbits, &r, 4);
                    } else {
                        uint32_t ufs = vu0_vf_read_lane(st, fs, fsf_lane);
                        float fsv; memcpy(&fsv, &ufs, 4);
                        if (ftv == 0.0f) {
                            uint32_t sign_diff = (uft ^ ufs) & 0x80000000u;
                            if (idx == 56) {
                                /* VDIV: x/0 or 0/0 both produce the
                                 * signed FLT_MAX clamp on real
                                 * hardware - the distinction only
                                 * affects the (unmodeled) status
                                 * flag. */
                                qbits = sign_diff ? 0xFF7FFFFFu : 0x7F7FFFFFu;
                            } else {
                                /* VRSQRT (idx58): fs!=0 clamps to
                                 * signed FLT_MAX like VDIV; fs==0 (a
                                 * genuine 0/sqrt(0)) clamps to signed
                                 * zero instead. */
                                if (fsv != 0.0f) qbits = sign_diff ? 0xFF7FFFFFu : 0x7F7FFFFFu;
                                else qbits = sign_diff ? 0x80000000u : 0x00000000u;
                            }
                        } else if (idx == 56) {
                            float r = fsv / ftv;
                            memcpy(&qbits, &r, 4);
                        } else {
                            float temp = sqrtf(fabsf(ftv));
                            float r = fsv / temp;
                            memcpy(&qbits, &r, 4);
                        }
                    }
                    vu0_vi_write(st, 22, qbits);
                } else if (idx == 59) {
                    /* VWAITQ: a true no-op - confirmed against real
                     * PCSX2 upstream's own _vuWAITQ, which has an
                     * empty body even there (PCSX2 itself computes Q
                     * synchronously with no latency to wait on), so
                     * this project needs no Q "busy" timing model at
                     * all, resolving the concern flagged in earlier
                     * rounds' docs. */
                } else if (idx == 64 || idx == 65) {
                    /* VRNEXT(64)/VRGET(65) (Round 29 continued, 24th
                     * change): the VU0 "R register" - a real 24-bit
                     * LFSR pseudo-random generator, always kept in
                     * float-bit-pattern range [1.0,2.0) (exponent/sign
                     * fixed at 0x3F800000, only the low 23 mantissa
                     * bits actually vary). REG_R is control register
                     * index 20 (PCSX2's VU.h REG_R=20) - no new state
                     * needed, already reachable via the existing
                     * vu0_vi_read/write helpers this file already uses
                     * for I(21)/Q(22). VRGET just broadcasts R's
                     * CURRENT value into every destmask-selected lane
                     * of VF[ft] (guarded ft!=0); VRNEXT advances the
                     * LFSR FIRST (ported bit-exact from PCSX2's
                     * AdvanceLFSR: x=bit4, y=bit22, shift left 1, xor
                     * bit0 with x^y, then re-clamp to the [1.0,2.0)
                     * bit pattern), then broadcasts the NEW value the
                     * same way. */
                    if (idx == 64) {
                        uint32_t r = vu0_vi_read(st, 20);
                        uint32_t x = (r >> 4) & 1u;
                        uint32_t y = (r >> 22) & 1u;
                        r <<= 1;
                        r ^= (x ^ y);
                        r = (r & 0x7FFFFFu) | 0x3F800000u;
                        vu0_vi_write(st, 20, r);
                    }
                    if (ft != 0) {
                        uint32_t r = vu0_vi_read(st, 20);
                        for (int lane = 0; lane < 4; lane++) {
                            if (!(destmask & (0x8u >> lane))) continue;
                            vu0_vf_write_lane(st, ft, (uint32_t)lane, r);
                        }
                    }
                } else if (idx == 66) {
                    /* VRINIT: R = 0x3F800000 | (VF[fs][Fsf] &
                     * 0x7FFFFF) - seeds R's mantissa from a single VF
                     * lane's raw bits. Fsf reuses destmask's low 2
                     * bits as a lane index, the same convention
                     * VMTIR/VRXOR use. */
                    uint32_t fsf_lane = destmask & 0x3u;
                    uint32_t raw = vu0_vf_read_lane(st, fs, fsf_lane);
                    vu0_vi_write(st, 20, 0x3F800000u | (raw & 0x7FFFFFu));
                } else if (idx == 67) {
                    /* VRXOR: R = 0x3F800000 | ((R ^ VF[fs][Fsf]) &
                     * 0x7FFFFF) - XORs R's mantissa with a single VF
                     * lane's raw bits. */
                    uint32_t fsf_lane = destmask & 0x3u;
                    uint32_t raw = vu0_vf_read_lane(st, fs, fsf_lane);
                    uint32_t r = vu0_vi_read(st, 20);
                    vu0_vi_write(st, 20, 0x3F800000u | ((r ^ raw) & 0x7FFFFFu));
                } else if (idx == 31) {
                    /* VCLIPw (Round 29 continued, 26th change): judges
                     * |VF[fs].x|,|VF[fs].y|,|VF[fs].z| against
                     * |VF[ft].w|, treating the raw 32-bit bit patterns
                     * as SIGNED INTEGERS with a sign-flip XOR trick
                     * (NOT a float comparison) - ported bit-exact from
                     * a real PCSX2 upstream reference clone's
                     * VUops.cpp _vuCLIP. No Fsf/Ftf lane selector at
                     * all - xyz vs w is hardwired, confirmed via
                     * DisR5900asm.cpp's P_VCLIPw formatter ("vclip
                     * %sxyz, %sw"). Shifts 6 new judgment bits into
                     * the CLIP flag register each call (this project's
                     * cop2_ctrl[18], reusing the existing generic VI
                     * array the same way R(20)/I(21)/Q(22) already do
                     * - REG_CLIP_FLAG=18 in PCSX2's VU.h, and just
                     * like the Q-register unification decision in the
                     * 25th change, this project writes directly into
                     * that single slot rather than modeling PCSX2's
                     * separate clipflag/SYNCCLIPFLAG() split), masked
                     * to the low 24 bits (4 calls' worth of judgment
                     * history, matching real hardware). This is the
                     * only VU0 macro-mode op this session that needed
                     * genuinely new reachable state - resolved by
                     * reusing control-register slot 18, which this
                     * decoder's CFC2/MTC2/QMTC2 paths already handle
                     * generically for any register index. */
                    uint32_t ftw = vu0_vf_read_lane(st, ft, 3);
                    int32_t value = (int32_t)ftw;
                    value = (ftw & 0x7f800000u) ? (value & 0x7fffffff) : 0x007fffff;
                    uint32_t fsx = vu0_vf_read_lane(st, fs, 0);
                    uint32_t fsy = vu0_vf_read_lane(st, fs, 1);
                    uint32_t fsz = vu0_vf_read_lane(st, fs, 2);
                    uint32_t clip = vu0_vi_read(st, 18);
                    clip <<= 6;
                    if ((int32_t)(fsx ^ 0x00000000u) > value) clip |= 0x01u;
                    if ((int32_t)(fsx ^ 0x80000000u) > value) clip |= 0x02u;
                    if ((int32_t)(fsy ^ 0x00000000u) > value) clip |= 0x04u;
                    if ((int32_t)(fsy ^ 0x80000000u) > value) clip |= 0x08u;
                    if ((int32_t)(fsz ^ 0x00000000u) > value) clip |= 0x10u;
                    if ((int32_t)(fsz ^ 0x80000000u) > value) clip |= 0x20u;
                    clip &= 0xFFFFFFu;
                    vu0_vi_write(st, 18, clip);
                } else {
                    halt("unimplemented COP2 SPECIAL2 sub-opcode (VU0 vector datapath not implemented)");
                    return 1;
                }
            } else if (funct == 0x32) {
                /* VIADDI: VI[ft] = VI[fs] + sign_extend(imm), where
                 * imm is the raw 5-bit field at the SAME bit position
                 * (6-10) as FD in the other CO-format arithmetic ops
                 * above, reused here as an immediate rather than a
                 * register index - and unlike VIADD/VISUB/VIAND/VIOR
                 * (dest=FD, confirmed via a live "viadd vi02,vi00,vi00"
                 * disassembly), VIADDI's real operand order is
                 * dest=FT, src=FS, imm=SA (confirmed against PCSX2's
                 * own DisR5900asm.cpp P_VIADDI: "viaddi FT, FS,
                 * 0x%x(SA)"). The sign-extension itself is ported
                 * verbatim from PCSX2's VUops.cpp _vuIADDI rather than
                 * reinvented: imm = (code>>6)&0x1f, then
                 * (imm&0x10 ? 0xfff0 : 0) | (imm&0xf) - a real-hardware
                 * quirk (effectively a signed 4-bit magnitude with a
                 * separate sign bit, not a plain 5-bit two's-complement
                 * sign-extend), closing the gap this project's own
                 * VIADD/VISUB/VIAND/VIOR comment already flagged as a
                 * "scoped future gap". */
                uint32_t imm5 = fd; /* raw bits 6-10, reused as SA/imm here */
                uint32_t imm = ((imm5 & 0x10u) ? 0xFFF0u : 0u) | (imm5 & 0xFu);
                if (ft != 0) {
                    uint32_t a = vu0_vi_read(st, fs);
                    vu0_vi_write(st, ft, (a + imm) & 0xFFFFu);
                }
            } else if (funct == 0x30 || funct == 0x31 || funct == 0x34 || funct == 0x35) {
                /* VIADD/VISUB/VIAND/VIOR - plain integer ALU on VI
                 * registers (VI[fd] = VI[fs] op VI[ft]), found right
                 * after the VSUB.xyzw sequence above in a real BIOS
                 * "clear every VU0 register" init routine (VF0-31
                 * cleared via self-subtract, VI0-15 cleared via
                 * self-add-of-zero: viadd viN,vi00,vi00). Confirmed
                 * against R5900OpcodeTables.cpp's SPECIAL1 table
                 * (funct 0x30=VIADD,0x31=VISUB,0x34=VIAND,0x35=VIOR;
                 * FD/FS/FT field roles carried over unchanged from
                 * VSUB's, confirmed via a live PCSX2 disassembly of
                 * "viadd vi02,vi00,vi00" - fd=2,fs=0,ft=0). Real VI
                 * registers are 16-bit (PCSX2's REG_VI union), so
                 * VIADD/VISUB results are masked to 16 bits; VIAND/
                 * VIOR operate on already-16-bit-clean values so no
                 * extra masking is needed there. VIADDI (immediate
                 * form) and VIAND/VIOR's less common siblings aren't
                 * implemented - not seen yet, a scoped future gap. */
                uint32_t a = vu0_vi_read(st, fs);
                uint32_t b = vu0_vi_read(st, ft);
                uint32_t r;
                switch (funct) {
                case 0x30: r = (a + b) & 0xFFFFu; break; /* VIADD */
                case 0x31: r = (a - b) & 0xFFFFu; break; /* VISUB */
                case 0x34: r = a & b; break;              /* VIAND */
                default:   r = a | b; break;              /* VIOR  */
                }
                vu0_vi_write(st, fd, r);
            } else if (funct >= 0x20 && funct <= 0x27) {
                /* VADDq/VMADDq/VADDi/VMADDi/VSUBq/VMSUBq/VSUBi/VMSUBi
                 * (funct 0x20-0x27) - Round 446 (task #242) originally
                 * added ONLY funct==0x20 (VADDq) as an empirically-
                 * tested fix for a real observed halt; task #249
                 * (Round 455) completes the full row, since this
                 * file's own Round 446 comment already correctly
                 * identified and named all 8 real siblings but left
                 * 7 of them unimplemented as a "scoped future gap,
                 * not guessed at". Round 455 closes that gap using
                 * real, directly-fetched PCSX2 source (pcsx2/
                 * VUops.cpp, fetched this round from the live GitHub
                 * repo, not guessed): each sibling is a thin wrapper
                 * calling the SAME applyBinaryMACOpBroadcast/
                 * applyTernaryMACOpBroadcast templates this file's
                 * own funct<=0x1F row already ports from (see that
                 * row's own comment) - real, cited definitions:
                 *   static __fi void _vuADDq(VURegs* VU)   { vuADDbc(VU, VU->VI[REG_Q].UL); }
                 *   static __fi void _vuMADDq(VURegs* VU)  { vuMADDbc(VU, VU->VI[REG_Q].UL); }
                 *   static __fi void _vuADDi(VURegs* VU)   { vuADDbc_addsubhack(VU, VU->VI[REG_I].UL); }
                 *   static __fi void _vuMADDi(VURegs* VU)  { vuMADDbc(VU, VU->VI[REG_I].UL); }
                 *   static __fi void _vuSUBq(VURegs* VU)   { vuSUBbc(VU, VU->VI[REG_Q].UL); }
                 *   static __fi void _vuMSUBq(VURegs* VU)  { vuMSUBbc(VU, VU->VI[REG_Q].UL); }
                 *   static __fi void _vuSUBi(VURegs* VU)   { vuSUBbc(VU, VU->VI[REG_I].UL); }
                 *   static __fi void _vuMSUBi(VURegs* VU)  { vuMSUBbc(VU, VU->VI[REG_I].UL); }
                 * where vuADDbc/vuSUBbc = applyBinaryMACOpBroadcast
                 * (dst=FD, FD[lane]=FS[lane] OP bc) and vuMADDbc/
                 * vuMSUBbc = applyTernaryMACOpBroadcast (dst=FD,
                 * FD[lane]=ACC[lane] OP FS[lane]*bc) - i.e. the MADD/
                 * MSUB variants here write FD, reading ACC as a THIRD
                 * operand (st->vu0_acc[4], the same accumulator array
                 * this file's own funct<=0x1F VMADDx/y/z/w block
                 * already uses) - never writing back into ACC itself
                 * (that's the separate VMADDA/VMSUBA family, same
                 * asymmetry this file's own existing ACC-family
                 * comment already documents for the non-broadcast
                 * VMADD/VMSUB at funct 0x29/0x2D below). _vuADDi uses
                 * "vuADDbc_addsubhack" in real PCSX2 (a TriAce-games
                 * float-rounding compatibility hack, CHECK_VUADDSUBHACK-
                 * gated) - not ported here, matching this project's
                 * existing, already-stated policy of not modeling
                 * per-game compatibility hacks; the un-hacked path
                 * (plain float add) is what CHECK_VUADDSUBHACK's own
                 * disabled branch already falls back to, so this is a
                 * faithful "hack disabled" real-hardware-equivalent
                 * implementation, not an approximation. REG_Q=22,
                 * REG_I=21 (PCSX2's VU.h, same mapping this file's
                 * own funct<=0x1F Q/I-row already uses). Untested
                 * against a NEW real halt-site disassembly (unlike
                 * 0x20 itself, which was) - these 7 siblings are
                 * implemented from the real, cited PCSX2 semantics on
                 * the well-evidenced expectation (same opcode row,
                 * same operand shape, real hardware/PCSX2-confirmed
                 * ordering) that they'll be needed next; verified via
                 * the full host-native regression suite before being
                 * kept, and confirmed to introduce no regression
                 * against the Round 446 EE-progress baseline. */
                uint32_t op_idx = funct - 0x20u; /* 0=ADDq,1=MADDq,2=ADDi,3=MADDi,4=SUBq,5=MSUBq,6=SUBi,7=MSUBi */
                int is_i = (op_idx & 0x2u) != 0;      /* 2,3,6,7 use I; 0,1,4,5 use Q */
                int is_mac = (op_idx & 0x1u) != 0;    /* 1,3,5,7 are MADD/MSUB (read ACC) */
                int is_sub = (op_idx & 0x4u) != 0;    /* 4,5,6,7 are SUB-family */
                uint32_t ubc = vu0_vi_read(st, is_i ? 21u : 22u);
                float bc; memcpy(&bc, &ubc, 4);
                for (int lane = 0; lane < 4; lane++) {
                    if (!(destmask & (0x8u >> lane))) continue;
                    uint32_t ua = vu0_vf_read_lane(st, fs, (uint32_t)lane);
                    float a, r; uint32_t ur;
                    memcpy(&a, &ua, 4);
                    if (is_mac) {
                        uint32_t uacc = st->vu0_acc[lane]; float acc; memcpy(&acc, &uacc, 4);
                        r = is_sub ? (acc - a * bc) : (acc + a * bc);
                    } else {
                        r = is_sub ? (a - bc) : (a + bc);
                    }
                    memcpy(&ur, &r, 4);
                    vu0_vf_write_lane(st, fd, (uint32_t)lane, ur);
                }
            } else {
                halt("unimplemented COP2 CO-format sub-opcode (VU0 vector datapath not implemented)");
                return 1;
            }
        }
        break;
