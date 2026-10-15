/* GPL-3.0-or-later. Shared scalar/AOT semantics from ee_core.c. */
    case 0x11: /* COP1 (FPU, single-precision only) */
        switch (rs) {
        case 0x00: /* MFC1 */ if (rt) GPR(rt) = sext32(st->fpr[rd]); break;
        case 0x02: /* CFC1 */
            if (rt) {
                if (rd == 31) GPR(rt) = sext32(st->fcr31);
                else if (rd == 0) GPR(rt) = sext32(0x2E00u);
                else GPR(rt) = 0;
            }
            break;
        case 0x04: /* MTC1 */ st->fpr[rd] = rt32; break;
        case 0x06: /* CTC1 */ if (rd == 31) st->fcr31 = rt32; break;
        case 0x10: { /* COP1.S - single-precision arithmetic, fd=sa fs=rd ft=rt */
            uint32_t fd = sa, fs = rd, ft = rt;
            switch (funct) {
            case 0x00: /* ADD.S */ {
                float r = fpu_double(st->fpr[fs]) + fpu_double(st->fpr[ft]);
                st->fpr[fd] = float_to_bits(r);
                if (!fpu_check_overflow(&st->fpr[fd])) fpu_check_underflow(&st->fpr[fd]);
            } break;
            case 0x01: /* SUB.S */ {
                float r = fpu_double(st->fpr[fs]) - fpu_double(st->fpr[ft]);
                st->fpr[fd] = float_to_bits(r);
                if (!fpu_check_overflow(&st->fpr[fd])) fpu_check_underflow(&st->fpr[fd]);
            } break;
            case 0x02: /* MUL.S */ {
                float r = fpu_double(st->fpr[fs]) * fpu_double(st->fpr[ft]);
                st->fpr[fd] = float_to_bits(r);
                if (!fpu_check_overflow(&st->fpr[fd])) fpu_check_underflow(&st->fpr[fd]);
            } break;
            case 0x03: /* DIV.S */ {
                uint32_t divisor = st->fpr[ft], dividend = st->fpr[fs];
                if ((divisor & 0x7F800000u) == 0) {
                    /* divide-by-zero (denormal counts as zero too): result is
                     * signed +/-Fmax, sign = XOR of operand signs. */
                    st->fpr[fd] = ((divisor ^ dividend) & 0x80000000u) | FPU_POS_FMAX;
                } else {
                    float r = fpu_double(dividend) / fpu_double(divisor);
                    st->fpr[fd] = float_to_bits(r);
                    fpu_check_overflow(&st->fpr[fd]);
                    fpu_check_underflow(&st->fpr[fd]);
                }
            } break;
            case 0x05: /* ABS.S */ st->fpr[fd] = st->fpr[fs] & 0x7fffffffu; break;
            case 0x06: /* MOV.S */ st->fpr[fd] = st->fpr[fs]; break;
            case 0x07: /* NEG.S */ st->fpr[fd] = st->fpr[fs] ^ 0x80000000u; break;
            case 0x24: /* CVT.W.S (float -> int32) */
                if ((st->fpr[fs] & 0x7F800000u) <= 0x4E800000u) {
                    st->fpr[fd] = (uint32_t)(int32_t)bits_to_float(st->fpr[fs]);
                } else {
                    st->fpr[fd] = (st->fpr[fs] & 0x80000000u) ? 0x80000000u : 0x7fffffffu;
                }
                break;
            case 0x04: /* SQRT.S - real hardware/PCSX2 quirk: the source
                        * operand is Ft (rt field), NOT Fs, and Fs
                        * (rd field) is unused - ported exactly from
                        * PCSX2's SQRT_S(), not a typo. Documented
                        * simplification, matching this file's
                        * existing FPU scope: the D/I exception-cause
                        * control-register flags PCSX2 also sets here
                        * are NOT modeled (nothing in this project
                        * raises FPU exceptions from them yet - same
                        * simplification already in place for every
                        * other FPU op above, which also skip the
                        * O/U/SO/SU flags). */
                if ((st->fpr[ft] & 0x7F800000u) == 0) {
                    st->fpr[fd] = st->fpr[ft] & 0x80000000u; /* +/-0 */
                } else if (st->fpr[ft] & 0x80000000u) {
                    float r = sqrtf(fabsf(fpu_double(st->fpr[ft])));
                    st->fpr[fd] = float_to_bits(r);
                } else {
                    float r = sqrtf(fpu_double(st->fpr[ft]));
                    st->fpr[fd] = float_to_bits(r);
                }
                break;
            case 0x16: /* RSQRT.S: fd = fs / sqrt(ft) - ported from
                        * PCSX2's RSQRT_S(). Same documented flag
                        * simplification as SQRT.S above. */
                if ((st->fpr[ft] & 0x7F800000u) == 0) {
                    /* ft is +/-0 (denormals-are-zero): result is
                     * +/-Fmax, sign taken from ft. */
                    st->fpr[fd] = (st->fpr[ft] & 0x80000000u) | FPU_POS_FMAX;
                } else if (st->fpr[ft] & 0x80000000u) {
                    float denom = sqrtf(fabsf(fpu_double(st->fpr[ft])));
                    float r = fpu_double(st->fpr[fs]) / denom;
                    st->fpr[fd] = float_to_bits(r);
                    fpu_check_overflow(&st->fpr[fd]);
                    fpu_check_underflow(&st->fpr[fd]);
                } else {
                    float r = fpu_double(st->fpr[fs]) / sqrtf(fpu_double(st->fpr[ft]));
                    st->fpr[fd] = float_to_bits(r);
                    fpu_check_overflow(&st->fpr[fd]);
                    fpu_check_underflow(&st->fpr[fd]);
                }
                break;
            case 0x28: /* MAX.S - ported from PCSX2's MAX_S()/fp_max():
                        * a bit-level signed-int max/min, not a float
                        * compare - see fp_max()'s comment. No
                        * overflow/underflow clamping needed (result
                        * is always one of the two original values). */
                st->fpr[fd] = fp_max(st->fpr[fs], st->fpr[ft]);
                break;
            case 0x29: /* MIN.S - ported from PCSX2's MIN_S()/fp_min(). */
                st->fpr[fd] = fp_min(st->fpr[fs], st->fpr[ft]);
                break;
            case 0x18: /* ADDA.S - ACC = fs + ft, ported from PCSX2's
                        * ADDA_S(). */
            {
                float r = fpu_double(st->fpr[fs]) + fpu_double(st->fpr[ft]);
                st->acc = float_to_bits(r);
                if (!fpu_check_overflow(&st->acc)) fpu_check_underflow(&st->acc);
            } break;
            case 0x19: /* SUBA.S - ACC = fs - ft, ported from SUBA_S(). */
            {
                float r = fpu_double(st->fpr[fs]) - fpu_double(st->fpr[ft]);
                st->acc = float_to_bits(r);
                if (!fpu_check_overflow(&st->acc)) fpu_check_underflow(&st->acc);
            } break;
            case 0x1A: /* MULA.S - ACC = fs * ft, ported from MULA_S(). */
            {
                float r = fpu_double(st->fpr[fs]) * fpu_double(st->fpr[ft]);
                st->acc = float_to_bits(r);
                if (!fpu_check_overflow(&st->acc)) fpu_check_underflow(&st->acc);
            } break;
            case 0x1C: /* MADD.S - fd = ACC + (fs * ft), ported from
                        * PCSX2's MADD_S(). Real hardware/PCSX2 quirk
                        * worth preserving exactly: the intermediate
                        * product is run through fpuDouble() a SECOND
                        * time when it's read back for the addition
                        * (PCSX2's own FPRreg temp; temp.f = fpuDouble
                        * (fs)*fpuDouble(ft); then fpuDouble(temp.UL)
                        * again) - unlike MADDA.S below, which doesn't
                        * do this second pass. Not an oversight to
                        * "simplify away" - ported as-is. */
            {
                float prod = fpu_double(st->fpr[fs]) * fpu_double(st->fpr[ft]);
                uint32_t prod_bits = float_to_bits(prod);
                float r = fpu_double(st->acc) + fpu_double(prod_bits);
                st->fpr[fd] = float_to_bits(r);
                if (!fpu_check_overflow(&st->fpr[fd])) fpu_check_underflow(&st->fpr[fd]);
            } break;
            case 0x1D: /* MSUB.S - fd = ACC - (fs * ft), same
                        * double-fpuDouble()-pass quirk as MADD.S
                        * above, ported from PCSX2's MSUB_S(). */
            {
                float prod = fpu_double(st->fpr[fs]) * fpu_double(st->fpr[ft]);
                uint32_t prod_bits = float_to_bits(prod);
                float r = fpu_double(st->acc) - fpu_double(prod_bits);
                st->fpr[fd] = float_to_bits(r);
                if (!fpu_check_overflow(&st->fpr[fd])) fpu_check_underflow(&st->fpr[fd]);
            } break;
            case 0x1E: /* MADDA.S - ACC += fs * ft, ported from PCSX2's
                        * MADDA_S(). NOTE: no intermediate
                        * fpuDouble()-of-the-product pass here, unlike
                        * MADD.S above - that asymmetry is real,
                        * ported exactly as PCSX2 has it. */
            {
                float r = fpu_double(st->acc) + (fpu_double(st->fpr[fs]) * fpu_double(st->fpr[ft]));
                st->acc = float_to_bits(r);
                if (!fpu_check_overflow(&st->acc)) fpu_check_underflow(&st->acc);
            } break;
            case 0x1F: /* MSUBA.S - ACC -= fs * ft, ported from MSUBA_S(). */
            {
                float r = fpu_double(st->acc) - (fpu_double(st->fpr[fs]) * fpu_double(st->fpr[ft]));
                st->acc = float_to_bits(r);
                if (!fpu_check_overflow(&st->acc)) fpu_check_underflow(&st->acc);
            } break;
            case 0x32: /* C.EQ.S */
                if (fpu_double(st->fpr[fs]) == fpu_double(st->fpr[ft])) st->fcr31 |= 0x00800000u;
                else st->fcr31 &= ~0x00800000u;
                break;
            case 0x34: /* C.LT.S */
                if (fpu_double(st->fpr[fs]) < fpu_double(st->fpr[ft])) st->fcr31 |= 0x00800000u;
                else st->fcr31 &= ~0x00800000u;
                break;
            case 0x36: /* C.LE.S */
                if (fpu_double(st->fpr[fs]) <= fpu_double(st->fpr[ft])) st->fcr31 |= 0x00800000u;
                else st->fcr31 &= ~0x00800000u;
                break;
            default:
                halt("unimplemented COP1.S funct (SQRT/RSQRT/MADD/MAX/MIN family not implemented)");
                return 1;
            }
        } break;
        case 0x14: /* COP1.W - only CVT.S.W (int32 -> float) */
            if (funct == 0x20) {
                uint32_t fd = sa, fs = rd;
                st->fpr[fd] = float_to_bits((float)(int32_t)st->fpr[fs]);
            } else {
                halt("unimplemented COP1.W funct");
                return 1;
            }
            break;
        case 0x08: /* COP1 BC (branch on FP condition flag) - sub-selected
                    * by the rt field (matches PCSX2's tbl_COP1_BC1[32],
                    * indexed 0=BC1F, 1=BC1T, 2=BC1FL, 3=BC1TL). All
                    * four variants implemented as of Round 400 - see
                    * the BC1FL/BC1TL cases below for the "likely"
                    * nullify-delay-slot semantics and citation. */
            switch (rt) {
            case 0x00: /* BC1F - branch if the FP condition flag (fcr31
                        * bit 0x00800000, set by C.EQ/LT/LE.S) is
                        * CLEAR. Ported from PCSX2's BC1()/BC1F(). */
                st->branch_pending = 1; /* regular branch: delay slot always executes */
                if (!(st->fcr31 & 0x00800000u)) BRANCH_TO(this_pc + 4 + (imm << 2));
                break;
            case 0x01: /* BC1T - branch if the flag is SET. */
                st->branch_pending = 1;
                if ((st->fcr31 & 0x00800000u)) BRANCH_TO(this_pc + 4 + (imm << 2));
                break;
            case 0x02: /* BC1FL - "branch likely" variant of BC1F
                        * (Round 400/task GAP-1). This project's
                        * earlier comment here claiming "no likely-
                        * branch infrastructure for ANY branch" was
                        * stale by the time of this fix - the integer
                        * likely family (BEQL/BNEL/BLEZL/BGTZL, primary
                        * 0x14-0x17, and BLTZL/BGEZL/BLTZALL/BGEZALL,
                        * REGIMM 0x02/0x03/0x12/0x13) was already
                        * implemented elsewhere in this same file,
                        * using the exact nullify-delay-slot-on-not-
                        * taken pattern reused here verbatim: `st->pc =
                        * fallthrough_pc + 4; st->next_pc =
                        * fallthrough_pc + 8;` skips straight past the
                        * delay slot instead of letting it execute,
                        * matching real MIPS II+ "likely" semantics
                        * (ported from PCSX2's Interpreter.cpp
                        * tbl_COP1_BC1[32] convention, same real source
                        * already cited for BC1F/BC1T immediately
                        * above). Only the branch CONDITION differs
                        * from BC1F (fcr31 bit 0x00800000 clear). */
                if (!(st->fcr31 & 0x00800000u)) BRANCH_TO(this_pc + 4 + (imm << 2));
                else { st->pc = fallthrough_pc + 4; st->next_pc = fallthrough_pc + 8; }
                break;
            case 0x03: /* BC1TL - "branch likely" variant of BC1T; same
                        * nullify-on-not-taken semantics as BC1FL
                        * above, condition is fcr31 bit 0x00800000 SET
                        * (matches BC1T). */
                if ((st->fcr31 & 0x00800000u)) BRANCH_TO(this_pc + 4 + (imm << 2));
                else { st->pc = fallthrough_pc + 4; st->next_pc = fallthrough_pc + 8; }
                break;
            default:
                halt("unimplemented COP1 BC variant");
                return 1;
            }
            break;
        default:
            halt("unimplemented COP1 sub-opcode");
            return 1;
        }
        break;
