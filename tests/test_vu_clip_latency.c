/* Synthetic CLIP/FCSET pipeline oracle: no game microcode or assets. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
#define CHECK(x) do { if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;} } while(0)
static uint32_t vf[32][4],vi[32],acc[4],pc,bd,bt,ed;
static uint64_t retired,unknown;
static uint8_t mem[16384],micro[16384];
static vu_pipeline_t pipeline;static vu_timing_t timing;
#define NOP 0x2ffu
#define CLIP ((2u<<16)|(1u<<11)|(7u<<6)|63u)
static void reset(void){memset(vf,0,sizeof vf);memset(vi,0,sizeof vi);memset(acc,0,sizeof acc);memset(&pipeline,0,sizeof pipeline);memset(&timing,0,sizeof timing);pc=bd=bt=ed=0;retired=unknown=0;vf[0][3]=0x3f800000;vf[1][0]=0x40000000;vf[2][3]=0x3f800000;}
static int step(uint32_t upper,uint32_t lower){for(unsigned i=0;i<4;i++){micro[pc+i]=lower>>(8*i);micro[pc+4+i]=upper>>(8*i);}return vu_micro_step_timed(vf,vi,acc,mem,16383,micro,16383,&pc,&bd,&bt,&ed,&retired,&unknown,&pipeline,&timing);}
int main(void){
 /* Stable checkpoint layout: new payload occupies previously unused padding. */
 CHECK(sizeof(timing.flags[0])==24);CHECK(offsetof(vu_timing_t,head)==96);
 reset();vi[18]=7;vi[16]=0x31;vi[17]=0x1234;
 step(CLIP,(16u<<25)|7);CHECK(vi[1]==1&&vi[18]==7);
 for(unsigned i=0;i<3;i++){step(NOP,(28u<<25)|(3u<<16));CHECK(vi[3]==7&&vi[18]==7);}
 step(NOP,(28u<<25)|(3u<<16));CHECK(vi[3]==449&&vi[18]==449);
 CHECK(vi[16]==0x31&&vi[17]==0x1234);
 /* Back-to-back history is accumulated at issue, queried at retirement. */
 reset();for(unsigned i=0;i<4;i++){step(CLIP,0);CHECK(vi[18]==0);}
 const uint32_t history[]={1,65,4161,266305};
 for(unsigned i=0;i<4;i++){step(NOP,(28u<<25)|(3u<<16));CHECK(vi[18]==history[i]&&vi[3]==(history[i]&4095));}
 /* Same-pair FCSET loses to CLIP; an intervening FCSET seeds next CLIP. */
 reset();step(CLIP,(17u<<25)|0xffffff);step(NOP,(17u<<25)|2);step(CLIP,0);
 step(NOP,0);step(NOP,0);CHECK(vi[18]==1);step(NOP,0);CHECK(vi[18]==2);step(NOP,0);CHECK(vi[18]==129);
 /* I-bit still issues upper CLIP; lower is immediate data, not FCSET. */
 reset();step(CLIP|0x80000000u,(17u<<25)|2);CHECK(vi[21]==((17u<<25)|2)&&vi[18]==0);
 for(unsigned i=0;i<4;i++)step(NOP,0);CHECK(vi[18]==1);
 /* CLIP-only entries cannot overwrite or conceal in-flight MAC/STATUS. */
 reset();vf[3][0]=0x3f800000;vf[4][0]=0xbf800000;
 step((8u<<21)|(4u<<16)|(3u<<11)|(5u<<6)|40u,0);
 step(CLIP,0);step(NOP,(21u<<25)|0x100);
 for(unsigned i=0;i<4;i++)step(NOP,0);
 CHECK(vi[17]==8&&(vi[16]&0xfcf)==0x101&&vi[18]==1);
 /* Arithmetic + FCSET occupy one issue slot and retire independently. */
 reset();step((8u<<21)|(1u<<11)|(3u<<6)|40u,(17u<<25)|123);
 for(unsigned i=0;i<4;i++)step(NOP,0);CHECK(vi[18]==123&&vi[17]==0);
 /* Real operand stall matures CLIP before a paired FCGET. */
 reset();vf[3][0]=0x3f800000;step(CLIP,0);
 step((8u<<21)|(3u<<11)|(4u<<6)|40u,0);
 step((8u<<21)|(4u<<11)|(5u<<6)|40u,(28u<<25)|(3u<<16));CHECK(vi[3]==1);
 /* E-bit delay slot drains the same queue before returning STOP. */
 reset();step(CLIP|0x40000000u,0);CHECK(step(NOP,0)==1&&vi[18]==1&&timing.count==0);
 CHECK(unknown==0);
 /* Actual checkpoint round trip preserves in-flight CLIP + VI history for
  * both units without moving the later EE/VU1 serialized fields. */
 bios_image_t bios={0};CHECK(!system_init(&bios,&bios));
 reset();step(CLIP,(8u<<25)|(1u<<16)|1u);
 vu_timing_t pending=timing;
 ee_state_t *ee=ee_core_get_state();vu1_state_t *vu=vu1_get_state();
 ee->vu0_timing=pending;vu->timing=pending;
 ee->fileio_rpc_version=0x12345678;vu->xgkick_deferred_addr=0x1230;vu->xgkick_deferred_stalls=0x23456789;
 char path[]="/tmp/gekko2-vu-timing-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);close(fd);
 CHECK(!checkpoint_save(path));memset(&ee->vu0_timing,0,sizeof pending);memset(&vu->timing,0,sizeof pending);
 CHECK(!checkpoint_load(path,&bios,&bios,NULL));unlink(path);
 CHECK(!memcmp(&ee->vu0_timing,&pending,sizeof pending)&&!memcmp(&vu->timing,&pending,sizeof pending));
 CHECK(ee->fileio_rpc_version==0x12345678&&vu->xgkick_deferred_addr==0x1230&&vu->xgkick_deferred_stalls==0x23456789);
 timing=vu->timing;for(unsigned i=0;i<4;i++)step(NOP,0);CHECK(vi[18]==1);
 puts("PASS CLIP: four-cycle visibility, issued history, FCSET/I/E, mixed flags and stalls");return 0;
}
