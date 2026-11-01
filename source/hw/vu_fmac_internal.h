/* GPL-3.0-or-later. Flag issue/retirement follows the bundled PCSX2
 * VUops.cpp _vuAddFMACStalls/_vuFMACflush (PCSX2 Dev Team).
 * This scalar datapath uses host single precision, with PS2 range handling;
 * hardware-specific mantissa truncation is not asserted here. */
/* PCSX2 VUops.cpp::vuADD: the VU discards an addend separated by
 * at least 25 exponent steps. Host IEEE addition alone can round to
 * a different adjacent value for opposite signs near a power of two.
 * This is one alignment rule, not a complete VU mantissa model. */
static float vu_fmac_add(uint32_t a,uint32_t b,int subtract) {
    int distance=(int)((a>>23)&255u)-(int)((b>>23)&255u);
    if(distance>=25)b&=0x80000000u;
    if(distance<=-25)a&=0x80000000u;
    if(subtract)b^=0x80000000u;
    return vu_f(vu_scalar_bits(a))+vu_f(vu_scalar_bits(b));
}
/* Untimed scalar/native diagnostic paths retain their fast arithmetic for
 * ordinary operands; only ADD/SUB alignment edge cases need the shared
 * helper. Timed production execution already uses that helper directly. */
static int vu_fmac_add_alignment_needed(uint32_t vf[32][4],uint32_t *vi,uint32_t w) {
    unsigned f=w&63u,sub=(w>>6)&31u,bc=w&3u,scalar=0;
    if(f<8u)scalar=1;
    else if(f>=32u&&f<=39u&&!(f&1u))scalar=(f&2u)?3u:2u;
    else if(f==40u||f==44u){}
    else if(f>=60u){
        if(sub<=1u)scalar=1;
        else if((sub==8u||sub==9u)&&!(bc&1u))scalar=(bc&2u)?3u:2u;
        else if((sub==10u||sub==11u)&&bc==0u){}
        else return 0;
    }else return 0;
    unsigned fs=(w>>11)&31u,ft=(w>>16)&31u,mask=(w>>21)&15u;
    for(unsigned l=0;l<4;l++)if(mask&(8u>>l)){
        uint32_t a=fs?vf[fs][l]:(l==3u?0x3f800000u:0);
        uint32_t b=scalar==2u?vi[22]:scalar==3u?vi[21]:
            (ft?vf[ft][scalar==1u?bc:l]:((scalar==1u?bc:l)==3u?0x3f800000u:0));
        int distance=(int)((a>>23)&255u)-(int)((b>>23)&255u);
        if(distance>=25||distance<=-25)return 1;
    }
    return 0;
}
static void vu_fmac_publish(vu_timing_t *t,uint32_t *vi,uint64_t cycle) {
    while(t&&t->count&&t->flags[t->head].ready<=cycle) {
        unsigned n=t->head;uint32_t s=t->flags[n].status;
        if(!(t->flags[n].set&4u)) {
        if(t->flags[n].set&1u)vi[16]=(vi[16]&0x30u)|(s&0xfcfu);
        else vi[16]=(vi[16]&0xff0u)|(s&15u)|((s&15u)<<6);
        vi[17]=t->flags[n].mac;
        }
        if(t->flags[n].set&2u)vi[18]=t->flags[n].clip;
        t->head=(t->head+1u)&3u;t->count--;
    }
}
static void vu_fmac_issue(vu_timing_t *t,uint32_t *vi,vu_pipeline_t *p,
                          uint32_t mac,uint32_t status,unsigned set,uint32_t clip) {
    if(!t||!p) {
        if(!(set&4u)) {
        vi[16]=(set&1u)?((vi[16]&0x30u)|(status&0xfcfu)):
            ((vi[16]&0xff0u)|(status&15u)|((status&15u)<<6));
        vi[17]=mac;
        }
        if(set&2u)vi[18]=clip;
        return;
    }
    if(t->count==4u) {
        if(p->cycle<t->flags[t->head].ready)p->cycle=t->flags[t->head].ready;
        vu_fmac_publish(t,vi,p->cycle);
    }
    unsigned n=(t->head+t->count)&3u;
    t->flags[n].ready=p->cycle+4u;t->flags[n].mac=mac;
    t->flags[n].clip=clip;
    t->flags[n].status=status;t->flags[n].set=(uint8_t)set;t->count++;
}
/* Consecutive CLIPs shift the latest issued history, while lower queries
 * observe only the four-cycle retired value. Entries with other flag writes
 * must not hide an earlier CLIP still in flight. */
static uint32_t vu_clip_issued(const vu_timing_t *t,uint32_t visible) {
    for(unsigned i=t?t->count:0;i;i--) {
        unsigned n=(t->head+i-1u)&3u;
        if(t->flags[n].set&2u)return t->flags[n].clip;
    }
    return visible;
}
/* Integer ALU/MTIR and auto-update addresses feed conditional branches
 * through the preceding VI value for one issue cycle. Other VI readers,
 * including JR/JALR, use the current value. Flag queries and ILW do not
 * participate in this bypass (PCSX2 VUops _vuBackupVI call sites). */
static unsigned vu_vi_backup_dest(uint32_t w) {
    unsigned op=w>>25,fn=w&63u,sub=(w>>6)&31u;
    if(op==8u||op==9u)return (w>>16)&15u;
    if(op!=64u)return 0;
    if(fn==48u||fn==49u||fn==52u||fn==53u)return sub&15u;
    if(fn==50u)return (w>>16)&15u;
    if(fn>=60u&&sub==13u)return (w>>((w&1u)?16:11))&15u;
    if(fn>=60u&&sub==15u&&(w&3u)==0u)return (w>>16)&15u;
    return 0;
}
static void vu_vi_age(vu_timing_t *t,uint64_t clocks) {
    if(!t)return;
    if(t->vi_marker!=0x56u||t->vi_age>2u||t->vi_reg>15u) {
        t->vi_age=0;t->vi_marker=0x56u;
    }
    t->vi_age=clocks>=t->vi_age?0:t->vi_age-(unsigned)clocks;
}
static int vu_vi_conditional_branch(uint32_t w) {
    unsigned op=w>>25;
    return op==40u||op==41u||(op>=44u&&op<=47u);
}
/* Returns one for an arithmetic upper instruction, including VF00 writes.
 * No dependency on the host backend: interpreter/AOT/JIT pairs share this
 * path whenever flag timing is required. MAX/MINI, conversions, ABS and
 * CLIP do not overwrite arithmetic flags. */
static inline __attribute__((always_inline)) int vu_fmac_arithmetic(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],
                              uint32_t w,uint32_t *mac_out,uint32_t *status_out) {
    unsigned fn=w&63u,sub=(w>>6)&31u,bc=w&3u,mask=(w>>21)&15u;
    unsigned fs=(w>>11)&31u,ft=(w>>16)&31u,fd=(w>>6)&31u;
    int op=-1,scalar=0,to_acc=0,cross=0;
    /* op: ADD, SUB, MUL, MADD, MSUB. scalar: 1 broadcast, 2 Q, 3 I. */
    if(fn<16u){op=(fn>>2)==0u?0:(fn>>2)==1u?1:(fn>>2)==2u?3:4;scalar=1;}
    else if(fn>=24u&&fn<=27u){op=2;scalar=1;}
    else if(fn==28u||fn==30u){op=2;scalar=fn==28u?2:3;}
    else if(fn>=32u&&fn<=39u){op=(fn&4u)?((fn&1u)?4:1):((fn&1u)?3:0);scalar=(fn&2u)?3:2;}
    else if(fn==40u)op=0;
    else if(fn==41u)op=3;
    else if(fn==42u)op=2;
    else if(fn==44u)op=1;
    else if(fn==45u)op=4;
    else if(fn==46u){op=4;cross=1;mask=14u;}
    else if(fn>=60u) {
        to_acc=1;
        if(sub<=3u){op=sub==0?0:sub==1?1:sub==2?3:4;scalar=1;}
        else if(sub==6u){op=2;scalar=1;}
        else if(sub==8u||sub==9u){op=sub==8u?(bc&1u?3:0):(bc&1u?4:1);scalar=bc&2u?3:2;}
        else if(sub==10u&&bc<3u)op=bc==0?0:bc==1?3:2;
        else if(sub==11u&&bc<3u){op=bc==0?1:bc==1?4:2;if(bc==2){cross=1;mask=14u;}}
        else if(sub==7u&&(bc==0u||bc==2u)){op=2;scalar=bc==0u?2:3;}
    }
    if(op<0)return 0;
    uint32_t result[4],mac=0;unsigned status=0;
    for(unsigned l=0;l<4;l++)if(mask&(8u>>l)) {
        unsigned al=cross?(l+1u)%3u:l,bl=cross?(l+2u)%3u:l;
        uint32_t ab=fs?vf[fs][al]:(al==3u?0x3f800000u:0);
        uint32_t bb=scalar==2?vi[22]:scalar==3?vi[21]:
            (ft?vf[ft][scalar==1?bc:bl]:((scalar==1?bc:bl)==3u?0x3f800000u:0));
        float a=vu_f(vu_scalar_bits(ab)),b=vu_f(vu_scalar_bits(bb));
        float product=a*b,c=vu_f(vu_scalar_bits(acc[l])),r;
        /* Separate multiplication/addition, never an implicit fused FMA. */
        volatile float rounded_product=product;
        r=op<=1?vu_fmac_add(ab,bb,op==1):op==2?product:op==3?c+rounded_product:c-rounded_product;
        uint32_t bits=vu_u(r),exp=bits&0x7f800000u;unsigned lane_flag=0;
        if(bits&0x80000000u)lane_flag|=2u;
        if(exp==0x7f800000u){lane_flag|=8u;bits=(bits&0x80000000u)|0x7f7fffffu;}
        else if(exp==0u){if(bits&0x7fffffu)lane_flag|=4u;bits&=0x80000000u;lane_flag|=1u;}
        result[l]=bits;status|=lane_flag;
        for(unsigned f=0;f<4;f++)if(lane_flag&(1u<<f))mac|=(8u>>l)<<(4u*f);
    }
    /* All sources are captured before overlapping destination writes. */
    for(unsigned l=0;l<4;l++)if(mask&(8u>>l)) {
        if(to_acc)acc[l]=result[l];else if(fd)vf[fd][l]=result[l];
    }
    *mac_out=mac;*status_out=status;return 1;
}

/* RAW scoreboarding uses the shared dependency decoder. Values are kept
 * canonical immediately; a dependent pair waits for the producing issue
 * clock before either half executes or sees matured flags. */
static void vu_fmac_stall(vu_timing_t *t,vu_pipeline_t *p,uint32_t upper,uint32_t lower) {
    if(!t||!p)return;
    unsigned fn=upper&63u,sub=(upper>>6)&31u,bc=upper&3u;
    unsigned mask=(upper>>21)&15u,fs=(upper>>11)&31u,ft=(upper>>16)&31u;
    unsigned sm=mask,tm=mask;
    if(fn==46u){sm=tm=14u;}
    else if(fn<28u)tm=mask?8u>>(fn&3u):0;
    else if(fn>=28u&&fn<=39u)tm=0;
    else if(fn>=60u) {
        if(sub==11u&&bc==3u)sm=tm=0;
        else if(sub==7u&&bc==3u){sm=14u;tm=1u;}
        else if(sub<=3u||sub==6u)tm=mask?8u>>bc:0;
        else if(sub==4u||sub==5u||sub==7u||sub==8u||sub==9u)tm=0;
        else if(sub==11u&&bc==2u)sm=tm=14u;
    }
    for(unsigned l=0;l<4;l++) {
        if(fs&&(sm&(8u>>l))&&p->cycle<t->vf_ready[fs][l])p->cycle=t->vf_ready[fs][l];
        if(ft&&(tm&(8u>>l))&&p->cycle<t->vf_ready[ft][l])p->cycle=t->vf_ready[ft][l];
    }
    if(!(upper&0x80000000u))for(unsigned r=1;r<32;r++)if(vu_pair_lower_reads(lower,r))
        for(unsigned l=0;l<4;l++)if(p->cycle<t->vf_ready[r][l])p->cycle=t->vf_ready[r][l];
}
static void vu_fmac_mark_write(vu_timing_t *t,vu_pipeline_t *p,unsigned dst,unsigned mask) {
    if(!t||!p||!dst)return;
    for(unsigned l=0;l<4;l++)if(mask&(8u>>l))t->vf_ready[dst][l]=p->cycle+4u;
}
