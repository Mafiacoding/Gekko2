#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
int main(void) {
    bios_image_t bios={0};
    if(system_init(&bios,&bios)) return 2;
    ee_state_t *s=ee_core_get_state(),base=*s;
    const uint32_t values[]={0,1,0x7fffffff,0x80000000,0xffffffff};
    unsigned checks=0;
    for(unsigned bank=0;bank<2;bank++) for(unsigned uns=0;uns<2;uns++)
    for(unsigned a=0;a<5;a++) for(unsigned b=0;b<5;b++) {
        uint32_t x=values[a],y=values[b],q,r;
        if(!y){q=uns||!(x&0x80000000u)?0xffffffffu:1;r=x;}
        else if(!uns&&x==0x80000000u&&y==0xffffffffu){q=x;r=0;}
        else if(uns){q=x/y;r=x%y;}
        else {q=(uint32_t)((int64_t)(int32_t)x/(int32_t)y);r=(uint32_t)((int64_t)(int32_t)x%(int32_t)y);}
        *s=base;s->gpr[1].ud0=x;s->gpr[2].ud0=y;
        s->hi.ud0=s->hi.ud1=0x1122334455667788ULL;
        s->lo.ud0=s->lo.ud1=0x8877665544332211ULL;
        uint32_t iw=(bank?0x1cu<<26:0)|(1u<<21)|(2u<<16)|(0x1au+uns);
        s->pc=0xa0004000;s->next_pc=s->pc+4;s->cop0[12]=4;
        for(unsigned k=0;k<4;k++)s->ram[0x4000+k]=(uint8_t)(iw>>(8*k));
        ee_core_step();
        uint64_t gotq=bank?s->lo.ud1:s->lo.ud0,gotr=bank?s->hi.ud1:s->hi.ud0;
        if(gotq!=(uint64_t)(int64_t)(int32_t)q||gotr!=(uint64_t)(int64_t)(int32_t)r||
           (bank?s->lo.ud0:s->lo.ud1)!=0x8877665544332211ULL||
           (bank?s->hi.ud0:s->hi.ud1)!=0x1122334455667788ULL){
            printf("FAIL bank=%u unsigned=%u rs=%08x rt=%08x\n",bank,uns,x,y);return 1;
        }
        checks++;
    }
    printf("PASS %u DIV/DIVU/DIV1/DIVU1 cases\n",checks);return 0;
}
