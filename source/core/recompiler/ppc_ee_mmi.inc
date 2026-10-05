/* Additional R5900 MMI emitters. Included after the common PPC encoders.
 * Each body uses only caller-saved registers and at most 128 code words.
 * Return 1 when handled; return 0 without emitting on an unknown opcode. */
static uint32_t mmi_cmpw(int a,int b) {return (31u<<26)|((unsigned)a<<16)|((unsigned)b<<11);}
static uint32_t mmi_cmpi(int a,int16_t v) {return (11u<<26)|((unsigned)a<<16)|(uint16_t)v;}
static void mmi_patch_bc(ppc_codegen_ctx_t *c,size_t at,int bo,int bi) {
    c->code[at]=enc_bc(bo,bi,(int32_t)(c->used_words-at)*4);
}
static void mmi_patch_b(ppc_codegen_ctx_t *c,size_t at) {
    c->code[at]=enc_b((int32_t)(c->used_words-at)*4);
}
static int emit_mmi_extra(ppc_codegen_ctx_t *ctx,unsigned rs,unsigned rt,unsigned rd,unsigned sa,unsigned fn)
{
    if((fn==9u&&(sa==0x10u||sa==0x11u||sa==0x14u||sa==0x15u))) {
        for(unsigned n=0;n<8;n++) {
            int p=4+(int)n;
            emit(ctx,enc_lhz(p,3,mmi_h_off(rs,n)));emit(ctx,enc_lhz(12,3,mmi_h_off(rt,n)));
            emit(ctx,enc_extsh(p,p));emit(ctx,enc_extsh(12,12));emit(ctx,enc_mullw(p,p,12));
        }
        if(sa==0x10u||sa==0x14u)for(unsigned n=0;n<8;n++) {
            unsigned bank=(n&2u)?HI_IDX:LO_IDX,lane=(n>>2)*2u+(n&1u);int p=4+(int)n;
            emit(ctx,enc_lwz(12,3,mmi_w_off(bank,lane)));
            emit(ctx,sa==0x10u?enc_add(p,12,p):enc_subf(p,p,12));
            emit(ctx,enc_stw(p,3,mmi_w_off(bank,lane)));
        } else for(unsigned n=0;n<8;n+=2) {
            unsigned bank=(n&2u)?HI_IDX:LO_IDX,lane=(n>>2)*2u;int even=4+(int)n,odd=even+1;
            emit(ctx,sa==0x11u?enc_add(even,odd,even):enc_subf(even,even,odd));
            if(sa==0x15u)emit(ctx,enc_nor(odd,odd,odd));
            emit(ctx,enc_stw(even,3,mmi_w_off(bank,lane)));emit(ctx,enc_stw(odd,3,mmi_w_off(bank,lane+1)));
        }
        if(rd)for(unsigned n=0;n<4;n++)emit(ctx,enc_stw(4+(int)n*2,3,mmi_w_off(rd,n)));
        return 1;
    }
    if((fn==9u&&(sa==10u||sa==26u||sa==27u||sa==30u))||
       (fn==41u&&(sa==10u||sa==26u||sa==30u))) {
        if(!rd)return 1;
        static const unsigned eh[8]={2,1,0,3,6,5,4,7},rev[8]={3,2,1,0,7,6,5,4};
        static const unsigned ch[8]={0,2,1,3,4,6,5,7},ew[4]={2,1,0,3},cw[4]={0,2,1,3};
        unsigned words=sa==30u,lanes=words?4u:8u;
        for(unsigned n=0;n<lanes;n++) {
            unsigned src=rt,lane=n;
            if(words)lane=fn==9u?ew[n]:cw[n];
            else if(sa==10u){src=(n&1u)?rs:rt;lane=fn==9u?((n&1u)?n/2u+4u:n/2u):(n&~1u);}
            else lane=fn==41u?ch[n]:(sa==27u?rev[n]:eh[n]);
            emit(ctx,words?enc_lwz(4+n,3,mmi_w_off(src,lane)):enc_lhz(4+n,3,mmi_h_off(src,lane)));
        }
        for(unsigned n=0;n<lanes;n++)emit(ctx,words?enc_stw(4+n,3,mmi_w_off(rd,n)):enc_sth(4+n,3,mmi_h_off(rd,n)));
        return 1;
    }
    if((fn==9u&&(sa==2u||sa==3u))||(fn==41u&&sa==3u)) {
        if(!rd)return 1;
        for(unsigned pipe=0;pipe<2;pipe++) {
            int lane=pipe*2;
            emit(ctx,enc_lwz(4,3,mmi_w_off(rs,lane)));emit(ctx,enc_lwz(5,3,mmi_w_off(rt,lane)));
            emit(ctx,enc_andi_dot(4,4,31));
            emit(ctx,fn==41u?enc_sraw(6,5,4):(sa==2u?enc_slw(6,5,4):enc_srw(6,5,4)));
            emit(ctx,enc_srawi(7,6,31));emit(ctx,enc_stw(6,3,mmi_w_off(rd,lane)));emit(ctx,enc_stw(7,3,mmi_w_off(rd,lane+1)));
        }
        return 1;
    }
    if(fn==41u&&(sa==0u||sa==12u||sa==13u)) {
        for(unsigned pipe=0;pipe<2;pipe++) {
            int lane=pipe*2;
            emit(ctx,enc_lwz(4,3,mmi_w_off(rs,lane)));emit(ctx,enc_lwz(5,3,mmi_w_off(rt,lane)));
            if(sa!=13u) {
                emit(ctx,enc_mullw(6,4,5));emit(ctx,enc_mulhwu(7,4,5));
                if(sa==0u){emit(ctx,enc_lwz(8,3,mmi_w_off(LO_IDX,lane)));emit(ctx,enc_lwz(9,3,mmi_w_off(HI_IDX,lane)));
                    emit(ctx,enc_addc(6,6,8));emit(ctx,enc_adde(7,7,9));}
            } else {
                emit(ctx,mmi_cmpi(5,0));size_t zero=ctx->used_words;emit(ctx,0);
                emit(ctx,enc_divwu(6,4,5));emit(ctx,enc_mullw(7,6,5));emit(ctx,enc_subf(7,7,4));
                size_t done=ctx->used_words;emit(ctx,0);mmi_patch_bc(ctx,zero,12,2);
                emit(ctx,enc_addi(6,0,-1));emit(ctx,enc_or(7,4,4));mmi_patch_b(ctx,done);
            }
            emit(ctx,enc_srawi(8,6,31));emit(ctx,enc_srawi(9,7,31));
            emit(ctx,enc_stw(6,3,mmi_w_off(LO_IDX,lane)));emit(ctx,enc_stw(8,3,mmi_w_off(LO_IDX,lane+1)));
            emit(ctx,enc_stw(7,3,mmi_w_off(HI_IDX,lane)));emit(ctx,enc_stw(9,3,mmi_w_off(HI_IDX,lane+1)));
            if(rd&&sa!=13u){emit(ctx,enc_stw(6,3,mmi_w_off(rd,lane)));emit(ctx,enc_stw(7,3,mmi_w_off(rd,lane+1)));}
        }
        return 1;
    }
    if(fn==8u||fn==40u) {
        unsigned bits=0,kind=0;
        if(fn==8u) {
            if(sa==2u||sa==6u||sa==10u){bits=32u>>(sa/4u);kind=1;}
            if(sa==3u||sa==7u){bits=32u>>(sa/4u);kind=2;}
            if(sa==16u||sa==17u||sa==20u||sa==21u||sa==24u||sa==25u){bits=32u>>((sa-16u)/4u);kind=(sa&1u)?5u:4u;}
        } else {
            if(sa==2u||sa==6u||sa==10u){bits=32u>>(sa/4u);kind=6;}
            if(sa==3u||sa==7u){bits=32u>>(sa/4u);kind=3;}
            if(sa==1u||sa==5u){bits=32u>>(sa/4u);kind=7;}
            if(sa==17u||sa==21u||sa==25u){bits=32u>>((sa-16u)/4u);kind=8;}
        }
        if(kind) {
            if(!rd)return 1;
            emit(ctx,enc_addi(4,3,REG_HI(rs)));emit(ctx,enc_addi(5,3,REG_HI(rt)));emit(ctx,enc_addi(6,3,REG_HI(rd)));
            emit(ctx,enc_addi(7,0,128u/bits));size_t loop=ctx->used_words;
            emit(ctx,bits==32u?enc_lwz(8,4,0):(bits==16u?enc_lhz(8,4,0):enc_lbz(8,4,0)));
            emit(ctx,bits==32u?enc_lwz(9,5,0):(bits==16u?enc_lhz(9,5,0):enc_lbz(9,5,0)));
            if(kind!=6u&&kind!=8u&&bits!=32u){emit(ctx,bits==16u?enc_extsh(8,8):enc_extsb(8,8));emit(ctx,bits==16u?enc_extsh(9,9):enc_extsb(9,9));}
            if(kind==1u||kind==6u) {
                emit(ctx,mmi_cmpw(8,9));emit(ctx,enc_addi(10,0,0));emit(ctx,enc_bc(4,kind==1u?1:2,8));emit(ctx,enc_addi(10,0,-1));
            } else if(kind==2u||kind==3u) {
                emit(ctx,enc_or(10,8,8));emit(ctx,mmi_cmpw(8,9));emit(ctx,enc_bc(4,kind==2u?0:1,8));emit(ctx,enc_or(10,9,9));
            } else if(kind==7u) {
                emit_load_const32(ctx,11,bits==32u?0x80000000u:0xffff8000u);emit(ctx,enc_cmplw(9,11));
                size_t ordinary=ctx->used_words;emit(ctx,0);emit_load_const32(ctx,10,bits==32u?0x7fffffffu:0x7fffu);
                size_t done=ctx->used_words;emit(ctx,0);mmi_patch_bc(ctx,ordinary,4,2);
                emit(ctx,enc_srawi(11,9,31));emit(ctx,enc_xor(10,9,11));emit(ctx,enc_subf(10,11,10));mmi_patch_b(ctx,done);
            } else {
                emit(ctx,kind==4u?enc_add(10,8,9):enc_subf(10,9,8));
                if(kind==8u){emit(ctx,enc_cmplw(8,9));emit(ctx,enc_bc(4,0,8));emit(ctx,enc_addi(10,0,0));}
                else if(bits==32u) {
                    emit(ctx,enc_xor(11,8,9));if(kind==4u)emit(ctx,enc_nor(11,11,11));
                    emit(ctx,enc_xor(12,8,10));emit(ctx,enc_and(11,11,12));emit(ctx,mmi_cmpi(11,0));
                    size_t okay=ctx->used_words;emit(ctx,0);emit(ctx,enc_srawi(11,8,31));emit_load_const32(ctx,12,0x7fffffffu);
                    emit(ctx,enc_xor(10,11,12));mmi_patch_bc(ctx,okay,4,0);
                } else {
                    int16_t hi=bits==16u?32767:127,lo=bits==16u?-32768:-128;
                    emit(ctx,mmi_cmpi(10,hi));emit(ctx,enc_bc(4,1,8));emit(ctx,enc_addi(10,0,hi));
                    emit(ctx,mmi_cmpi(10,lo));emit(ctx,enc_bc(4,0,8));emit(ctx,enc_addi(10,0,lo));
                }
            }
            emit(ctx,bits==32u?enc_stw(10,6,0):(bits==16u?enc_sth(10,6,0):enc_stb(10,6,0)));
            emit(ctx,enc_addi(4,4,bits/8));emit(ctx,enc_addi(5,5,bits/8));emit(ctx,enc_addi(6,6,bits/8));
            emit(ctx,enc_addi(7,7,-1));emit(ctx,mmi_cmpi(7,0));emit(ctx,enc_bc(4,2,((int32_t)loop-(int32_t)ctx->used_words)*4));
            return 1;
        }
        if(fn==40u&&sa==4u) {
            if(rd)for(unsigned n=0;n<8;n++) {
                emit(ctx,enc_lhz(4,3,mmi_h_off(rs,n)));emit(ctx,enc_lhz(5,3,mmi_h_off(rt,n)));
                emit(ctx,n<4u?enc_subf(6,5,4):enc_add(6,4,5));emit(ctx,enc_sth(6,3,mmi_h_off(rd,n)));
            }
            return 1;
        }
        if(fn==8u&&(sa==30u||sa==31u)) {
            if(rd)for(unsigned n=0;n<4;n++) {
                emit(ctx,enc_lwz(4,3,mmi_w_off(rt,n)));emit(ctx,enc_addi(5,0,0));
                for(unsigned field=0;field<4;field++) {
                    unsigned start=field==3u?15u:field*5u,width=field==3u?1u:5u,shift=field==3u?16u:3u+field*3u;
                    emit(ctx,sa==30u?enc_rlwinm(6,4,shift,32u-start-width-shift,31u-start-shift):
                                     enc_rlwinm(6,4,32u-shift,32u-start-width,31u-start));
                    emit(ctx,enc_or(5,5,6));
                }
                emit(ctx,enc_stw(5,3,mmi_w_off(rd,n)));
            }
            return 1;
        }
    }
    if(fn==9u&&(sa==0u||sa==4u)) {
        for(unsigned pipe=0;pipe<2;pipe++) {
            int lane=pipe*2;
            emit(ctx,enc_lwz(4,3,mmi_w_off(rs,lane)));emit(ctx,enc_lwz(5,3,mmi_w_off(rt,lane)));
            emit(ctx,enc_mullw(6,4,5));emit(ctx,enc_mulhw(7,4,5));
            emit(ctx,enc_lwz(8,3,mmi_w_off(LO_IDX,lane)));emit(ctx,enc_lwz(9,3,mmi_w_off(HI_IDX,lane)));
            emit(ctx,sa==0u?enc_add(8,8,6):enc_subf(8,6,8));emit(ctx,enc_srawi(10,8,31));
            emit(ctx,enc_stw(8,3,mmi_w_off(LO_IDX,lane)));emit(ctx,enc_stw(10,3,mmi_w_off(LO_IDX,lane+1)));
            if(rd)emit(ctx,enc_stw(8,3,mmi_w_off(rd,lane)));
            if(sa==0u)emit(ctx,enc_add(7,7,9));
            else {emit(ctx,enc_addi(8,0,0));emit(ctx,enc_subfc(6,6,8));emit(ctx,enc_subfe(7,7,9));}
            if(sa==0u&&!pipe) {
                emit(ctx,enc_cmplw(4,5));size_t equal=ctx->used_words;emit(ctx,0);
                emit(ctx,enc_rlwinm(8,5,0,1,31));emit(ctx,mmi_cmpi(8,0));size_t correction=ctx->used_words;emit(ctx,0);
                emit_load_const32(ctx,9,0x7fffffffu);emit(ctx,enc_cmplw(8,9));size_t no=ctx->used_words;emit(ctx,0);
                mmi_patch_bc(ctx,correction,12,2);emit_load_const32(ctx,8,0x70000000u);emit(ctx,enc_addi(9,0,0));
                emit(ctx,enc_addc(6,6,8));emit(ctx,enc_adde(7,7,9));mmi_patch_bc(ctx,equal,12,2);mmi_patch_bc(ctx,no,4,2);
            }
            /* abs(X)=a*2^32+b => abs(X)/(2^32-1)=a+[a+b>=2^32-1]. */
            emit(ctx,enc_srawi(8,7,31));emit(ctx,enc_xor(6,6,8));emit(ctx,enc_xor(7,7,8));
            emit(ctx,enc_subfc(6,8,6));emit(ctx,enc_subfe(7,8,7));
            emit(ctx,enc_addc(9,6,7));emit(ctx,enc_subfe(10,10,10));emit(ctx,enc_nor(10,10,10));
            emit(ctx,enc_andi_dot(10,10,1));emit(ctx,mmi_cmpi(9,-1));emit(ctx,enc_bc(4,2,8));emit(ctx,enc_addi(10,0,1));
            emit(ctx,enc_add(7,7,10));emit(ctx,enc_xor(7,7,8));emit(ctx,enc_subf(7,8,7));emit(ctx,enc_srawi(8,7,31));
            emit(ctx,enc_stw(7,3,mmi_w_off(HI_IDX,lane)));emit(ctx,enc_stw(8,3,mmi_w_off(HI_IDX,lane+1)));
            if(rd)emit(ctx,enc_stw(7,3,mmi_w_off(rd,lane+1)));
        }
        return 1;
    }
    return 0;
}
