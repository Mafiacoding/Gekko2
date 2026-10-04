#include "core/hw/vu.h"
#include "core/recompiler/vu_jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int errors;
#define CHECK(x) do{if(!(x)){printf("FAIL: %s line %d\n",#x,__LINE__);errors++;}}while(0)
static void word(uint8_t*p,uint32_t w){p[0]=w;p[1]=w>>8;p[2]=w>>16;p[3]=w>>24;}
static uint32_t bits(float f){union{float f;uint32_t u;}v={.f=f};return v.u;}
int main(void){
 uint32_t vf[32][4]={{0}},vi[32]={0},acc[4]={0};uint8_t mem[4096]={0},micro[4096]={0};
 uint32_t pc=0,delay=0,target=0,ebit=0;uint64_t count=0,unknown=0;
 vf[0][3]=bits(1);for(int l=0;l<4;l++){vf[1][l]=bits(2);vf[2][l]=bits(3);acc[l]=bits(10);}
 /* I-data is loaded before ADDi; E stops only after the next pair. */
 word(micro,bits(3));word(micro+4,0x80000000u|(15u<<21)|(1u<<11)|(2u<<6)|0x22u);
 word(micro+8,(0x40u<<25)|(1u<<16)|(1u<<11)|(2u<<6)|0x30u);
 word(micro+12,0x40000000u|0x2ffu); /* NOP + E */
 word(micro+16,(0x40u<<25)|(2u<<16)|(1u<<11)|(3u<<6)|0x30u);
 word(micro+20,(15u<<21)|(2u<<16)|(1u<<11)|(4u<<6)|0x2au);
 vi[1]=7;
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==0);
 CHECK(vi[21]==bits(3)&&vf[2][0]==bits(5)&&vi[1]==7);
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==0);
 CHECK(vi[2]==14&&ebit==1);
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==1);
 CHECK(vi[3]==21&&vf[4][0]==bits(10)&&count==3&&unknown==0&&pc==24);
 /* JALR captures a target before overwriting its aliased VI link. */
 vi[1]=9;pc=0;delay=target=ebit=0;
 word(micro,(0x25u<<25)|(1u<<16)|(1u<<11));word(micro+4,0x2ffu);
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==0);
 CHECK(vi[1]==2&&target==72&&delay==1&&pc==8);
 word(micro+8,(0x40u<<25)|(4u<<16)|(3u<<11)|(2u<<6)|0x30u);word(micro+12,0x2ffu);
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==0);
 CHECK(pc==72&&vi[2]==21&&delay==0);
 /* MULA and OPMSUB use the real upper-table encodings. */
 pc=0;word(micro,0x8000033cu);word(micro+4,(15u<<21)|(2u<<16)|(1u<<11)|0x2beu);
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==0);
 CHECK(acc[0]==bits(10));
 pc=0;word(micro+4,(15u<<21)|(2u<<16)|(1u<<11)|(1u<<6)|0x2eu);
 CHECK(vu_micro_step(vf,vi,acc,mem,4095,micro,4095,&pc,&delay,&target,&ebit,&count,&unknown)==0);
 CHECK(vf[1][0]==bits(0)&&vf[1][1]==bits(0)&&vf[1][2]==bits(0)&&vf[1][3]==bits(2)&&unknown==0);
 printf("%s VU pair I/E/branch scheduling, new upper decode, aliases\n",errors?"FAIL":"PASS");return errors!=0;
}
