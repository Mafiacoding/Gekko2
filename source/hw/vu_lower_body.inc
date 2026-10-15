/* GPL-3.0-or-later. Shared scalar/AOT VU semantics. */

    uint32_t opcode = VU_L_OPCODE(w);
    uint32_t dest = VU_L_DEST(w);
    uint32_t rt = VU_L_RT(w), rs = VU_L_RS(w), rd = VU_L_RD(w);

    /* real branch offset: bits10-0, sign-extended, scaled by 8
     * (doublewords), target = pc + 8 (delay slot) + offset*8 */
    int32_t off11 = vu_sext(VU_L_IMM11(w), 11);

    switch (opcode) {
        case VUL_B:
            *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8);
            return 1;
        case VUL_BAL:
            vu_write_vi(vi, rt, ((*branch_delay==1u?*branch_target+8u:pc+16u)>>3));
            *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8);
            return 1;
        case VUL_JR:
            *branch_delay = 2u; *branch_target = vu_read_vi16(vi, rs) * 8u; /* Is holds an instruction-pair index, matching MSCAL's own *8 convention (vu.h) */
            return 1;
        case VUL_JALR: {
            /* Capture target before a potentially aliased link write,
             * matching primary VUops.cpp _vuJALR. */
            uint32_t target = vu_read_vi16(vi, rs) * 8u;
            vu_write_vi(vi, rt, ((*branch_delay==1u?*branch_target+8u:pc+16u)>>3));
            *branch_delay = 2u; *branch_target = target;
            return 1;
        }
        case VUL_FCEQ:vu_write_vi(vi,1,(vi[18]&0xffffffu)==(w&0xffffffu));return 1;
        case VUL_FCSET:vi[18]=w&0xffffffu;return 1;
        case VUL_FCAND:vu_write_vi(vi,1,!!((vi[18]&0xffffffu)&(w&0xffffffu)));return 1;
        case VUL_FCOR:vu_write_vi(vi,1,((vi[18]|w)&0xffffffu)==0xffffffu);return 1;
        case VUL_FCGET:vu_write_vi(vi,rt,vi[18]&0xfffu);return 1;
        case VUL_FSEQ:case VUL_FSSET:case VUL_FSAND:case VUL_FSOR: {
            uint32_t imm=((w>>10)&0x800u)|(w&0x7ffu),status=vi[16]&0xfffu;
            if(opcode==VUL_FSSET)vi[16]=(vi[16]&0x3fu)|(imm&0xfc0u);
            else vu_write_vi(vi,rt,opcode==VUL_FSEQ?status==imm:opcode==VUL_FSAND?status&imm:status|imm);
            return 1;
        }
        case VUL_FMEQ:case VUL_FMAND:case VUL_FMOR: {
            uint32_t mac=vi[17]&0xffffu,source=vu_read_vi16(vi,rs);
            vu_write_vi(vi,rt,opcode==VUL_FMEQ?mac==source:opcode==VUL_FMAND?mac&source:mac|source);return 1;
        }
        case VUL_IBEQ:
            if (vu_read_vi16(vi, rs) == vu_read_vi16(vi, rt)) { *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8); }
            return 1;
        case VUL_IBNE:
            if (vu_read_vi16(vi, rs) != vu_read_vi16(vi, rt)) { *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8); }
            return 1;
        case VUL_IBLTZ:
            if (vu_read_vi16_signed(vi, rs) < 0)  { *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8); }
            return 1;
        case VUL_IBGTZ:
            if (vu_read_vi16_signed(vi, rs) > 0)  { *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8); }
            return 1;
        case VUL_IBLEZ:
            if (vu_read_vi16_signed(vi, rs) <= 0) { *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8); }
            return 1;
        case VUL_IBGEZ:
            if (vu_read_vi16_signed(vi, rs) >= 0) { *branch_delay = 2u; *branch_target = (uint32_t)((int32_t)pc + 8 + off11 * 8); }
            return 1;

        case VUL_LQ: { /* Masked Ft = mem[(Is+imm11)*16 .. +15] */
            uint32_t addr = (uint32_t)((int32_t)vu_read_vi16(vi, rs) + off11) * 16u;
            for (int l = 0; l < 4; l++) {
                if ((dest & (8u >> l)) && rt != 0) vf[rt][l] = vu_rd_le32(mem + ((addr + (uint32_t)l * 4u) & mem_mask));
            }
            return 1;
        }
        case VUL_SQ: { /* mem[(It+imm11)*16..] = Fs */
            uint32_t addr = (uint32_t)((int32_t)vu_read_vi16(vi, rt) + off11) * 16u;
            for (int l = 0; l < 4; l++) if (dest & (8u >> l))
                vu_wr_le32(mem + ((addr + (uint32_t)l * 4u) & mem_mask), vf[rs][l]);
            return 1;
        }
        case VUL_ILW: { /* It = mem[(Is+imm11)*16 + elem*4], truncated to 16 bits; elem = lowest set dest bit */
            int elem = 0;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) { elem = l; break; }
            uint32_t addr = (uint32_t)((int32_t)vu_read_vi16(vi, rs) + off11) * 16u + (uint32_t)elem * 4u;
            uint32_t word = vu_rd_le32(mem + (addr & mem_mask));
            vu_write_vi(vi, rt, word);
            return 1;
        }
        case VUL_ISW: { /* mem[(Is+imm11)*16 + elem*4] = It (zero-extended) */
            int elem = 0;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) { elem = l; break; }
            uint32_t addr = (uint32_t)((int32_t)vu_read_vi16(vi, rs) + off11) * 16u + (uint32_t)elem * 4u;
            vu_wr_le32(mem + (addr & mem_mask), vu_read_vi16(vi, rt));
            return 1;
        }

        case VU_L_SPECIAL_OPCODE: {
            uint32_t funct6 = VU_L_FUNCT6(w);
            switch (funct6) {
                case VULS_IADD:  vu_write_vi(vi, rd, vu_read_vi16(vi, rs) + vu_read_vi16(vi, rt)); return 1;
                case VULS_ISUB:  vu_write_vi(vi, rd, vu_read_vi16(vi, rs) - vu_read_vi16(vi, rt)); return 1;
                case VULS_IAND:  vu_write_vi(vi, rd, vu_read_vi16(vi, rs) & vu_read_vi16(vi, rt)); return 1;
                case VULS_IOR:   vu_write_vi(vi, rd, vu_read_vi16(vi, rs) | vu_read_vi16(vi, rt)); return 1;
                case VULS_IADDI: {
                    int32_t imm5 = vu_sext(rd, 5); /* 5-bit signed immediate in the rd slot (bits10-6) - see vu_opcodes.h */
                    vu_write_vi(vi, rt, (uint32_t)((int32_t)vu_read_vi16(vi, rs) + imm5));
                    return 1;
                }
                default: break;
            }

            {
                uint32_t fdslot = VU_L_RD(w);
                uint32_t bc2 = w & 0x3u;
                if(mem_mask==VU1_MEM_SIZE-1u&&funct6>=60u) {
                    if(fdslot==25u&&bc2==0u) {
                        if(rt)for(unsigned l=0;l<4;l++)if(dest&(8u>>l))vf[rt][l]=vi[23];
                        return 1;
                    }
                    if(vu_efu_compute(vf,vi,w))return 1;
                }
                if (fdslot == VULS_FD_MOVE_GROUP) {
                    float rr[4];
                    for (int l = 0; l < 4; l++) rr[l] = vu_f(vf[rs][l]);
                    if (bc2 == 0) { vu_write_dest(vf, rt, dest, rr); return 1; } /* MOVE */
                    if (bc2 == 1) { /* MR32: rotate lanes (x,y,z,w) -> (y,z,w,x) */
                        float rot[4] = { rr[1], rr[2], rr[3], rr[0] };
                        vu_write_dest(vf, rt, dest, rot);
                        return 1;
                    }
                }
                if (fdslot == VULS_FD_LSQ_GROUP) {
                    /* Loads use Is/Ft; stores use It/Fs, including updates. */
                    if (bc2 == 0) { /* LQI */
                        uint32_t addr = vu_read_vi16(vi, rs) * 16u;
                        for (int l = 0; l < 4; l++) if ((dest & (8u >> l)) && rt != 0) vf[rt][l] = vu_rd_le32(mem + ((addr + (uint32_t)l * 4u) & mem_mask));
                        vu_write_vi(vi, rs, vu_read_vi16(vi, rs) + 1u);
                        return 1;
                    }
                    if (bc2 == 1) { /* SQI */
                        uint32_t addr = vu_read_vi16(vi, rt) * 16u;
                        for (int l = 0; l < 4; l++) if (dest & (8u >> l)) vu_wr_le32(mem + ((addr + (uint32_t)l * 4u) & mem_mask), vf[rs][l]);
                        vu_write_vi(vi, rt, vu_read_vi16(vi, rt) + 1u);
                        return 1;
                    }
                    if (bc2 == 2) { /* LQD: pre-decrement */
                        vu_write_vi(vi, rs, vu_read_vi16(vi, rs) - 1u);
                        uint32_t addr = vu_read_vi16(vi, rs) * 16u;
                        for (int l = 0; l < 4; l++) if ((dest & (8u >> l)) && rt != 0) vf[rt][l] = vu_rd_le32(mem + ((addr + (uint32_t)l * 4u) & mem_mask));
                        return 1;
                    }
                    if (bc2 == 3) { /* SQD: pre-decrement */
                        vu_write_vi(vi, rt, vu_read_vi16(vi, rt) - 1u);
                        uint32_t addr = vu_read_vi16(vi, rt) * 16u;
                        for (int l = 0; l < 4; l++) if (dest & (8u >> l)) vu_wr_le32(mem + ((addr + (uint32_t)l * 4u) & mem_mask), vf[rs][l]);
                        return 1;
                    }
                }
                if (fdslot == VULS_FD_MTIR_GROUP) {
                    if (bc2 == 0) { /* MTIR: It = Fs.elem (elem via bits22-21, see vu_opcodes.h) */
                        uint32_t elem = (w >> 21) & 0x3u;
                        vu_write_vi(vi, rt, vf[rs][elem]);
                        return 1;
                    }
                    if (bc2 == 1) { /* MFIR: Fd(masked lanes) = sign-extended It, raw bits (not numeric conversion) */
                        float rr[4];
                        uint32_t bits = (uint32_t)(int32_t)vu_read_vi16_signed(vi, rs);
                        for (int l = 0; l < 4; l++) rr[l] = vu_f(bits);
                        vu_write_dest(vf, rt, dest, rr);
                        return 1;
                    }
                    if (bc2 == 2) { /* ILWR: It = mem[Is*16 + elem*4], no immediate */
                        int elem = 0;
                        for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) { elem = l; break; }
                        uint32_t addr = vu_read_vi16(vi, rs) * 16u + (uint32_t)elem * 4u;
                        vu_write_vi(vi, rt, vu_rd_le32(mem + (addr & mem_mask)));
                        return 1;
                    }
                    if (bc2 == 3) { /* ISWR: mem[Is*16 + elem*4] = It, no immediate */
                        int elem = 0;
                        for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) { elem = l; break; }
                        uint32_t addr = vu_read_vi16(vi, rs) * 16u + (uint32_t)elem * 4u;
                        vu_wr_le32(mem + (addr & mem_mask), vu_read_vi16(vi, rt));
                        return 1;
                    }
                }
                if (fdslot == VULS_FD_DIVQ_GROUP) {
                    if(bc2<3u)vu_q_compute(vf,vi,w);
                    return 1;
                }
                if (fdslot == VULS_FD_WAITP && bc2 == 3) return 1; /* WAITP - same as WAITQ, no-op here */
                if (fdslot == VULS_FD_XTOP_GROUP && bc2 <= 1) {
                    vif_state_t *vif = mem_mask == VU1_MEM_SIZE - 1u
                        ? vif1_get_state() : vif0_get_state();
                    vu_write_vi(vi, rt, bc2 == 0 ? vif->top : vif->itop);
                    return 1;
                }
                if (fdslot == VULS_FD_XGKICK_GROUP && bc2 == 0) {
                    /* XGKICK Is: kicks off a real PATH1 GIF transfer starting
                     * at VU1 local mem address (Is register, masked to 0x3FF
                     * quadwords then *16 - see PCSX2's _vuXGKICK(), a live
                     * fetch this round: "addr = (VI[_Is_].US[0] & 0x3ff) *
                     * 16"). Round 571 (task #536/#545): this was the missing
                     * link between the already-working VU1 microcode
                     * interpreter (MSCAL/MSCNT run real VU1 programs to
                     * completion - see vif.c) and the GS - without XGKICK,
                     * ANY geometry a VU1 microprogram computes (the real
                     * PS2 boot animation's rotating-logo 3D geometry is
                     * VU1-driven, per real PS2 architecture - EE-side GIF
                     * packets alone only ever carried the 2D wireframe/
                     * sprite UI elements, matching Round 570's finding of
                     * zero EE-side triangle vertex kicks) had no way to
                     * ever reach the rasterizer.
                     *
                     * The production VU1 sender meters quadwords at VU
                     * issue boundaries and flushes at E/second-kick boundaries.
                     * VU execution itself still runs synchronously within VIF.
                     *
                     * VU0 has NO real XGKICK/GIF connection at all on real
                     * hardware (only VU1 is wired to the GIF unit) - this
                     * function is shared between VU0 macro-mode's COP2
                     * micro-mode path (ee_core.c's vu0_exec_micro(), called
                     * with VU0's 4KB mem_mask=0xFFF) and VU1's real
                     * micro-mode (vu1_exec_micro(), mem_mask=0x3FFF), so the
                     * real transfer is gated on VU1's specific mem_mask
                     * value - VU0 callers safely no-op here instead of
                     * misinterpreting VU0's much smaller data memory as a
                     * GS packet stream. */
                    uint32_t addr = (vu_read_vi16(vi, rs) & 0x3FFu) * 16u;
                    /* Only the physical VU1 instance can own PATH1. */
                    if(mem==g_vu1.mem && mem_mask==VU1_MEM_SIZE-1u)
                        vu1_issue_xgkick(addr);
                    return 1;
                }
                if (fdslot == VULS_FD_R_GROUP) {
                    /* Real VU R register lives at VI[20] (REG_R).  PCSX2's
                     * interpreter keeps it in 1.x IEEE mantissa form: the
                     * exponent is forced to 0x3f800000 and only 23 mantissa
                     * bits participate in the LFSR/XOR operations.  All four
                     * R instructions are architecturally suppressed when Ft
                     * is VF0, including RINIT/RXOR's REG_R side effect. */
                    if (rt == 0) return 1;
                    uint32_t r = vi[20];
                    if (bc2 == 0) { /* RNEXT */
                        uint32_t x = (r >> 4) & 1u, y = (r >> 22) & 1u;
                        r = (r << 1) ^ x ^ y;
                        r = (r & 0x007fffffu) | 0x3f800000u;
                        vi[20] = r;
                        float rr[4] = { vu_f(r), vu_f(r), vu_f(r), vu_f(r) };
                        vu_write_dest(vf, rt, dest, rr);
                        return 1;
                    }
                    if (bc2 == 1) { /* RGET */
                        float rr[4] = { vu_f(r), vu_f(r), vu_f(r), vu_f(r) };
                        vu_write_dest(vf, rt, dest, rr);
                        return 1;
                    }
                    if (bc2 == 2) { /* RINIT */
                        uint32_t elem = (w >> 21) & 3u;
                        vi[20] = (vf[rs][elem] & 0x007fffffu) | 0x3f800000u;
                        return 1;
                    }
                    if (bc2 == 3) { /* RXOR */
                        uint32_t elem = (w >> 21) & 3u;
                        vi[20] = ((vf[rs][elem] ^ r) & 0x007fffffu) | 0x3f800000u;
                        return 1;
                    }
                }
                return 0; /* Any genuinely unmatched SPECIAL2 selector stays on the explicit unimplemented path. */
            }
        }

        case VUL_IADDIU: { /* Round 581 (task #559): It = Is + imm15, real
             * unsigned 15-bit immediate packing ground-truthed from
             * PCSX2's own _vuIADDIU() (VUops.cpp): imm15 =
             * ((code>>10)&0x7800)|(code&0x7ff) - bits 14-11 of the
             * immediate come from code bits 13-10, bits 10-0 come
             * straight from code bits 10-0. Resolves this project's
             * prior "uncertain immediate packing" note. vu_write_vi()
             * already implements the real hardware's It==0 no-op and
             * 16-bit truncation, matching _vuIADDIU()'s _It_==0 guard
             * and VI[].SS[0]'s 16-bit storage. */
            uint32_t imm15 = ((w >> 10) & 0x7800u) | (w & 0x7FFu);
            vu_write_vi(vi, rt, vu_read_vi16(vi, rs) + imm15);
            return 1;
        }
        case VUL_ISUBIU: { /* Round 581 (task #559): It = Is - imm15, same
             * real immediate packing as IADDIU above (_vuISUBIU()). */
            uint32_t imm15 = ((w >> 10) & 0x7800u) | (w & 0x7FFu);
            vu_write_vi(vi, rt, vu_read_vi16(vi, rs) - imm15);
            return 1;
        }

        default:
            return 0; /* FC-/FS-/FM-family flag ops - still unimplemented */
    }
