/* GPL-3.0-or-later. Shared scalar/AOT VU semantics. */

    uint32_t dest = VU_U_DEST(w);
    uint32_t ft   = VU_U_FT(w);
    uint32_t fs   = VU_U_FS(w);
    uint32_t fd   = VU_U_FD(w);
    uint32_t fn=w&63u;
    if((fn>=16u&&fn<=23u)||fn==29u||fn==31u||fn==43u||fn==47u) {
        int full=fn==43u||fn==47u;
        int minimum=(fn>=20u&&fn<=23u)||fn==31u||fn==47u;
        uint32_t broadcast=(fn==29u||fn==31u)?vi[21]:vf[ft][fn&3u];
        if(!ft && !(fn==29u||fn==31u))broadcast=(fn&3u)==3u?0x3f800000u:0;
        for(unsigned l=0;l<4;l++)if(fd && (dest&(8u>>l))) {
            uint32_t a=fs?vf[fs][l]:(l==3u?0x3f800000u:0);
            uint32_t b=full?(ft?vf[ft][l]:(l==3u?0x3f800000u:0)):broadcast;
            vf[fd][l]=vu_minmax_bits(a,b,minimum);
        }
        return 1;
    }
    float Q = vu_f(vi[22]);
    float I = vu_f(vi[21]);
    float r[4];

    if (VU_U_IS_SPECIAL(w)) {
        uint32_t sub = VU_SPEC_SUBOP(w);
        uint32_t bc  = VU_U_BC(w);

        /* Round 573 (task #548): real accumulator-broadcast family,
         * ground-truthed from PCSX2's own _UPPER_FD_bc_TABLE dispatch
         * tables (see vu_opcodes.h's updated citation) - bc selects
         * which lane of Ft is broadcast into every lane of the ACC
         * computation. Evidence: the live diskless-boot VU1 trace
         * that motivated this round hit sub=0x01(SUBAbc) bc=3(w) and
         * sub=0x09(SUBAqi group) bc=0(SUBAq) 32 times each - real,
         * load-bearing instructions this project was silently
         * treating as unimplemented no-ops. The sibling ADDAbc/
         * MADDAbc/MSUBAbc/MULAbc/ADDAq/MADDAq/ADDAi/MADDAi/MSUBAq/
         * SUBAi/MSUBAi forms are implemented alongside since the real
         * table structure is now unambiguous (not spec-only guessing -
         * directly transcribed from the real dispatch table). */
        if (sub == VUS_FD_ADDABC_GROUP) { /* ADDAx/y/z/w: ACC = Fs + Ft[bc] */
            float ftbc = vu_f(vf[ft][bc]);
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + ftbc;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_SUBABC_GROUP) { /* SUBAx/y/z/w: ACC = Fs - Ft[bc] */
            float ftbc = vu_f(vf[ft][bc]);
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - ftbc;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_MADDABC_GROUP) { /* MADDAx/y/z/w: ACC = ACC + Fs*Ft[bc] */
            float ftbc = vu_f(vf[ft][bc]);
            for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * ftbc;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_MSUBABC_GROUP) { /* MSUBAx/y/z/w: ACC = ACC - Fs*Ft[bc] */
            float ftbc = vu_f(vf[ft][bc]);
            for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * ftbc;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_MULABC_GROUP) { /* MULAx/y/z/w: ACC = Fs*Ft[bc] */
            float ftbc = vu_f(vf[ft][bc]);
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * ftbc;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_ADDAQI_GROUP) { /* bc: 00=ADDAq 01=MADDAq 10=ADDAi 11=MADDAi */
            if (bc == 0) { for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + Q; }
            else if (bc == 1) { for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * Q; }
            else if (bc == 2) { for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + I; }
            else { for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * I; }
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_SUBAQI_GROUP) { /* bc: 00=SUBAq 01=MSUBAq 10=SUBAi 11=MSUBAi */
            if (bc == 0) { for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - Q; }
            else if (bc == 1) { for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * Q; }
            else if (bc == 2) { for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - I; }
            else { for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * I; }
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }

        if (sub == VUS_FD_ADDA_GROUP && bc == 0) { /* ADDA: ACC = Fs + Ft */
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + vu_f(vf[ft][l]);
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_ADDA_GROUP && bc == 1) { /* MADDA: ACC = ACC + Fs*Ft */
            for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * vu_f(vf[ft][l]);
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        /* Real _UPPER_FD_10_TABLE[10] selects MULA. */
        if (sub == VUS_FD_ADDA_GROUP && bc == 2) {
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * vu_f(vf[ft][l]);
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_SUBA_GROUP && bc == 0) { /* SUBA: ACC = Fs - Ft */
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - vu_f(vf[ft][l]);
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_SUBA_GROUP && bc == 1) { /* MSUBA: ACC = ACC - Fs*Ft */
            for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * vu_f(vf[ft][l]);
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_SUBA_GROUP && bc == 2) { /* OPMULA: ACC = Fs x Ft (outer/cross product terms, xyz only) */
            acc[0] = vu_u(vu_f(vf[fs][1]) * vu_f(vf[ft][2]));
            acc[1] = vu_u(vu_f(vf[fs][2]) * vu_f(vf[ft][0]));
            acc[2] = vu_u(vu_f(vf[fs][0]) * vu_f(vf[ft][1]));
            return 1;
        }
        if (sub == VUS_FD_SUBA_GROUP && bc == 3) { /* NOP (upper) */
            return 1;
        }
        if (sub == VUS_FD_ABSCLIP && bc == 0) { /* MULAq: ACC = Fs*Q (real table[7][bc=0]) */
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * Q;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_ABSCLIP && bc == 1) { /* ABS: Fd(=ft slot) = |Fs| */
            for (int l = 0; l < 4; l++) r[l] = fabsf(vu_f(vf[fs][l]));
            vu_write_dest(vf, ft, dest, r);
            return 1;
        }
        if (sub == VUS_FD_ABSCLIP && bc == 2) { /* MULAi: ACC = Fs*I (real table[7][bc=2]) */
            for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * I;
            for (int l = 0; l < 4; l++) if ((dest >> (3 - l)) & 1) acc[l] = vu_u(r[l]);
            return 1;
        }
        if (sub == VUS_FD_ABSCLIP && bc == 3) {
            /* CLIPw (Round 573, task #548): judges |Fs.x|,|Fs.y|,|Fs.z|
             * against |Ft.w|, ported bit-exact from real PCSX2's
             * _vuCLIP() (docs/reference/pcsx2/pcsx2/VUops.cpp) - the
             * SAME real algorithm this project's VU0 macro-mode
             * VCLIPw already implements at ee_core.c's idx==31 case
             * (see that comment for the full citation trail: signed-
             * integer sign-flip-XOR comparison, NOT a float compare,
             * denormal Ft.w clamped to the largest denormal so only
             * non-denormals compare higher). No destination register
             * (Fd) - Fs/Ft here are true source operands, unlike ABS's
             * reuse of the Ft field slot as a destination above.
             * Result: 6 new judgment bits shifted into the clip-flag
             * register each call, masked to the low 24 bits (4 calls'
             * worth of history, matching real hardware's REG_CLIP_FLAG
             * semantics). Stored in vi[18], reusing this project's own
             * R(20)/I(21)/Q(22)-style convention of keeping VU control
             * registers directly in the generic vi[] array - vi[18] =
             * REG_CLIP_FLAG in PCSX2's VU.h VURegFlags enum, matching
             * the ee_core.c VU0-macro-mode CLIP's cop2_ctrl[18] choice
             * exactly so both VU0 and VU1 use the same real register
             * index for the same real register.
             *
             * Evidence this was worth implementing (not spec-only):
             * Round 573's live diskless-boot VU1 trace hit this exact
             * bc==3 case 64 times out of 66,376 real VU1 instructions
             * executed (the entire "unimplemented_opcodes_seen=96"
             * count, minus 32 hits of upper funct=0x30 - confirmed via
             * live cross-check against PCSX2's own opcode table
             * (the PREFIX##unknown entries at the 0x30 slot in
             * VUmicro.h's macro-generated dispatch tables in VUops.cpp)
             * to be a genuinely invalid/unknown encoding on real
             * hardware too, not a modeling gap - left unimplemented). */
            uint32_t ftw = vf[ft][3];
            int32_t value = (int32_t)ftw;
            value = (ftw & 0x7f800000u) ? (value & 0x7fffffff) : 0x007fffff;
            uint32_t fsx = vf[fs][0], fsy = vf[fs][1], fsz = vf[fs][2];
            uint32_t clip = vi[18];
            clip <<= 6;
            if ((int32_t)(fsx ^ 0x00000000u) > value) clip |= 0x01u;
            if ((int32_t)(fsx ^ 0x80000000u) > value) clip |= 0x02u;
            if ((int32_t)(fsy ^ 0x00000000u) > value) clip |= 0x04u;
            if ((int32_t)(fsy ^ 0x80000000u) > value) clip |= 0x08u;
            if ((int32_t)(fsz ^ 0x00000000u) > value) clip |= 0x10u;
            if ((int32_t)(fsz ^ 0x80000000u) > value) clip |= 0x20u;
            vi[18] = clip & 0xFFFFFFu;
            return 1;
        }
        if (sub == VUS_FD_ITOF) { /* ITOF0/4/12/15: Fd(=ft slot) = (int32)Fs / 2^scale */
            static const int shift[4] = {0, 4, 12, 15};
            for (int l = 0; l < 4; l++)
                r[l] = (float)(int32_t)vf[fs][l] / (float)(1u << shift[bc]);
            vu_write_dest(vf, ft, dest, r);
            return 1;
        }
        if (sub == VUS_FD_FTOI) { /* FTOI0/4/12/15: Fd(=ft slot) = (int32)(Fs * 2^scale), bits stored raw */
            static const int shift[4] = {0, 4, 12, 15};
            for (int l = 0; l < 4; l++) {
                float scaled = vu_f(vf[fs][l]) * (float)(1u << shift[bc]);
                uint32_t sb = vu_u(scaled);
                int32_t iv = (sb&0x7fffffffu)>=0x4f000000u
                    ? ((sb&0x80000000u)?INT32_MIN:INT32_MAX) : (int32_t)scaled;
                r[l] = vu_f((uint32_t)iv);
            }
            vu_write_dest(vf, ft, dest, r);
            return 1;
        }
        return 0; /* CLIPw and the accumulator-broadcast/AI/AQ forms - not implemented, see vu_opcodes.h */
    }

    /* Class B: broadcast forms (bits5-2 = 0x0-0x6) */
    {
        uint32_t sub4 = VU_U_SUB4(w);
        if (sub4 <= 0x6u) {
            uint32_t bc = VU_U_BC(w);
            float ftbc = vu_f(vf[ft][bc]);
            int matched = 1;
            switch (sub4) {
                case VUB_ADDBC:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + ftbc; break;
                case VUB_SUBBC:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - ftbc; break;
                case VUB_MULBC:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * ftbc; break;
                case VUB_MAXBC:  for (int l = 0; l < 4; l++) r[l] = vu_f(vu_minmax_bits(vf[fs][l],vf[ft][bc],0)); break;
                case VUB_MINIBC: for (int l = 0; l < 4; l++) r[l] = vu_f(vu_minmax_bits(vf[fs][l],vf[ft][bc],1)); break;
                case VUB_MADDBC: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * ftbc; break;
                case VUB_MSUBBC: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * ftbc; break;
                default: matched = 0; break;
            }
            if (matched) { vu_write_dest(vf, fd, dest, r); return 1; }
        }
    }

    /* Class A: full 6-bit funct */
    {
        uint32_t funct = VU_U_FUNCT6(w);
        switch (funct) {
            /* PCSX2 VUops.cpp: _UPPER table opcode 0x2e, _vuOPMSUB.
             * Capture all source components before aliased writes; xyz
             * always update and w is preserved, independent of dest. */
            case VUA_OPMSUB:
                for (int l = 0; l < 3; l++)
                    r[l] = vu_f(acc[l]) - vu_f(vf[fs][(l+1)%3]) * vu_f(vf[ft][(l+2)%3]);
                if (fd) for (int l = 0; l < 3; l++) vf[fd][l] = vu_u(r[l]);
                return 1;
            case VUA_ADD:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + vu_f(vf[ft][l]); break;
            case VUA_SUB:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - vu_f(vf[ft][l]); break;
            case VUA_MUL:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * vu_f(vf[ft][l]); break;
            case VUA_MAX:  for (int l = 0; l < 4; l++) r[l] = vu_f(vu_minmax_bits(vf[fs][l],vf[ft][l],0)); break;
            case VUA_MINI: for (int l = 0; l < 4; l++) r[l] = vu_f(vu_minmax_bits(vf[fs][l],vf[ft][l],1)); break;
            case VUA_MADD: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * vu_f(vf[ft][l]); break;
            case VUA_MSUB: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * vu_f(vf[ft][l]); break;

            case VUA_ADDQ:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + Q; break;
            case VUA_SUBQ:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - Q; break;
            case VUA_MULQ:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * Q; break;
            case VUA_MADDQ: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * Q; break;
            case VUA_MSUBQ: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * Q; break;

            case VUA_ADDI:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) + I; break;
            case VUA_SUBI:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) - I; break;
            case VUA_MULI:  for (int l = 0; l < 4; l++) r[l] = vu_f(vf[fs][l]) * I; break;
            case VUA_MADDI: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) + vu_f(vf[fs][l]) * I; break;
            case VUA_MSUBI: for (int l = 0; l < 4; l++) r[l] = vu_f(acc[l]) - vu_f(vf[fs][l]) * I; break;
            case VUA_MAXI:  for (int l = 0; l < 4; l++) r[l] = vu_f(vu_minmax_bits(vf[fs][l],vi[21],0)); break;
            case VUA_MINII: for (int l = 0; l < 4; l++) r[l] = vu_f(vu_minmax_bits(vf[fs][l],vi[21],1)); break;

            default: return 0; /* includes the 0x1B collision case - see vu_opcodes.h */
        }
        vu_write_dest(vf, fd, dest, r);
        return 1;
    }
