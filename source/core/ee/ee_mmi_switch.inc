/* GPL-3.0-or-later. Shared scalar/AOT semantics from ee_core.c. */
    case 0x1C: /* MMI */
        switch (funct) {
        case 0x00: /* MADD */ {
            int64_t acc = (int64_t)((uint64_t)st->lo.ud0 & 0xFFFFFFFFu) | ((int64_t)(int32_t)(uint32_t)st->hi.ud0 << 32);
            int64_t res = acc + (int64_t)(int32_t)rs32 * (int64_t)(int32_t)rt32;
            st->lo.ud0 = sext32((uint32_t)(res & 0xFFFFFFFFu));
            st->hi.ud0 = sext32((uint32_t)(res >> 32));
            if (rd) GPR(rd) = st->lo.ud0;
        } break;
        case 0x01: /* MADDU */ {
            uint64_t acc = ((uint64_t)(uint32_t)st->lo.ud0) | ((uint64_t)(uint32_t)st->hi.ud0 << 32);
            uint64_t res = acc + (uint64_t)rs32 * (uint64_t)rt32;
            st->lo.ud0 = sext32((uint32_t)(res & 0xFFFFFFFFu));
            st->hi.ud0 = sext32((uint32_t)(res >> 32));
            if (rd) GPR(rd) = st->lo.ud0;
        } break;
        case 0x04: /* PLZCW */
            if (rd) {
                for (int n = 0; n < 2; n++) {
                    int32_t v = (int32_t)lane_w(st->gpr[rs], n);
                    int count = 0;
                    /* count leading bits equal to the sign bit, excluding it */
                    uint32_t sign = (uint32_t)v >> 31;
                    uint32_t x = (uint32_t)v;
                    for (int b = 30; b >= 0; b--) {
                        if (((x >> b) & 1) != sign) break;
                        count++;
                    }
                    set_lane_w(&st->gpr[rd], n, (uint32_t)count);
                }
            }
            break;
        case 0x10: /* MFHI1 */ if (rd) GPR(rd) = st->hi.ud1; break;
        case 0x11: /* MTHI1 */ st->hi.ud1 = GPR(rs); break;
        case 0x12: /* MFLO1 */ if (rd) GPR(rd) = st->lo.ud1; break;
        case 0x13: /* MTLO1 */ st->lo.ud1 = GPR(rs); break;
        case 0x18: /* MULT1 */ {
            int64_t res = (int64_t)(int32_t)rs32 * (int64_t)(int32_t)rt32;
            st->lo.ud1 = sext32((uint32_t)(res & 0xFFFFFFFFu));
            st->hi.ud1 = sext32((uint32_t)(res >> 32));
            if (rd) GPR(rd) = st->lo.ud1;
        } break;
        case 0x19: /* MULTU1 */ {
            uint64_t res = (uint64_t)rs32 * (uint64_t)rt32;
            st->lo.ud1 = sext32((uint32_t)(res & 0xFFFFFFFFu));
            st->hi.ud1 = sext32((uint32_t)(res >> 32));
            if (rd) GPR(rd) = st->lo.ud1;
        } break;
        case 0x1A: /* DIV1: R5900 zero/overflow results; no host UB. */
            if (rs32 == 0x80000000u && rt32 == 0xffffffffu) {
                st->lo.ud1 = sext32(0x80000000u);
                st->hi.ud1 = 0;
            } else if (rt32 != 0) {
                st->lo.ud1 = sext32((uint32_t)((int32_t)rs32 / (int32_t)rt32));
                st->hi.ud1 = sext32((uint32_t)((int32_t)rs32 % (int32_t)rt32));
            } else {
                st->lo.ud1 = (int32_t)rs32 < 0 ? 1 : UINT64_MAX;
                st->hi.ud1 = sext32(rs32);
            }
            break;
        case 0x1B: /* DIVU1 */
            st->lo.ud1 = rt32 ? sext32(rs32 / rt32) : UINT64_MAX;
            st->hi.ud1 = sext32(rt32 ? rs32 % rt32 : rs32);
            break;
        case 0x34: /* PSLLH */ if (rd) for (int n = 0; n < 8; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)(lane_h(st->gpr[rt], n) << (sa & 0xF))); break;
        case 0x36: /* PSRLH */ if (rd) for (int n = 0; n < 8; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)(lane_h(st->gpr[rt], n) >> (sa & 0xF))); break;
        case 0x37: /* PSRAH */ if (rd) for (int n = 0; n < 8; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)((int16_t)lane_h(st->gpr[rt], n) >> (sa & 0xF))); break;
        case 0x3C: /* PSLLW */ if (rd) for (int n = 0; n < 4; n++) set_lane_w(&st->gpr[rd], n, lane_w(st->gpr[rt], n) << sa); break;
        case 0x3E: /* PSRLW */ if (rd) for (int n = 0; n < 4; n++) set_lane_w(&st->gpr[rd], n, lane_w(st->gpr[rt], n) >> sa); break;
        case 0x3F: /* PSRAW */ if (rd) for (int n = 0; n < 4; n++) set_lane_w(&st->gpr[rd], n, (uint32_t)((int32_t)lane_w(st->gpr[rt], n) >> sa)); break;

        /* Round 64 (task #172 aside, 19 remaining real MMI opcodes):
         * MADD1/MADDU1/PMFHL/PMTHL, ported directly from PCSX2 v1.6.0's
         * pcsx2/MMI.cpp (fetched from
         * raw.githubusercontent.com/PCSX2/pcsx2/v1.6.0/pcsx2/MMI.cpp -
         * current PCSX2 master has moved this logic into x86 recompiler
         * emission and no longer carries a plain interpreter reference,
         * so the last version with clean C++ semantics was used
         * instead), cross-checked against the real sa/funct encoding in
         * the same tag's pcsx2/R5900OpcodeTables.cpp (tbl_MMI[64]/
         * tbl_MMI1[32]/tbl_MMI2[32]/tbl_MMI3[32]) rather than guessed -
         * this also corrected this project's own prior "~23 remaining"
         * estimate to the real, precise count of 19. */
        case 0x20: /* MADD1 - pipe-1 32x32->64 signed multiply-add,
                     * ported from MADD1(). Same 64-bit HI:LO accumulator
                     * convention as MADD (case 0x00 above) but using the
                     * SECOND pipe (.ud1) throughout - real hardware has
                     * two independent HI:LO pipes for exactly this kind
                     * of dual-issue MMI arithmetic. */
        {
            int64_t acc = (int64_t)((uint64_t)st->lo.ud1 & 0xFFFFFFFFu) | ((int64_t)(int32_t)(uint32_t)st->hi.ud1 << 32);
            int64_t res = acc + (int64_t)(int32_t)rs32 * (int64_t)(int32_t)rt32;
            st->lo.ud1 = sext32((uint32_t)(res & 0xFFFFFFFFu));
            st->hi.ud1 = sext32((uint32_t)(res >> 32));
            if (rd) GPR(rd) = st->lo.ud1;
        } break;
        case 0x21: /* MADDU1 - unsigned pipe-1 variant, ported from
                     * MADDU1(). */
        {
            uint64_t acc = ((uint64_t)(uint32_t)st->lo.ud1) | ((uint64_t)(uint32_t)st->hi.ud1 << 32);
            uint64_t res = acc + (uint64_t)rs32 * (uint64_t)rt32;
            st->lo.ud1 = sext32((uint32_t)(res & 0xFFFFFFFFu));
            st->hi.ud1 = sext32((uint32_t)(res >> 32));
            if (rd) GPR(rd) = st->lo.ud1;
        } break;
        case 0x30: /* PMFHL - move from HI:LO, sub-mode selected by the
                     * `sa` field (LW=0, UW=1, SLW=2, LH=3, SH=4), ported
                     * from PMFHL()/PMFHL_CLAMP(). SLW/SH saturate to a
                     * 32-bit/16-bit signed range respectively; LW/UW/LH
                     * are plain bit extraction with no saturation. */
            if (rd) {
                switch (sa) {
                case 0x00: /* LW */
                    set_lane_w(&st->gpr[rd], 0, (uint32_t)st->lo.ud0);
                    set_lane_w(&st->gpr[rd], 1, (uint32_t)st->hi.ud0);
                    set_lane_w(&st->gpr[rd], 2, (uint32_t)st->lo.ud1);
                    set_lane_w(&st->gpr[rd], 3, (uint32_t)st->hi.ud1);
                    break;
                case 0x01: /* UW */
                    set_lane_w(&st->gpr[rd], 0, (uint32_t)(st->lo.ud0 >> 32));
                    set_lane_w(&st->gpr[rd], 1, (uint32_t)(st->hi.ud0 >> 32));
                    set_lane_w(&st->gpr[rd], 2, (uint32_t)(st->lo.ud1 >> 32));
                    set_lane_w(&st->gpr[rd], 3, (uint32_t)(st->hi.ud1 >> 32));
                    break;
                case 0x02: /* SLW - saturate the 64-bit HI:LO pipe value
                             * to signed 32-bit range before truncating. */
                {
                    int64_t v0 = (int64_t)(((uint64_t)(uint32_t)st->hi.ud0 << 32) | (uint32_t)st->lo.ud0);
                    int64_t v1 = (int64_t)(((uint64_t)(uint32_t)st->hi.ud1 << 32) | (uint32_t)st->lo.ud1);
                    uint64_t r0, r1;
                    if (v0 >= 0x7fffffffLL) r0 = 0x7fffffffu;
                    else if (v0 <= -0x80000000LL) r0 = 0xffffffff80000000ULL;
                    else r0 = sext32((uint32_t)st->lo.ud0);
                    if (v1 >= 0x7fffffffLL) r1 = 0x7fffffffu;
                    else if (v1 <= -0x80000000LL) r1 = 0xffffffff80000000ULL;
                    else r1 = sext32((uint32_t)st->lo.ud1);
                    st->gpr[rd].ud0 = r0;
                    st->gpr[rd].ud1 = r1;
                } break;
                case 0x03: /* LH */
                    set_lane_h(&st->gpr[rd], 0, (uint16_t)st->lo.ud0);
                    set_lane_h(&st->gpr[rd], 1, (uint16_t)(st->lo.ud0 >> 32));
                    set_lane_h(&st->gpr[rd], 2, (uint16_t)st->hi.ud0);
                    set_lane_h(&st->gpr[rd], 3, (uint16_t)(st->hi.ud0 >> 32));
                    set_lane_h(&st->gpr[rd], 4, (uint16_t)st->lo.ud1);
                    set_lane_h(&st->gpr[rd], 5, (uint16_t)(st->lo.ud1 >> 32));
                    set_lane_h(&st->gpr[rd], 6, (uint16_t)st->hi.ud1);
                    set_lane_h(&st->gpr[rd], 7, (uint16_t)(st->hi.ud1 >> 32));
                    break;
                case 0x04: /* SH - saturate each 32-bit LO/HI lane to a
                             * signed 16-bit range, ported from
                             * PMFHL_CLAMP() (strict >/< bounds, unlike
                             * SLW's >=/<=). */
                {
                    int32_t vals[8];
                    vals[0] = (int32_t)(uint32_t)st->lo.ud0;       vals[1] = (int32_t)(uint32_t)(st->lo.ud0 >> 32);
                    vals[2] = (int32_t)(uint32_t)st->hi.ud0;       vals[3] = (int32_t)(uint32_t)(st->hi.ud0 >> 32);
                    vals[4] = (int32_t)(uint32_t)st->lo.ud1;       vals[5] = (int32_t)(uint32_t)(st->lo.ud1 >> 32);
                    vals[6] = (int32_t)(uint32_t)st->hi.ud1;       vals[7] = (int32_t)(uint32_t)(st->hi.ud1 >> 32);
                    for (int n = 0; n < 8; n++) {
                        uint16_t r;
                        if (vals[n] > 0x7fff) r = 0x7fffu;
                        else if (vals[n] < -0x8000) r = 0x8000u;
                        else r = (uint16_t)vals[n];
                        set_lane_h(&st->gpr[rd], n, r);
                    }
                } break;
                default: break; /* real hardware: other sa values are reserved/no-op, matching PMFHL()'s own switch with no default case */
                }
            }
            break;
        case 0x31: /* PMTHL - move to HI:LO, sa must be 0 (LW mode is
                     * the only one real hardware implements for the
                     * "to" direction). Ported from PMTHL(): note it
                     * only overwrites the LOW 32 bits of each 64-bit
                     * pipe half, leaving the upper 32 bits of .ud0/.ud1
                     * untouched - a genuine hardware quirk, not a bug,
                     * preserved exactly rather than "cleaned up". */
            if (sa == 0) {
                st->lo.ud0 = (st->lo.ud0 & 0xFFFFFFFF00000000ULL) | (uint64_t)lane_w(st->gpr[rs], 0);
                st->hi.ud0 = (st->hi.ud0 & 0xFFFFFFFF00000000ULL) | (uint64_t)lane_w(st->gpr[rs], 1);
                st->lo.ud1 = (st->lo.ud1 & 0xFFFFFFFF00000000ULL) | (uint64_t)lane_w(st->gpr[rs], 2);
                st->hi.ud1 = (st->hi.ud1 & 0xFFFFFFFF00000000ULL) | (uint64_t)lane_w(st->gpr[rs], 3);
            }
            break;

        case 0x08: /* MMI0 */
            switch (sa) {
            case 0x00: /* PADDW */ if (rd) for (int n = 0; n < 4; n++) set_lane_w(&st->gpr[rd], n, lane_w(st->gpr[rs], n) + lane_w(st->gpr[rt], n)); break;
            case 0x01: /* PSUBW */ if (rd) for (int n = 0; n < 4; n++) set_lane_w(&st->gpr[rd], n, lane_w(st->gpr[rs], n) - lane_w(st->gpr[rt], n)); break;
            case 0x04: /* PADDH */ if (rd) for (int n = 0; n < 8; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)(lane_h(st->gpr[rs], n) + lane_h(st->gpr[rt], n))); break;
            case 0x05: /* PSUBH */ if (rd) for (int n = 0; n < 8; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)(lane_h(st->gpr[rs], n) - lane_h(st->gpr[rt], n))); break;
            case 0x08: /* PADDB */ if (rd) for (int n = 0; n < 16; n++) set_lane_b(&st->gpr[rd], n, (uint8_t)(lane_b(st->gpr[rs], n) + lane_b(st->gpr[rt], n))); break;
            case 0x09: /* PSUBB */ if (rd) for (int n = 0; n < 16; n++) set_lane_b(&st->gpr[rd], n, (uint8_t)(lane_b(st->gpr[rs], n) - lane_b(st->gpr[rt], n))); break;
            case 0x02: /* PCGTW - per-lane signed 32-bit compare, ported
                        * from PCSX2's _PCGTW(): result is an all-1s
                        * (0xFFFFFFFF) or all-0s mask, not a boolean 0/1,
                        * matching real hardware's SIMD-compare convention. */
                if (rd) for (int n = 0; n < 4; n++)
                    set_lane_w(&st->gpr[rd], n, ((int32_t)lane_w(st->gpr[rs], n) > (int32_t)lane_w(st->gpr[rt], n)) ? 0xFFFFFFFFu : 0x00000000u);
                break;
            case 0x03: /* PMAXW - per-lane signed 32-bit max, ported from _PMAXW(). */
                if (rd) for (int n = 0; n < 4; n++) {
                    int32_t a = (int32_t)lane_w(st->gpr[rs], n), b = (int32_t)lane_w(st->gpr[rt], n);
                    set_lane_w(&st->gpr[rd], n, (uint32_t)((a > b) ? a : b));
                }
                break;
            case 0x06: /* PCGTH - per-lane signed 16-bit compare, ported from _PCGTH(). */
                if (rd) for (int n = 0; n < 8; n++)
                    set_lane_h(&st->gpr[rd], n, ((int16_t)lane_h(st->gpr[rs], n) > (int16_t)lane_h(st->gpr[rt], n)) ? 0xFFFFu : 0x0000u);
                break;
            case 0x07: /* PMAXH - per-lane signed 16-bit max, ported from _PMAXH(). */
                if (rd) for (int n = 0; n < 8; n++) {
                    int16_t a = (int16_t)lane_h(st->gpr[rs], n), b = (int16_t)lane_h(st->gpr[rt], n);
                    set_lane_h(&st->gpr[rd], n, (uint16_t)((a > b) ? a : b));
                }
                break;
            case 0x0A: /* PCGTB - per-lane signed 8-bit compare, ported from _PCGTB(). */
                if (rd) for (int n = 0; n < 16; n++)
                    set_lane_b(&st->gpr[rd], n, ((int8_t)lane_b(st->gpr[rs], n) > (int8_t)lane_b(st->gpr[rt], n)) ? 0xFFu : 0x00u);
                break;
            case 0x12: /* PEXTLW */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_w(&out, 0, lane_w(Rt, 0)); set_lane_w(&out, 1, lane_w(Rs, 0));
                    set_lane_w(&out, 2, lane_w(Rt, 1)); set_lane_w(&out, 3, lane_w(Rs, 1));
                    st->gpr[rd] = out;
                }
                break;
            case 0x13: /* PPACW */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_w(&out, 0, lane_w(Rt, 0)); set_lane_w(&out, 1, lane_w(Rt, 2));
                    set_lane_w(&out, 2, lane_w(Rs, 0)); set_lane_w(&out, 3, lane_w(Rs, 2));
                    st->gpr[rd] = out;
                }
                break;
            case 0x16: /* PEXTLH */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 0)); set_lane_h(&out, 1, lane_h(Rs, 0));
                    set_lane_h(&out, 2, lane_h(Rt, 1)); set_lane_h(&out, 3, lane_h(Rs, 1));
                    set_lane_h(&out, 4, lane_h(Rt, 2)); set_lane_h(&out, 5, lane_h(Rs, 2));
                    set_lane_h(&out, 6, lane_h(Rt, 3)); set_lane_h(&out, 7, lane_h(Rs, 3));
                    st->gpr[rd] = out;
                }
                break;
            case 0x17: /* PPACH */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 0)); set_lane_h(&out, 1, lane_h(Rt, 2));
                    set_lane_h(&out, 2, lane_h(Rt, 4)); set_lane_h(&out, 3, lane_h(Rt, 6));
                    set_lane_h(&out, 4, lane_h(Rs, 0)); set_lane_h(&out, 5, lane_h(Rs, 2));
                    set_lane_h(&out, 6, lane_h(Rs, 4)); set_lane_h(&out, 7, lane_h(Rs, 6));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1A: /* PEXTLB */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    for (int n = 0; n < 8; n++) {
                        set_lane_b(&out, n * 2,     lane_b(Rt, n));
                        set_lane_b(&out, n * 2 + 1, lane_b(Rs, n));
                    }
                    st->gpr[rd] = out;
                }
                break;
            case 0x1B: /* PPACB */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    for (int n = 0; n < 8; n++) {
                        set_lane_b(&out, n,     lane_b(Rt, n * 2));
                        set_lane_b(&out, n + 8, lane_b(Rs, n * 2));
                    }
                    st->gpr[rd] = out;
                }
                break;
            case 0x10: /* PADDSW - saturated per-lane signed 32-bit
                        * add, ported from PCSX2's _PADDSW(): computes
                        * the sum in 64-bit to detect over/underflow,
                        * then clamps to INT32_MAX/INT32_MIN instead
                        * of wrapping - the "S" in the name is for
                        * "saturated", not "signed" (all PMMI arithmetic
                        * here is signed either way). */
                if (rd) for (int n = 0; n < 4; n++) {
                    int64_t sum = (int64_t)(int32_t)lane_w(st->gpr[rs], n) + (int64_t)(int32_t)lane_w(st->gpr[rt], n);
                    uint32_t r;
                    if (sum > 0x7FFFFFFFLL) r = 0x7FFFFFFFu;
                    else if (sum < -0x80000000LL) r = 0x80000000u;
                    else r = (uint32_t)(int32_t)sum;
                    set_lane_w(&st->gpr[rd], n, r);
                }
                break;
            case 0x11: /* PSUBSW - saturated per-lane signed 32-bit
                        * subtract, ported from _PSUBSW(). */
                if (rd) for (int n = 0; n < 4; n++) {
                    int64_t diff = (int64_t)(int32_t)lane_w(st->gpr[rs], n) - (int64_t)(int32_t)lane_w(st->gpr[rt], n);
                    uint32_t r;
                    if (diff >= 0x7FFFFFFFLL) r = 0x7FFFFFFFu;
                    else if (diff < -0x80000000LL) r = 0x80000000u;
                    else r = (uint32_t)(int32_t)diff;
                    set_lane_w(&st->gpr[rd], n, r);
                }
                break;
            case 0x14: /* PADDSH - saturated per-lane signed 16-bit
                        * add, ported from _PADDSH(). */
                if (rd) for (int n = 0; n < 8; n++) {
                    int32_t sum = (int32_t)(int16_t)lane_h(st->gpr[rs], n) + (int32_t)(int16_t)lane_h(st->gpr[rt], n);
                    uint16_t r;
                    if (sum > 0x7FFF) r = 0x7FFFu;
                    else if (sum < -0x8000) r = 0x8000u;
                    else r = (uint16_t)(int16_t)sum;
                    set_lane_h(&st->gpr[rd], n, r);
                }
                break;
            case 0x15: /* PSUBSH - saturated per-lane signed 16-bit
                        * subtract, ported from _PSUBSH(). */
                if (rd) for (int n = 0; n < 8; n++) {
                    int32_t diff = (int32_t)(int16_t)lane_h(st->gpr[rs], n) - (int32_t)(int16_t)lane_h(st->gpr[rt], n);
                    uint16_t r;
                    if (diff >= 0x7FFF) r = 0x7FFFu;
                    else if (diff < -0x8000) r = 0x8000u;
                    else r = (uint16_t)(int16_t)diff;
                    set_lane_h(&st->gpr[rd], n, r);
                }
                break;
            case 0x18: /* PADDSB - saturated per-lane signed 8-bit
                        * add, ported from _PADDSB(). */
                if (rd) for (int n = 0; n < 16; n++) {
                    int16_t sum = (int16_t)(int8_t)lane_b(st->gpr[rs], n) + (int16_t)(int8_t)lane_b(st->gpr[rt], n);
                    uint8_t r;
                    if (sum > 0x7F) r = 0x7Fu;
                    else if (sum < -128) r = 0x80u;
                    else r = (uint8_t)(int8_t)sum;
                    set_lane_b(&st->gpr[rd], n, r);
                }
                break;
            case 0x19: /* PSUBSB - saturated per-lane signed 8-bit
                        * subtract, ported from _PSUBSB(). */
                if (rd) for (int n = 0; n < 16; n++) {
                    int16_t diff = (int16_t)(int8_t)lane_b(st->gpr[rs], n) - (int16_t)(int8_t)lane_b(st->gpr[rt], n);
                    uint8_t r;
                    if (diff >= 0x7F) r = 0x7Fu;
                    else if (diff < -128) r = 0x80u;
                    else r = (uint8_t)(int8_t)diff;
                    set_lane_b(&st->gpr[rd], n, r);
                }
                break;
            case 0x1E: /* PEXT5 - unpacks a GS 16-bit 5551 pixel format
                        * (5 bits R, 5 bits G, 5 bits B, 1 bit A, packed
                        * in the low 16 bits of each 32-bit lane) up
                        * into a 32-bit lane with each channel
                        * left-aligned in its own byte (R in bits 3-7,
                        * G in bits 11-15, B in bits 19-23, A in bit
                        * 31) - ported exactly from PCSX2's _PEXT5().
                        * Uses only Rt; Rs is unused (matches real
                        * hardware/PCSX2 - this is a unary unpack, not
                        * a binary op). */
                if (rd) for (int n = 0; n < 4; n++) {
                    uint32_t v = lane_w(st->gpr[rt], n);
                    uint32_t r = ((v & 0x0000001Fu) << 3)  |
                                 ((v & 0x000003E0u) << 6)  |
                                 ((v & 0x00007C00u) << 9)  |
                                 ((v & 0x00008000u) << 16);
                    set_lane_w(&st->gpr[rd], n, r);
                }
                break;
            case 0x1F: /* PPAC5 - inverse of PEXT5: packs a 32-bit lane
                        * (as produced by PEXT5's layout) back down to
                        * a 16-bit 5551 pixel. Ported from _PPAC5().
                        * Also Rt-only, Rs unused. */
                if (rd) for (int n = 0; n < 4; n++) {
                    uint32_t v = lane_w(st->gpr[rt], n);
                    uint32_t r = ((v >> 3)  & 0x0000001Fu) |
                                 ((v >> 6)  & 0x000003E0u) |
                                 ((v >> 9)  & 0x00007C00u) |
                                 ((v >> 16) & 0x00008000u);
                    set_lane_w(&st->gpr[rd], n, r);
                }
                break;
            default:
                ee_core_raise_reserved(st);
                break;
            }
            break;

        case 0x28: /* MMI1 */
            switch (sa) {
            case 0x01: /* PABSW - per-lane 32-bit absolute value, ported
                        * from PCSX2's _PABSW(): real hardware quirk
                        * preserved exactly - INT32_MIN (0x80000000) has
                        * no positive 32-bit representation, so it's
                        * clamped to INT32_MAX (0x7FFFFFFF) instead of
                        * overflowing/wrapping. */
                if (rd) for (int n = 0; n < 4; n++) {
                    uint32_t v = lane_w(st->gpr[rt], n);
                    uint32_t r;
                    if (v == 0x80000000u) r = 0x7FFFFFFFu;
                    else if ((int32_t)v < 0) r = (uint32_t)(-(int32_t)v);
                    else r = v;
                    set_lane_w(&st->gpr[rd], n, r);
                }
                break;
            case 0x02: /* PCEQW - per-lane 32-bit equality compare (mask
                        * result), ported from _PCEQW(). */
                if (rd) for (int n = 0; n < 4; n++)
                    set_lane_w(&st->gpr[rd], n, (lane_w(st->gpr[rs], n) == lane_w(st->gpr[rt], n)) ? 0xFFFFFFFFu : 0x00000000u);
                break;
            case 0x03: /* PMINW - per-lane signed 32-bit min, ported from _PMINW(). */
                if (rd) for (int n = 0; n < 4; n++) {
                    int32_t a = (int32_t)lane_w(st->gpr[rs], n), b = (int32_t)lane_w(st->gpr[rt], n);
                    set_lane_w(&st->gpr[rd], n, (uint32_t)((a < b) ? a : b));
                }
                break;
            case 0x04: /* PADSBH - "add/subtract halfword", ported from
                        * PCSX2's PADSBH(): NOT a uniform op across all 8
                        * lanes - the low 4 lanes get PSUBH (fs-ft), the
                        * high 4 lanes get PADDH (fs+ft). A real,
                        * deliberately asymmetric instruction, not a typo. */
                if (rd) {
                    for (int n = 0; n < 4; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)(lane_h(st->gpr[rs], n) - lane_h(st->gpr[rt], n)));
                    for (int n = 4; n < 8; n++) set_lane_h(&st->gpr[rd], n, (uint16_t)(lane_h(st->gpr[rs], n) + lane_h(st->gpr[rt], n)));
                }
                break;
            case 0x05: /* PABSH - per-lane 16-bit absolute value, same
                        * INT16_MIN-clamps-to-INT16_MAX quirk as PABSW,
                        * ported from _PABSH(). */
                if (rd) for (int n = 0; n < 8; n++) {
                    uint16_t v = lane_h(st->gpr[rt], n);
                    uint16_t r;
                    if (v == 0x8000u) r = 0x7FFFu;
                    else if ((int16_t)v < 0) r = (uint16_t)(-(int16_t)v);
                    else r = v;
                    set_lane_h(&st->gpr[rd], n, r);
                }
                break;
            case 0x06: /* PCEQH - per-lane 16-bit equality compare, ported from _PCEQH(). */
                if (rd) for (int n = 0; n < 8; n++)
                    set_lane_h(&st->gpr[rd], n, (lane_h(st->gpr[rs], n) == lane_h(st->gpr[rt], n)) ? 0xFFFFu : 0x0000u);
                break;
            case 0x07: /* PMINH - per-lane signed 16-bit min, ported from _PMINH(). */
                if (rd) for (int n = 0; n < 8; n++) {
                    int16_t a = (int16_t)lane_h(st->gpr[rs], n), b = (int16_t)lane_h(st->gpr[rt], n);
                    set_lane_h(&st->gpr[rd], n, (uint16_t)((a < b) ? a : b));
                }
                break;
            case 0x0A: /* PCEQB - per-lane 8-bit equality compare, ported from _PCEQB(). */
                if (rd) for (int n = 0; n < 16; n++)
                    set_lane_b(&st->gpr[rd], n, (lane_b(st->gpr[rs], n) == lane_b(st->gpr[rt], n)) ? 0xFFu : 0x00u);
                break;
            case 0x10: /* PADDUW: unsigned saturation, primary MMI.cpp _PADDUW. */ if (rd) for (int n = 0; n < 4; n++) { uint64_t sum=(uint64_t)lane_w(st->gpr[rs],n)+lane_w(st->gpr[rt],n);set_lane_w(&st->gpr[rd],n,sum>UINT32_MAX?UINT32_MAX:(uint32_t)sum); } break;
            case 0x11: /* PSUBUW */ if (rd) for (int n = 0; n < 4; n++) { uint32_t a = lane_w(st->gpr[rs], n), b = lane_w(st->gpr[rt], n); set_lane_w(&st->gpr[rd], n, (a > b) ? a - b : 0); } break;
            case 0x12: /* PEXTUW */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_w(&out, 0, lane_w(Rt, 2)); set_lane_w(&out, 1, lane_w(Rs, 2));
                    set_lane_w(&out, 2, lane_w(Rt, 3)); set_lane_w(&out, 3, lane_w(Rs, 3));
                    st->gpr[rd] = out;
                }
                break;
            case 0x14: /* PADDUH */ if (rd) for (int n = 0; n < 8; n++) { uint32_t sum=(uint32_t)lane_h(st->gpr[rs],n)+lane_h(st->gpr[rt],n);set_lane_h(&st->gpr[rd],n,sum>UINT16_MAX?UINT16_MAX:(uint16_t)sum); } break;
            case 0x15: /* PSUBUH */ if (rd) for (int n = 0; n < 8; n++) { uint16_t a = lane_h(st->gpr[rs], n), b = lane_h(st->gpr[rt], n); set_lane_h(&st->gpr[rd], n, (a > b) ? (uint16_t)(a - b) : 0); } break;
            case 0x16: /* PEXTUH */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 4)); set_lane_h(&out, 1, lane_h(Rs, 4));
                    set_lane_h(&out, 2, lane_h(Rt, 5)); set_lane_h(&out, 3, lane_h(Rs, 5));
                    set_lane_h(&out, 4, lane_h(Rt, 6)); set_lane_h(&out, 5, lane_h(Rs, 6));
                    set_lane_h(&out, 6, lane_h(Rt, 7)); set_lane_h(&out, 7, lane_h(Rs, 7));
                    st->gpr[rd] = out;
                }
                break;
            case 0x18: /* PADDUB */ if (rd) for (int n = 0; n < 16; n++) { uint32_t sum=(uint32_t)lane_b(st->gpr[rs],n)+lane_b(st->gpr[rt],n);set_lane_b(&st->gpr[rd],n,sum>UINT8_MAX?UINT8_MAX:(uint8_t)sum); } break;
            case 0x19: /* PSUBUB */ if (rd) for (int n = 0; n < 16; n++) { uint8_t a = lane_b(st->gpr[rs], n), b = lane_b(st->gpr[rt], n); set_lane_b(&st->gpr[rd], n, (a > b) ? (uint8_t)(a - b) : 0); } break;
            case 0x1A: /* PEXTUB */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    for (int n = 0; n < 8; n++) {
                        set_lane_b(&out, n * 2,     lane_b(Rt, n + 8));
                        set_lane_b(&out, n * 2 + 1, lane_b(Rs, n + 8));
                    }
                    st->gpr[rd] = out;
                }
                break;
            case 0x1B: /* QFSRV - "quadword funnel shift right variable":
                        * treats {Rs:Rt} as a 256-bit value (Rs in the
                        * upper 128 bits, Rt in the lower 128), shifts it
                        * right by a BYTE count taken from the dedicated
                        * SA register (task #177's MFSA/MTSA - NOT the
                        * instruction's own `sa` field bits, a real and
                        * easy-to-miss distinction), and keeps the low
                        * 128 bits as Rd. Ported from QFSRV(); sa_amt==0
                        * is a pure copy of Rt, sa_amt<64 and >=64 are
                        * separate cases because a plain C shift by >=64
                        * bits on a 64-bit value is undefined behavior. */
                if (rd) {
                    uint32_t sa_amt = (st->sa_reg & 0xFu) << 3;
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    if (sa_amt == 0) {
                        out = Rt;
                    } else if (sa_amt < 64) {
                        out.ud0 = (Rt.ud0 >> sa_amt) | (Rt.ud1 << (64 - sa_amt));
                        out.ud1 = (Rt.ud1 >> sa_amt) | (Rs.ud0 << (64 - sa_amt));
                    } else {
                        uint32_t sh = sa_amt - 64u;
                        out.ud0 = Rt.ud1 >> sh;
                        out.ud1 = Rs.ud0 >> sh;
                        if (sa_amt != 64u) {
                            out.ud0 |= Rs.ud0 << (128u - sa_amt);
                            out.ud1 |= Rs.ud1 << (128u - sa_amt);
                        }
                    }
                    st->gpr[rd] = out;
                }
                break;
            default:
                ee_core_raise_reserved(st);
                break;
            }
            break;

        case 0x09: /* MMI2 */
            switch (sa) {
            case 0x08: /* PMFHI */ if (rd) st->gpr[rd] = st->hi; break;
            case 0x09: /* PMFLO */ if (rd) st->gpr[rd] = st->lo; break;
            case 0x0E: /* PCPYLD */
                if (rd) {
                    ee_reg128_t out;
                    out.ud1 = st->gpr[rs].ud0;
                    out.ud0 = st->gpr[rt].ud0;
                    st->gpr[rd] = out;
                }
                break;
            case 0x12: /* PAND */ if (rd) { st->gpr[rd].ud0 = st->gpr[rs].ud0 & st->gpr[rt].ud0; st->gpr[rd].ud1 = st->gpr[rs].ud1 & st->gpr[rt].ud1; } break;
            case 0x13: /* PXOR */ if (rd) { st->gpr[rd].ud0 = st->gpr[rs].ud0 ^ st->gpr[rt].ud0; st->gpr[rd].ud1 = st->gpr[rs].ud1 ^ st->gpr[rt].ud1; } break;
            case 0x02: /* PSLLVW - variable logical-left-shift of two
                        * word lanes (Rt's lanes 0/2), each shifted by
                        * its OWN shift amount taken from the
                        * corresponding lane of Rs (lane 0's amount
                        * from Rs lane 0, lane 2's amount from Rs lane
                        * 2 - masked to 5 bits, standard MIPS variable-
                        * shift convention), sign-extended to 64 bits
                        * into gpr.ud0/ud1. Ported from PCSX2's
                        * PSLLVW(). */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt];
                    st->gpr[rd].ud0 = sext32(lane_w(Rt, 0) << (lane_w(Rs, 0) & 0x1F));
                    st->gpr[rd].ud1 = sext32(lane_w(Rt, 2) << (lane_w(Rs, 2) & 0x1F));
                }
                break;
            case 0x03: /* PSRLVW - same as PSLLVW but a variable
                        * LOGICAL right shift (not arithmetic), ported
                        * from PCSX2's PSRLVW(). */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt];
                    st->gpr[rd].ud0 = sext32(lane_w(Rt, 0) >> (lane_w(Rs, 0) & 0x1F));
                    st->gpr[rd].ud1 = sext32(lane_w(Rt, 2) >> (lane_w(Rs, 2) & 0x1F));
                }
                break;
            case 0x0A: /* PINTH - interleaves the low halfword lanes of
                        * Rt with the HIGH halfword lanes of Rs, ported
                        * from PCSX2's PINTH(). Note it's Rs's UPPER 4
                        * lanes (US[4..7]) that get interleaved in, not
                        * the lower ones - a real, easy-to-get-backwards
                        * detail worth preserving exactly. */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 0)); set_lane_h(&out, 1, lane_h(Rs, 4));
                    set_lane_h(&out, 2, lane_h(Rt, 1)); set_lane_h(&out, 3, lane_h(Rs, 5));
                    set_lane_h(&out, 4, lane_h(Rt, 2)); set_lane_h(&out, 5, lane_h(Rs, 6));
                    set_lane_h(&out, 6, lane_h(Rt, 3)); set_lane_h(&out, 7, lane_h(Rs, 7));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1A: /* PEXEH - swaps halfword lanes 0 and 2 within
                        * each 64-bit half (Rt only; Rs unused), ported
                        * from PEXEH(). */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 2)); set_lane_h(&out, 1, lane_h(Rt, 1));
                    set_lane_h(&out, 2, lane_h(Rt, 0)); set_lane_h(&out, 3, lane_h(Rt, 3));
                    set_lane_h(&out, 4, lane_h(Rt, 6)); set_lane_h(&out, 5, lane_h(Rt, 5));
                    set_lane_h(&out, 6, lane_h(Rt, 4)); set_lane_h(&out, 7, lane_h(Rt, 7));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1B: /* PREVH - fully reverses the halfword lanes
                        * within each 64-bit half (Rt only), ported
                        * from PREVH(). */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 3)); set_lane_h(&out, 1, lane_h(Rt, 2));
                    set_lane_h(&out, 2, lane_h(Rt, 1)); set_lane_h(&out, 3, lane_h(Rt, 0));
                    set_lane_h(&out, 4, lane_h(Rt, 7)); set_lane_h(&out, 5, lane_h(Rt, 6));
                    set_lane_h(&out, 6, lane_h(Rt, 5)); set_lane_h(&out, 7, lane_h(Rt, 4));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1E: /* PEXEW - swaps word lanes 0 and 2 (Rt only),
                        * ported from PEXEW(). */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    set_lane_w(&out, 0, lane_w(Rt, 2)); set_lane_w(&out, 1, lane_w(Rt, 1));
                    set_lane_w(&out, 2, lane_w(Rt, 0)); set_lane_w(&out, 3, lane_w(Rt, 3));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1F: /* PROT3W - rotates word lanes 0,1,2 left by one
                        * (lane 3 untouched), Rt only. Ported from
                        * PROT3W(). */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    set_lane_w(&out, 0, lane_w(Rt, 1)); set_lane_w(&out, 1, lane_w(Rt, 2));
                    set_lane_w(&out, 2, lane_w(Rt, 0)); set_lane_w(&out, 3, lane_w(Rt, 3));
                    st->gpr[rd] = out;
                }
                break;
            case 0x00: /* PMADDW - pipe-paired 32x32->64 signed
                        * multiply-add (ss=0/2 select Rs/Rt's lane 0 or
                        * 2; dd=0/1 select which HI:LO pipe half gets
                        * the result), ported from _PMADDW()/PMADDW().
                        * Real hardware "division voodoo" quirk
                        * preserved verbatim: for the LOW pipe only
                        * (ss==0), if Rt's lane is 0 or INT32_MAX (its
                        * low 31 bits are all-0 or all-1) AND Rs!=Rt, a
                        * fixed correction constant (0x70000000) is
                        * added before the final divide-by-2^32-1 step -
                        * an actual, cited real-hardware rounding
                        * anomaly, not a simplification. */
            {
                for (int k = 0; k < 2; k++) {
                    int ss = (k == 0) ? 0 : 2;
                    int64_t temp = (int64_t)(int32_t)lane_w(st->gpr[rs], ss) * (int64_t)(int32_t)lane_w(st->gpr[rt], ss);
                    uint64_t bits = (uint64_t)temp + ((uint64_t)(uint32_t)(k == 0 ? st->hi.ud0 : st->hi.ud1) << 32);
                    if (ss == 0) {
                        int32_t rtl = (int32_t)lane_w(st->gpr[rt], ss);
                        if (((rtl & 0x7FFFFFFF) == 0 || (rtl & 0x7FFFFFFF) == 0x7FFFFFFF) &&
                            (int32_t)lane_w(st->gpr[rs], ss) != rtl)
                            bits += 0x70000000u;
                    }
                    int32_t hi_new = (int32_t)((int64_t)bits / 4294967295LL);
                    uint32_t lo_new = (uint32_t)temp + (uint32_t)(k == 0 ? st->lo.ud0 : st->lo.ud1);
                    if (k == 0) { st->lo.ud0 = sext32((uint32_t)lo_new); st->hi.ud0 = sext32((uint32_t)hi_new); if (rd) GPR(rd) = ((uint64_t)(uint32_t)hi_new << 32) | lo_new; }
                    else        { st->lo.ud1 = sext32((uint32_t)lo_new); st->hi.ud1 = sext32((uint32_t)hi_new); if (rd) GPR1(rd) = ((uint64_t)(uint32_t)hi_new << 32) | lo_new; }
                }
            } break;
            case 0x04: /* PMSUBW - pipe-paired 32x32->64 signed
                        * multiply-subtract, ported from
                        * _PMSUBW()/PMSUBW(). Same off-by-one
                        * divide-by-2^32-1 rounding as PMADDW but no
                        * "division voodoo" correction term (real
                        * hardware only applies that quirk to PMADDW). */
            {
                for (int k = 0; k < 2; k++) {
                    int ss = (k == 0) ? 0 : 2;
                    int64_t temp = (int64_t)(int32_t)lane_w(st->gpr[rs], ss) * (int64_t)(int32_t)lane_w(st->gpr[rt], ss);
                    uint64_t bits = ((uint64_t)(uint32_t)(k == 0 ? st->hi.ud0 : st->hi.ud1) << 32) - (uint64_t)temp;
                    int32_t hi_new = (int32_t)((int64_t)bits / 4294967295LL);
                    uint32_t lo_new = (uint32_t)(k == 0 ? st->lo.ud0 : st->lo.ud1) - (uint32_t)temp;
                    if (k == 0) { st->lo.ud0 = sext32((uint32_t)lo_new); st->hi.ud0 = sext32((uint32_t)hi_new); if (rd) GPR(rd) = ((uint64_t)(uint32_t)hi_new << 32) | lo_new; }
                    else        { st->lo.ud1 = sext32((uint32_t)lo_new); st->hi.ud1 = sext32((uint32_t)hi_new); if (rd) GPR1(rd) = ((uint64_t)(uint32_t)hi_new << 32) | lo_new; }
                }
            } break;
            case 0x0C: /* PMULTW - pipe-paired 32x32->64 signed
                        * multiply (no accumulate), ported from
                        * _PMULTW()/PMULTW(). */
            {
                int64_t t0 = (int64_t)(int32_t)lane_w(st->gpr[rs], 0) * (int64_t)(int32_t)lane_w(st->gpr[rt], 0);
                int64_t t1 = (int64_t)(int32_t)lane_w(st->gpr[rs], 2) * (int64_t)(int32_t)lane_w(st->gpr[rt], 2);
                st->lo.ud0 = (uint64_t)(int64_t)(int32_t)(t0 & 0xFFFFFFFFu); st->hi.ud0 = (uint64_t)(t0 >> 32);
                st->lo.ud1 = (uint64_t)(int64_t)(int32_t)(t1 & 0xFFFFFFFFu); st->hi.ud1 = (uint64_t)(t1 >> 32);
                if (rd) { st->gpr[rd].ud0 = (uint64_t)t0; st->gpr[rd].ud1 = (uint64_t)t1; }
            } break;
            case 0x0D: /* PDIVW - pipe-paired 32-bit signed divide,
                        * ported from _PDIVW()/PDIVW(). Real hardware
                        * special-cases INT32_MIN/-1 (would overflow a
                        * plain division) and division-by-zero (real
                        * MIPS-style sign-of-dividend convention),
                        * unlike this project's existing simpler
                        * DIV/DIV1 which silently no-op on rt==0. */
            {
                for (int k = 0; k < 2; k++) {
                    int ss = (k == 0) ? 0 : 2;
                    uint32_t rsv = lane_w(st->gpr[rs], ss), rtv = lane_w(st->gpr[rt], ss);
                    int64_t qlo, qhi;
                    if (rsv == 0x80000000u && rtv == 0xFFFFFFFFu) { qlo = (int32_t)0x80000000; qhi = 0; }
                    else if ((int32_t)rtv != 0) { qlo = (int32_t)rsv / (int32_t)rtv; qhi = (int32_t)rsv % (int32_t)rtv; }
                    else { qlo = ((int32_t)rsv < 0) ? 1 : -1; qhi = (int32_t)rsv; }
                    if (k == 0) { st->lo.ud0 = sext32((uint32_t)qlo); st->hi.ud0 = sext32((uint32_t)qhi); }
                    else        { st->lo.ud1 = sext32((uint32_t)qlo); st->hi.ud1 = sext32((uint32_t)qhi); }
                }
            } break;
            case 0x10: /* PMADDH - 8-way 16x16->32 signed
                        * multiply-add across all 8 halfword lanes,
                        * ported from PMADDH(). Unlike PMADDW's
                        * dd/ss-paired 64-bit halves, PMADDH addresses
                        * all four 32-bit LO/HI lanes directly. */
            {
                uint32_t r[4];
                r[0] = (uint32_t)st->lo.ud0        + (int32_t)(int16_t)lane_h(st->gpr[rs], 0) * (int32_t)(int16_t)lane_h(st->gpr[rt], 0);
                uint32_t r_lo1 = (uint32_t)(st->lo.ud0 >> 32) + (int32_t)(int16_t)lane_h(st->gpr[rs], 1) * (int32_t)(int16_t)lane_h(st->gpr[rt], 1);
                uint32_t r_hi0 = (uint32_t)st->hi.ud0        + (int32_t)(int16_t)lane_h(st->gpr[rs], 2) * (int32_t)(int16_t)lane_h(st->gpr[rt], 2);
                uint32_t r_hi1 = (uint32_t)(st->hi.ud0 >> 32) + (int32_t)(int16_t)lane_h(st->gpr[rs], 3) * (int32_t)(int16_t)lane_h(st->gpr[rt], 3);
                uint32_t r2_lo0 = (uint32_t)st->lo.ud1        + (int32_t)(int16_t)lane_h(st->gpr[rs], 4) * (int32_t)(int16_t)lane_h(st->gpr[rt], 4);
                uint32_t r2_lo1 = (uint32_t)(st->lo.ud1 >> 32) + (int32_t)(int16_t)lane_h(st->gpr[rs], 5) * (int32_t)(int16_t)lane_h(st->gpr[rt], 5);
                uint32_t r2_hi0 = (uint32_t)st->hi.ud1        + (int32_t)(int16_t)lane_h(st->gpr[rs], 6) * (int32_t)(int16_t)lane_h(st->gpr[rt], 6);
                uint32_t r2_hi1 = (uint32_t)(st->hi.ud1 >> 32) + (int32_t)(int16_t)lane_h(st->gpr[rs], 7) * (int32_t)(int16_t)lane_h(st->gpr[rt], 7);
                st->lo.ud0 = ((uint64_t)(uint32_t)r_lo1 << 32) | (uint32_t)r[0];
                st->hi.ud0 = ((uint64_t)(uint32_t)r_hi1 << 32) | (uint32_t)r_hi0;
                st->lo.ud1 = ((uint64_t)(uint32_t)r2_lo1 << 32) | (uint32_t)r2_lo0;
                st->hi.ud1 = ((uint64_t)(uint32_t)r2_hi1 << 32) | (uint32_t)r2_hi0;
                if (rd) {
                    set_lane_w(&st->gpr[rd], 0, (uint32_t)r[0]); set_lane_w(&st->gpr[rd], 1, (uint32_t)r_hi0);
                    set_lane_w(&st->gpr[rd], 2, (uint32_t)r2_lo0); set_lane_w(&st->gpr[rd], 3, (uint32_t)r2_hi0);
                }
            } break;
            case 0x11: /* PHMADH - "horizontal" multiply-add: pairs
                        * ADJACENT halfword lanes (n, n+1) instead of
                        * matching lane indices, ported from
                        * _PHMADH_LO()/_PHMADH_HI()/PHMADH(). */
            {
                uint32_t lo_words[2], hi_words[2];
                for (int half = 0; half < 2; half++) {
                    int base = half * 4;
                    int32_t first_lo = (int32_t)(int16_t)lane_h(st->gpr[rs], base + 1) * (int32_t)(int16_t)lane_h(st->gpr[rt], base + 1);
                    uint32_t sum_lo = (uint32_t)first_lo + (int32_t)(int16_t)lane_h(st->gpr[rs], base) * (int32_t)(int16_t)lane_h(st->gpr[rt], base);
                    int32_t first_hi = (int32_t)(int16_t)lane_h(st->gpr[rs], base + 3) * (int32_t)(int16_t)lane_h(st->gpr[rt], base + 3);
                    uint32_t sum_hi = (uint32_t)first_hi + (int32_t)(int16_t)lane_h(st->gpr[rs], base + 2) * (int32_t)(int16_t)lane_h(st->gpr[rt], base + 2);
                    lo_words[half] = (uint32_t)sum_lo; hi_words[half] = (uint32_t)sum_hi;
                    if (half == 0) { st->lo.ud0 = (uint64_t)(uint32_t)first_lo << 32 | (uint32_t)sum_lo; st->hi.ud0 = (uint64_t)(uint32_t)first_hi << 32 | (uint32_t)sum_hi; }
                    else           { st->lo.ud1 = (uint64_t)(uint32_t)first_lo << 32 | (uint32_t)sum_lo; st->hi.ud1 = (uint64_t)(uint32_t)first_hi << 32 | (uint32_t)sum_hi; }
                }
                if (rd) {
                    set_lane_w(&st->gpr[rd], 0, lo_words[0]); set_lane_w(&st->gpr[rd], 1, hi_words[0]);
                    set_lane_w(&st->gpr[rd], 2, lo_words[1]); set_lane_w(&st->gpr[rd], 3, hi_words[1]);
                }
            } break;
            case 0x14: /* PMSUBH - like PMADDH but subtracting, ported
                        * from PMSUBH(). */
            {
                uint32_t r0 = (uint32_t)st->lo.ud0        - (int32_t)(int16_t)lane_h(st->gpr[rs], 0) * (int32_t)(int16_t)lane_h(st->gpr[rt], 0);
                uint32_t r1 = (uint32_t)(st->lo.ud0 >> 32) - (int32_t)(int16_t)lane_h(st->gpr[rs], 1) * (int32_t)(int16_t)lane_h(st->gpr[rt], 1);
                uint32_t r2 = (uint32_t)st->hi.ud0        - (int32_t)(int16_t)lane_h(st->gpr[rs], 2) * (int32_t)(int16_t)lane_h(st->gpr[rt], 2);
                uint32_t r3 = (uint32_t)(st->hi.ud0 >> 32) - (int32_t)(int16_t)lane_h(st->gpr[rs], 3) * (int32_t)(int16_t)lane_h(st->gpr[rt], 3);
                uint32_t r4 = (uint32_t)st->lo.ud1        - (int32_t)(int16_t)lane_h(st->gpr[rs], 4) * (int32_t)(int16_t)lane_h(st->gpr[rt], 4);
                uint32_t r5 = (uint32_t)(st->lo.ud1 >> 32) - (int32_t)(int16_t)lane_h(st->gpr[rs], 5) * (int32_t)(int16_t)lane_h(st->gpr[rt], 5);
                uint32_t r6 = (uint32_t)st->hi.ud1        - (int32_t)(int16_t)lane_h(st->gpr[rs], 6) * (int32_t)(int16_t)lane_h(st->gpr[rt], 6);
                uint32_t r7 = (uint32_t)(st->hi.ud1 >> 32) - (int32_t)(int16_t)lane_h(st->gpr[rs], 7) * (int32_t)(int16_t)lane_h(st->gpr[rt], 7);
                st->lo.ud0 = ((uint64_t)(uint32_t)r1 << 32) | (uint32_t)r0;
                st->hi.ud0 = ((uint64_t)(uint32_t)r3 << 32) | (uint32_t)r2;
                st->lo.ud1 = ((uint64_t)(uint32_t)r5 << 32) | (uint32_t)r4;
                st->hi.ud1 = ((uint64_t)(uint32_t)r7 << 32) | (uint32_t)r6;
                if (rd) { set_lane_w(&st->gpr[rd], 0, (uint32_t)r0); set_lane_w(&st->gpr[rd], 1, (uint32_t)r2);
                          set_lane_w(&st->gpr[rd], 2, (uint32_t)r4); set_lane_w(&st->gpr[rd], 3, (uint32_t)r6); }
            } break;
            case 0x15: /* PHMSBH - horizontal multiply-subtract; the
                        * "first" (odd-lane) product's COMPLEMENT is
                        * stored in the adjacent LO/HI word - a real,
                        * documented-as-"undocumented behaviour" hardware
                        * quirk in PCSX2's own source, preserved exactly
                        * (~firsttemp, not a sign-flip or fabrication).
                        * Ported from _PHMSBH_LO()/_PHMSBH_HI()/
                        * PHMSBH(). */
            {
                uint32_t lo_words[2], hi_words[2];
                for (int half = 0; half < 2; half++) {
                    int base = half * 4;
                    int32_t first_lo = (int32_t)(int16_t)lane_h(st->gpr[rs], base + 1) * (int32_t)(int16_t)lane_h(st->gpr[rt], base + 1);
                    uint32_t diff_lo = (uint32_t)first_lo - (int32_t)(int16_t)lane_h(st->gpr[rs], base) * (int32_t)(int16_t)lane_h(st->gpr[rt], base);
                    int32_t first_hi = (int32_t)(int16_t)lane_h(st->gpr[rs], base + 3) * (int32_t)(int16_t)lane_h(st->gpr[rt], base + 3);
                    uint32_t diff_hi = (uint32_t)first_hi - (int32_t)(int16_t)lane_h(st->gpr[rs], base + 2) * (int32_t)(int16_t)lane_h(st->gpr[rt], base + 2);
                    lo_words[half] = (uint32_t)diff_lo; hi_words[half] = (uint32_t)diff_hi;
                    if (half == 0) { st->lo.ud0 = (uint64_t)(uint32_t)(~first_lo) << 32 | (uint32_t)diff_lo; st->hi.ud0 = (uint64_t)(uint32_t)(~first_hi) << 32 | (uint32_t)diff_hi; }
                    else           { st->lo.ud1 = (uint64_t)(uint32_t)(~first_lo) << 32 | (uint32_t)diff_lo; st->hi.ud1 = (uint64_t)(uint32_t)(~first_hi) << 32 | (uint32_t)diff_hi; }
                }
                if (rd) {
                    set_lane_w(&st->gpr[rd], 0, lo_words[0]); set_lane_w(&st->gpr[rd], 1, hi_words[0]);
                    set_lane_w(&st->gpr[rd], 2, lo_words[1]); set_lane_w(&st->gpr[rd], 3, hi_words[1]);
                }
            } break;
            case 0x1C: /* PMULTH - 8-way 16x16->32 signed multiply (no
                        * accumulate), ported from PMULTH(). */
            {
                int32_t r0 = (int32_t)(int16_t)lane_h(st->gpr[rs], 0) * (int32_t)(int16_t)lane_h(st->gpr[rt], 0);
                int32_t r1 = (int32_t)(int16_t)lane_h(st->gpr[rs], 1) * (int32_t)(int16_t)lane_h(st->gpr[rt], 1);
                int32_t r2 = (int32_t)(int16_t)lane_h(st->gpr[rs], 2) * (int32_t)(int16_t)lane_h(st->gpr[rt], 2);
                int32_t r3 = (int32_t)(int16_t)lane_h(st->gpr[rs], 3) * (int32_t)(int16_t)lane_h(st->gpr[rt], 3);
                int32_t r4 = (int32_t)(int16_t)lane_h(st->gpr[rs], 4) * (int32_t)(int16_t)lane_h(st->gpr[rt], 4);
                int32_t r5 = (int32_t)(int16_t)lane_h(st->gpr[rs], 5) * (int32_t)(int16_t)lane_h(st->gpr[rt], 5);
                int32_t r6 = (int32_t)(int16_t)lane_h(st->gpr[rs], 6) * (int32_t)(int16_t)lane_h(st->gpr[rt], 6);
                int32_t r7 = (int32_t)(int16_t)lane_h(st->gpr[rs], 7) * (int32_t)(int16_t)lane_h(st->gpr[rt], 7);
                st->lo.ud0 = ((uint64_t)(uint32_t)r1 << 32) | (uint32_t)r0;
                st->hi.ud0 = ((uint64_t)(uint32_t)r3 << 32) | (uint32_t)r2;
                st->lo.ud1 = ((uint64_t)(uint32_t)r5 << 32) | (uint32_t)r4;
                st->hi.ud1 = ((uint64_t)(uint32_t)r7 << 32) | (uint32_t)r6;
                if (rd) { set_lane_w(&st->gpr[rd], 0, (uint32_t)r0); set_lane_w(&st->gpr[rd], 1, (uint32_t)r2);
                          set_lane_w(&st->gpr[rd], 2, (uint32_t)r4); set_lane_w(&st->gpr[rd], 3, (uint32_t)r6); }
            } break;
            case 0x1D: /* PDIVBW - 4-way 32/16-bit signed divide: all
                        * FOUR of Rs's 32-bit lanes are divided by the
                        * SAME divisor - Rt's halfword lane 0 broadcast
                        * to every iteration (a real, cited hardware
                        * quirk, not a copy-paste bug), ported from
                        * _PDIVBW()/PDIVBW(). */
            {
                for (int n = 0; n < 4; n++) {
                    uint32_t rsv = lane_w(st->gpr[rs], n);
                    uint16_t rtv16 = lane_h(st->gpr[rt], 0);
                    int32_t qlo, qhi;
                    if (rsv == 0x80000000u && rtv16 == 0xFFFFu) { qlo = (int32_t)0x80000000; qhi = 0; }
                    else if ((int16_t)rtv16 != 0) { qlo = (int32_t)rsv / (int32_t)(int16_t)rtv16; qhi = (int32_t)rsv % (int32_t)(int16_t)rtv16; }
                    else { qlo = ((int32_t)rsv < 0) ? 1 : -1; qhi = (int32_t)rsv; }
                    set_lane_w(&st->lo, n, (uint32_t)qlo);
                    set_lane_w(&st->hi, n, (uint32_t)qhi);
                }
            } break;
            default:
                ee_core_raise_reserved(st);
                break;
            }
            break;

        case 0x29: /* MMI3 */
            switch (sa) {
            case 0x08: /* PMTHI */ st->hi = st->gpr[rs]; break;
            case 0x09: /* PMTLO */ st->lo = st->gpr[rs]; break;
            case 0x0E: /* PCPYUD */
                if (rd) {
                    ee_reg128_t out;
                    out.ud0 = st->gpr[rs].ud1;
                    out.ud1 = st->gpr[rt].ud1;
                    st->gpr[rd] = out;
                }
                break;
            case 0x12: /* POR */  if (rd) { st->gpr[rd].ud0 = st->gpr[rs].ud0 | st->gpr[rt].ud0; st->gpr[rd].ud1 = st->gpr[rs].ud1 | st->gpr[rt].ud1; } break;
            case 0x13: /* PNOR */ if (rd) { st->gpr[rd].ud0 = ~(st->gpr[rs].ud0 | st->gpr[rt].ud0); st->gpr[rd].ud1 = ~(st->gpr[rs].ud1 | st->gpr[rt].ud1); } break;
            case 0x0A: /* PINTEH - interleaves EVEN halfword lanes of
                        * Rt with EVEN halfword lanes of Rs (odd lanes
                        * untouched by either input), ported from
                        * PINTEH(). Distinct from PINTH above: PINTH
                        * takes ALL of Rt's lanes plus Rs's upper half;
                        * PINTEH takes only the even-indexed lanes of
                        * BOTH Rs and Rt. */
                if (rd) {
                    ee_reg128_t Rs = st->gpr[rs], Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 0)); set_lane_h(&out, 1, lane_h(Rs, 0));
                    set_lane_h(&out, 2, lane_h(Rt, 2)); set_lane_h(&out, 3, lane_h(Rs, 2));
                    set_lane_h(&out, 4, lane_h(Rt, 4)); set_lane_h(&out, 5, lane_h(Rs, 4));
                    set_lane_h(&out, 6, lane_h(Rt, 6)); set_lane_h(&out, 7, lane_h(Rs, 6));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1A: /* PEXCH - swaps halfword lanes 1 and 2 within
                        * each 64-bit half (Rt only), ported from
                        * PEXCH(). Note this is a DIFFERENT permutation
                        * from PEXEH above (which swaps lanes 0 and 2) -
                        * easy to confuse, kept as two distinct case
                        * bodies rather than merged to avoid mixing up
                        * which lane pair each one swaps. */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    set_lane_h(&out, 0, lane_h(Rt, 0)); set_lane_h(&out, 1, lane_h(Rt, 2));
                    set_lane_h(&out, 2, lane_h(Rt, 1)); set_lane_h(&out, 3, lane_h(Rt, 3));
                    set_lane_h(&out, 4, lane_h(Rt, 4)); set_lane_h(&out, 5, lane_h(Rt, 6));
                    set_lane_h(&out, 6, lane_h(Rt, 5)); set_lane_h(&out, 7, lane_h(Rt, 7));
                    st->gpr[rd] = out;
                }
                break;
            case 0x1B: /* PCPYH - broadcasts halfword lane 0 across the
                        * low 64 bits and lane 4 across the high 64
                        * bits (Rt only), ported from PCPYH(). */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    uint16_t lo = lane_h(Rt, 0), hi = lane_h(Rt, 4);
                    for (int n = 0; n < 4; n++) set_lane_h(&out, n, lo);
                    for (int n = 4; n < 8; n++) set_lane_h(&out, n, hi);
                    st->gpr[rd] = out;
                }
                break;
            case 0x1E: /* PEXCW - swaps word lanes 1 and 2 (Rt only),
                        * ported from PEXCW(). Distinct from PEXEW
                        * above (which swaps lanes 0 and 2). */
                if (rd) {
                    ee_reg128_t Rt = st->gpr[rt], out;
                    set_lane_w(&out, 0, lane_w(Rt, 0)); set_lane_w(&out, 1, lane_w(Rt, 2));
                    set_lane_w(&out, 2, lane_w(Rt, 1)); set_lane_w(&out, 3, lane_w(Rt, 3));
                    st->gpr[rd] = out;
                }
                break;
            case 0x00: /* PMADDUW - pipe-paired 32x32(unsigned)->64
                        * multiply-add, ported from _PMADDUW()/
                        * PMADDUW(). Unlike PMADDW, no "division
                        * voodoo" correction applies (that quirk is
                        * PMADDW-specific per real hardware/PCSX2). */
            {
                for (int k = 0; k < 2; k++) {
                    int ss = (k == 0) ? 0 : 2;
                    uint64_t acc = (k == 0) ? (((uint64_t)(uint32_t)st->hi.ud0 << 32) | (uint32_t)st->lo.ud0)
                                             : (((uint64_t)(uint32_t)st->hi.ud1 << 32) | (uint32_t)st->lo.ud1);
                    uint64_t tempu = acc + (uint64_t)lane_w(st->gpr[rs], ss) * (uint64_t)lane_w(st->gpr[rt], ss);
                    if (k == 0) { st->lo.ud0 = sext32((uint32_t)(tempu & 0xFFFFFFFFu)); st->hi.ud0 = sext32((uint32_t)(tempu >> 32)); if (rd) st->gpr[rd].ud0 = tempu; }
                    else        { st->lo.ud1 = sext32((uint32_t)(tempu & 0xFFFFFFFFu)); st->hi.ud1 = sext32((uint32_t)(tempu >> 32)); if (rd) st->gpr[rd].ud1 = tempu; }
                }
            } break;
            case 0x03: /* PSRAVW - variable arithmetic-shift-right of a
                        * word pair (Rt's lanes 0/2, each shifted by a
                        * per-pair amount taken from Rs's matching lane,
                        * masked to 5 bits), writes Rd directly (no
                        * HI/LO involvement) - ported from PSRAVW(). */
                if (rd) {
                    st->gpr[rd].ud0 = sext32((uint32_t)((int32_t)lane_w(st->gpr[rt], 0) >> (lane_w(st->gpr[rs], 0) & 0x1Fu)));
                    st->gpr[rd].ud1 = sext32((uint32_t)((int32_t)lane_w(st->gpr[rt], 2) >> (lane_w(st->gpr[rs], 2) & 0x1Fu)));
                }
                break;
            case 0x0C: /* PMULTUW - pipe-paired 32x32(unsigned)->64
                        * multiply (no accumulate), ported from
                        * _PMULTUW()/PMULTUW(). Real hardware/PCSX2
                        * quirk preserved verbatim: the result's LOW
                        * 32 bits are stored via a SIGNED 32-bit cast
                        * (sign-extended) even though this is the
                        * "unsigned" multiply variant - not "cleaned
                        * up" to be purely unsigned. */
            {
                uint64_t t0 = (uint64_t)lane_w(st->gpr[rs], 0) * (uint64_t)lane_w(st->gpr[rt], 0);
                uint64_t t1 = (uint64_t)lane_w(st->gpr[rs], 2) * (uint64_t)lane_w(st->gpr[rt], 2);
                st->lo.ud0 = sext32((uint32_t)(t0 & 0xFFFFFFFFu)); st->hi.ud0 = sext32((uint32_t)(t0 >> 32));
                st->lo.ud1 = sext32((uint32_t)(t1 & 0xFFFFFFFFu)); st->hi.ud1 = sext32((uint32_t)(t1 >> 32));
                if (rd) { st->gpr[rd].ud0 = t0; st->gpr[rd].ud1 = t1; }
            } break;
            case 0x0D: /* PDIVUW - pipe-paired 32-bit UNSIGNED divide,
                        * ported from _PDIVUW()/PDIVUW(). Divide-by-zero
                        * convention differs from signed PDIVW's
                        * sign-of-dividend rule: LO is always -1
                        * (0xFFFFFFFF) here, per real hardware. */
            {
                for (int k = 0; k < 2; k++) {
                    int ss = (k == 0) ? 0 : 2;
                    uint32_t rsv = lane_w(st->gpr[rs], ss), rtv = lane_w(st->gpr[rt], ss);
                    int32_t qlo, qhi;
                    if (rtv != 0) { qlo = (int32_t)(rsv / rtv); qhi = (int32_t)(rsv % rtv); }
                    else { qlo = -1; qhi = (int32_t)rsv; }
                    if (k == 0) { st->lo.ud0 = sext32((uint32_t)qlo); st->hi.ud0 = sext32((uint32_t)qhi); }
                    else        { st->lo.ud1 = sext32((uint32_t)qlo); st->hi.ud1 = sext32((uint32_t)qhi); }
                }
            } break;
            default:
                ee_core_raise_reserved(st);
                break;
            }
            break;

        default:
                ee_core_raise_reserved(st);
                break;
        }
        break;
