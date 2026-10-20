/* Independent synthetic branch-bypass vectors; no proprietary microcode. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "core/hw/vu.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t vf[32][4],vi[32],acc[4],pc,bd,bt,ed;
static uint64_t retired,unknown;
static uint8_t mem[16384],micro[16384];
static vu_pipeline_t pipe;static vu_timing_t timing;
#define NOP 0x2ffu
static void reset(void){memset(vf,0,sizeof vf);memset(vi,0,sizeof vi);memset(acc,0,sizeof acc);memset(&pipe,0,sizeof pipe);memset(&timing,0,sizeof timing);memset(mem,0,sizeof mem);pc=bd=bt=ed=0;retired=unknown=0;vf[0][3]=0x3f800000;}
static void step(uint32_t up,uint32_t lo){for(unsigned i=0;i<4;i++){micro[pc+i]=lo>>(8*i);micro[pc+4+i]=up>>(8*i);}vu_micro_step_timed(vf,vi,acc,mem,16383,micro,16383,&pc,&bd,&bt,&ed,&retired,&unknown,&pipe,&timing);}
static uint32_t imm(unsigned reg,unsigned value){return 8u<<25|reg<<16|value;}
static uint32_t branch(unsigned op,unsigned s,unsigned t){return op<<25|s<<11|t<<16|5u;}
int main(void){
 CHECK(offsetof(vu_timing_t,vf_ready)==104);
 /* All six conditional branches use old VI; ordinary arithmetic sees new. */
 const unsigned op[]={40,41,44,45,46,47};
 const uint16_t old[]={0,1,0xffff,1,0,0};
 const uint16_t next[]={1,0,1,0xffff,1,0xffff};
 for(unsigned k=0;k<6;k++){
  reset();vi[1]=old[k];vi[2]=next[k];step(NOP,imm(1,0)|(2u<<11));CHECK(vi[1]==next[k]);
  step(NOP,branch(op[k],1,0));CHECK(bd==1&&vi[1]==next[k]&&bt==56);
  step(NOP,0);CHECK(pc==56&&bd==0&&unknown==0);
  reset();vi[1]=next[k];vi[2]=old[k];step(NOP,imm(1,0)|(2u<<11));
  step(NOP,branch(op[k],1,0));CHECK(bd==0&&vi[1]==old[k]);
 }
 /* Branch reads delayed It too, and a gap expires the bypass. */
 reset();step(NOP,imm(1,1));step(NOP,branch(40,0,1));CHECK(bd==1);
 reset();step(NOP,imm(1,1));step(NOP,0);step(NOP,branch(40,0,1));CHECK(bd==0);
 /* Consecutive writes to same VI retain the value before both writes. */
 reset();step(NOP,imm(1,1));step(NOP,imm(1,2));step(NOP,branch(40,1,0));CHECK(bd==1&&vi[1]==2);
 /* A write to a different VI replaces the bypass slot. */
 reset();step(NOP,imm(1,1));step(NOP,imm(2,2));step(NOP,branch(40,1,0));CHECK(bd==0);
 /* JR uses canonical VI; no conditional-branch bypass. */
 reset();step(NOP,imm(1,10));step(NOP,36u<<25|1u<<11);CHECK(bd==1&&bt==80&&vi[1]==10);
 /* A real VF dependency stall expires old VI before branch evaluation. */
 reset();step((8u<<21)|(1u<<11)|(2u<<6)|40u,imm(1,1));
 step((8u<<21)|(2u<<11)|(3u<<6)|40u,branch(40,1,0));CHECK(bd==0&&pipe.cycle>=5);
 /* Arithmetic, MTIR, and each auto-update address form start bypass. */
 const uint32_t writes[]={0x80000030u|(1u<<6)|(2u<<11),0x80000031u|(1u<<6)|(2u<<11),0x80000034u|(1u<<6)|(2u<<11)|(2u<<16),0x80000035u|(1u<<6)|(2u<<11),0x80000032u|(1u<<16)|(1u<<6),0x800003fcu|(1u<<16)|(1u<<11),0x8000037cu|(1u<<11)|(2u<<16),0x8000037du|(1u<<16)|(2u<<11),0x8000037eu|(1u<<11)|(2u<<16),0x8000037fu|(1u<<16)|(2u<<11),9u<<25|1u<<16|1};
 for(unsigned k=0;k<sizeof writes/sizeof writes[0];k++){
  reset();vi[2]=1;vf[1][0]=1;step(NOP,writes[k]);CHECK(timing.vi_reg==1&&timing.vi_age==2);
  step(NOP,branch(40,1,0));CHECK(bd==1);
 }
 /* ILW and flag queries use their current result, without ALU bypass. */
 reset();mem[0]=1;step(NOP,4u<<25|8u<<21|1u<<16);step(NOP,branch(40,1,0));CHECK(bd==0);
 reset();step(NOP,16u<<25);step(NOP,branch(40,1,0));CHECK(bd==0);
 /* Synthetic legacy padding cannot arm history without the marker. */
 reset();timing.vi_age=2;timing.vi_reg=1;timing.vi_old=1;step(NOP,branch(40,1,0));CHECK(bd==1);
 puts("PASS VI branch bypass: six conditions, sources, consecutive writes, stalls, writers and JR exclusion");return 0;
}
