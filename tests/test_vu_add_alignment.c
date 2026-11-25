/* Exact boundary vectors for timed VU ADD/SUB families, not game assets. */
#include <assert.h>
#include <stdio.h>
#include "hw/vu.c"
int main(void){
 const struct{unsigned encoding,acc,sub;} ops[]={
  {(3<<6)|40,0,0},{(3<<6)|0,0,0},{(3<<6)|32,0,0},{(3<<6)|34,0,0},
  {(3<<6)|44,0,1},{(3<<6)|4,0,1},{(3<<6)|36,0,1},{(3<<6)|38,0,1},
  {(10<<6)|60,1,0},{60,1,0},{(8<<6)|60,1,0},{(8<<6)|62,1,0},
  {(11<<6)|60,1,1},{(1<<6)|60,1,1},{(9<<6)|60,1,1},{(9<<6)|62,1,1}};
 const struct{uint32_t a,b,add,sub;} cases[]={
  {0x3f800000,0xb37fffff,0x3f800000,0x3f800000},
  {0xb37fffff,0x3f800000,0x3f800000,0xbf800000},
  {0xbf800000,0x337fffff,0xbf800000,0xbf800000},
  {0x3f800000,0xb3ffffff,0x3f7ffffe,0x3f800001}};
 unsigned checks=0;
 for(unsigned op=0;op<16;op++)for(unsigned mask=1;mask<16;mask++)for(unsigned c=0;c<4;c++){
  uint32_t vf[32][4]={{0}},vi[32]={0},acc[4],mac,status;
  for(unsigned l=0;l<4;l++){vf[1][l]=cases[c].a;vf[2][l]=cases[c].b;vf[3][l]=acc[l]=0x3e800000;}
  vi[21]=vi[22]=cases[c].b;
  unsigned iw=ops[op].encoding|(mask<<21)|(2<<16)|(1<<11);
  assert(vu_fmac_arithmetic(vf,vi,acc,iw,&mac,&status));
  uint32_t expected=ops[op].sub?cases[c].sub:cases[c].add;
  for(unsigned l=0;l<4;l++){
   uint32_t want=mask&(8u>>l)?expected:0x3e800000;
   assert((ops[op].acc?acc[l]:vf[3][l])==want);checks++;
  }
  assert(status==((expected>>31)?2u:0u));assert(mac==((expected>>31)?mask<<4:0));checks+=2;
  for(unsigned l=0;l<4;l++)vf[3][l]=acc[l]=0x3e800000;
  uint8_t micro[16]={0},memory[16384]={0};uint32_t pc=0,bd=0,bt=0,ed=0;
  uint64_t retired=0,unknown=0;vu_pipeline_t pipeline={0};
  vu_wr_le32(micro+4,iw);
  vu_micro_step_pipeline(vf,vi,acc,memory,16383,micro,15,&pc,&bd,&bt,&ed,&retired,&unknown,&pipeline);
  assert(retired==1&&!unknown);
  for(unsigned l=0;l<4;l++){
   uint32_t want=mask&(8u>>l)?expected:0x3e800000;
   assert((ops[op].acc?acc[l]:vf[3][l])==want);checks++;
  }
 }
 printf("PASS %u VU ADD/SUB broadcast/Q/I/ACC alignment, lane-mask and flag checks\n",checks);return 0;
}
