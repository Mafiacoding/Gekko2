#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Direct include exposes the static lower interpreter for an exact unit test;
 * tests/run_test.sh automatically excludes source/hw/vu.c from the link. */
#include "hw/vu.c"

static uint32_t rword(unsigned bc,unsigned rs,unsigned rt,unsigned elem)
{
    /* SPECIAL2 lower word: fdslot=REG_R group, low funct 0x3c|bc.
     * bits22:21 select Fsf for RINIT/RXOR; keep all destination lanes set
     * for RNEXT/RGET while choosing elem=3 in the main vector checks. */
    unsigned dest = elem==3u ? 0xfu : (elem & 3u);
    return (0x40u<<25)|(dest<<21)|(rt<<16)|(rs<<11)|
           (VULS_FD_R_GROUP<<6)|(0x3cu|(bc&3u));
}

int main(void)
{
    uint32_t vf[32][4]; uint32_t vi[32];
    uint8_t mem[64]; uint32_t bd=0,bt=0;
    memset(vf,0,sizeof(vf));memset(vi,0,sizeof(vi));memset(mem,0,sizeof(mem));

    vf[3][3]=0x41234567u;
    if(!vu_exec_lower(vf,vi,mem,63u,rword(2,3,4,3),0,&bd,&bt))return 1;
    uint32_t expect=(vf[3][3]&0x007fffffu)|0x3f800000u;
    if(vi[20]!=expect){fprintf(stderr,"RINIT got %08x expected %08x\n",vi[20],expect);return 2;}

    uint32_t old=vi[20];
    vf[3][3]=0x40abcdefu;
    if(!vu_exec_lower(vf,vi,mem,63u,rword(3,3,4,3),0,&bd,&bt))return 3;
    expect=((vf[3][3]^old)&0x007fffffu)|0x3f800000u;
    if(vi[20]!=expect){fprintf(stderr,"RXOR got %08x expected %08x\n",vi[20],expect);return 4;}

    old=vi[20];
    if(!vu_exec_lower(vf,vi,mem,63u,rword(1,3,4,3),0,&bd,&bt))return 5;
    for(unsigned l=0;l<4;l++)if(vf[4][l]!=old){fprintf(stderr,"RGET lane %u mismatch\n",l);return 6;}

    old=vi[20];
    expect=((old<<1)^((old>>4)&1u)^((old>>22)&1u));
    expect=(expect&0x007fffffu)|0x3f800000u;
    if(!vu_exec_lower(vf,vi,mem,63u,rword(0,3,4,3),0,&bd,&bt))return 7;
    if(vi[20]!=expect){fprintf(stderr,"RNEXT got %08x expected %08x\n",vi[20],expect);return 8;}
    for(unsigned l=0;l<4;l++)if(vf[4][l]!=expect){fprintf(stderr,"RNEXT lane %u mismatch\n",l);return 9;}

    /* PCSX2 suppresses the entire R op when Ft is VF0, including REG_R. */
    old=vi[20];vf[3][3]^=0x00123456u;
    if(!vu_exec_lower(vf,vi,mem,63u,rword(2,3,0,3),0,&bd,&bt))return 10;
    if(vi[20]!=old){fprintf(stderr,"Ft=0 RINIT changed REG_R\n");return 11;}

    puts("R1326 VU REG_R LFSR semantics: PASS");
    return 0;
}
