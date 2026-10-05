#ifndef GEKKO2_VU_PAIR_H
#define GEKKO2_VU_PAIR_H
#include <stdint.h>
/* Conservative VF dependency description shared by scalar and PPC paths.
 * Unknown forms cannot enter native pairs. Special ACC/CLIP forms have
 * no VF destination; ABS and conversions use the Ft field. */
static inline unsigned vu_pair_upper_dest(uint32_t w) {
    unsigned fn=w&63u;
    if(!((w>>21)&15u)&&fn!=46u)return 0;
    if(fn>=60u) {
        unsigned sub=(w>>6)&31u,bc=w&3u;
        return (sub==4u||sub==5u||(sub==7u&&bc==1u))?(w>>16)&31u:0;
    }
    return fn<=47u?(w>>6)&31u:0;
}
static inline unsigned vu_pair_lower_dest(uint32_t w) {
    unsigned op=w>>25,sub=(w>>6)&31u,fn=w&63u;
    if(op==0u)return (w>>16)&31u;
    if(op==64u&&fn>=60u&&
       ((sub==12u&&(w&3u)<=1u)||(sub==13u&&!(w&1u))||
        (sub==15u&&(w&3u)==1u)||(sub==25u&&(w&3u)==0u)))return (w>>16)&31u;
    return 0;
}
static inline int vu_pair_lower_reads(uint32_t w,unsigned vf) {
    if(!vf)return 0;
    unsigned op=w>>25,sub=(w>>6)&31u,bc=w&3u;
    if(op==1u)return ((w>>11)&31u)==vf;
    if(op!=64u||(w&63u)<60u)return 0;
    if(sub==12u||(sub==15u&&bc==0u))return ((w>>11)&31u)==vf;
    if(sub==13u&&(bc&1u))return ((w>>16)&31u)==vf;
    if(sub==14u&&bc<3u)return (bc!=1u&&((w>>11)&31u)==vf)||((w>>16)&31u)==vf;
    if(sub>=28u&&!(sub==30u&&bc==3u))return ((w>>11)&31u)==vf;
    return 0;
}
static inline int vu_pair_vf_conflict(uint32_t upper,uint32_t lower) {
    if(upper&0x80000000u)return 0;
    unsigned dst=vu_pair_upper_dest(upper);
    return dst&&(vu_pair_lower_dest(lower)==dst||vu_pair_lower_reads(lower,dst));
}
#endif
