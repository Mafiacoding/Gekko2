#include <stdio.h>
#include <stdint.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/vif.h"
static void step(ee_state_t *s,uint32_t iw){
 s->pc=0xa0004000;s->next_pc=s->pc+4;s->cop0[12]=4;
 for(unsigned n=0;n<4;n++)s->ram[0x4000+n]=(uint8_t)(iw>>(8*n));
 ee_core_step();
}
static uint32_t co(unsigned mask,unsigned ft,unsigned fs,unsigned fd,unsigned fn){
 return (0x12u<<26)|((0x10u|mask)<<21)|(ft<<16)|(fs<<11)|(fd<<6)|fn;
}
int main(void){
 bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t *s=ee_core_get_state();
 const uint32_t vals[]={0x12345678,0x80000001,0xaabbccdd,0x3f800000};
 s->cop2_ctrl[10]=3;
 for(unsigned n=0;n<4;n++)s->vu0_vf[1][n]=vals[n];
 step(s,co(15,10,1,13,0x3d)); /* VSQI vf1,(vi10++) */
 for(unsigned n=0;n<16;n++)if(s->vu0_mem[48+n]!=(uint8_t)(vals[n/4]>>(8*(n%4))))return puts("FAIL VSQI raw bytes"),1;
 if(s->cop2_ctrl[10]!=4)return 1;
 /* VIF0-originated lane values must be read unchanged by VLQI. */
 for(unsigned n=0;n<4;n++)vu0_mem_write32(s,96+n*4,vals[n]);
 s->cop2_ctrl[11]=6;step(s,co(15,2,11,13,0x3c));
 for(unsigned n=0;n<4;n++)if(s->vu0_vf[2][n]!=vals[n])return puts("FAIL VIF/macro lane"),1;
 if(s->cop2_ctrl[11]!=7)return 1;
 /* Predecrement counterparts preserve the same bytes. */
 s->cop2_ctrl[10]=9;step(s,co(15,10,1,13,0x3f));
 for(unsigned n=0;n<16;n++)if(s->vu0_mem[128+n]!=(uint8_t)(vals[n/4]>>(8*(n%4))))return puts("FAIL VSQD raw bytes"),1;
 s->cop2_ctrl[11]=9;step(s,co(15,3,11,13,0x3e));
 for(unsigned n=0;n<4;n++)if(s->vu0_vf[3][n]!=vals[n])return puts("FAIL VLQD lane"),1;
 s->cop2_ctrl[10]=12;s->cop2_ctrl[11]=0x1234;
 step(s,co(8,11,10,15,0x3f));
 if(s->vu0_mem[192]!=0x34||s->vu0_mem[193]!=0x12||s->vu0_mem[194]||s->vu0_mem[195])return puts("FAIL VISWR bytes"),1;
 puts("PASS VU0 macro/VIF byte-order integration");return 0;
}
