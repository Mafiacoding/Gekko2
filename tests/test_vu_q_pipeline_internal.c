#include <stdio.h>
#include <string.h>
#include "core/hw/vu.h"
#include "core/hw/gif.h"
#include "core/hw/ee_intc.h"
#include "core/ee/ee_core.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
static uint32_t vf[32][4],vi[32],acc[4],pc,bd,bt,ed;static uint64_t retired,unknown;
static uint8_t mem[16384],micro[16384];static vu_pipeline_t pipe;
static uint32_t bits(float v){union{float f;uint32_t u;}x;x.f=v;return x.u;}
static void put(unsigned off,uint32_t w){for(unsigned i=0;i<4;i++)micro[off+i]=w>>(i*8);}
static unsigned upper(unsigned dst,unsigned fs,unsigned ft,unsigned mask,unsigned fn){return mask<<21|ft<<16|fs<<11|dst<<6|fn;}
static unsigned lower(unsigned sub,unsigned fs,unsigned ft,unsigned fields,unsigned bc){return 0x80000000u|fields<<21|ft<<16|fs<<11|sub<<6|60u|bc;}
static void reset(void){memset(vf,0,sizeof(vf));memset(vi,0,sizeof(vi));memset(&pipe,0,sizeof(pipe));memset(micro,0,sizeof(micro));pc=bd=bt=ed=0;retired=unknown=0;vf[0][3]=bits(1);vi[22]=bits(11);}
static void step(uint32_t up,uint32_t lo){put(pc,lo);put(pc+4,up);vu_micro_step_pipeline(vf,vi,acc,mem,16383,micro,16383,&pc,&bd,&bt,&ed,&retired,&unknown,&pipe);}
#define NOP upper(11,0,0,0,63)
int main(void){
    for(unsigned fsf=0;fsf<4;fsf++)for(unsigned ftf=0;ftf<4;ftf++) {
        reset();for(unsigned n=0;n<4;n++){vf[1][n]=bits(12+n*4);vf[2][n]=bits(2+n);}
        step(NOP,lower(14,1,2,fsf|(ftf<<2),0));CHECK(vi[22]==bits(11));CHECK(pipe.q_ready==7);
        for(unsigned n=0;n<6;n++)step(NOP,0);CHECK(vi[22]==bits(11));
        step(upper(3,0,0,8,32),0);CHECK(vi[22]==bits((12.f+fsf*4)/(2+ftf)));CHECK(!pipe.q_pending);
    }
    reset();vf[1][2]=bits(18);vf[2][3]=bits(2);
    step(NOP,lower(14,1,2,2|(3<<2),0));
    /* WAITQ matures Q before its paired ADDq. */
    step(upper(3,0,0,8,32),lower(14,0,0,0,3));CHECK(vf[3][0]==bits(9));CHECK(pipe.cycle==8);CHECK(retired==2);
    reset();vf[2][1]=bits(-9);step(NOP,lower(14,0,2,1<<2,1));step(NOP,lower(14,0,0,0,3));CHECK(vi[22]==bits(3));CHECK((vi[16]&0x30)==0x10);
    reset();vf[1][0]=bits(12);vf[2][0]=bits(4);step(NOP,lower(14,1,2,0,2));CHECK(pipe.q_ready==13);step(NOP,lower(14,0,0,0,3));CHECK(vi[22]==bits(6));CHECK(pipe.cycle==14);
    reset();vf[1][0]=bits(-1);vf[2][0]=0;step(NOP,lower(14,1,2,0,0));step(NOP,lower(14,0,0,0,3));CHECK(vi[22]==0xff7fffffu);CHECK((vi[16]&0x30)==0x20);
    reset();vf[1][0]=vf[2][0]=0;step(NOP,lower(14,1,2,0,0));step(NOP,lower(14,0,0,0,3));CHECK(vi[22]==0x7f7fffffu);CHECK((vi[16]&0x30)==0x10);
    reset();vf[1][0]=bits(8);vf[2][0]=bits(2);step(NOP,lower(14,1,2,0,0));
    vf[1][0]=bits(15);step(NOP,lower(14,1,2,0,0));CHECK(vi[22]==bits(4));CHECK(pipe.q_ready==14);step(NOP,lower(14,0,0,0,3));CHECK(vi[22]==bits(7.5));
    /* Lower MOVE reads pre-pair VF; same-target LQI is discarded fully. */
    reset();vf[1][0]=bits(2);vf[2][0]=bits(3);vf[3][0]=bits(99);
    step(upper(3,1,2,8,40),lower(12,3,4,8,0));CHECK(vf[3][0]==bits(5));CHECK(vf[4][0]==bits(99));
    reset();vf[1][0]=bits(2);vf[2][0]=bits(3);vi[5]=7;
    step(upper(3,1,2,8,40),lower(13,5,3,15,0));CHECK(vf[3][0]==bits(5));CHECK(vi[5]==7);
    reset();vi[5]=7;vf[3][0]=bits(99);
    step(upper(3,1,2,0,40),lower(13,5,3,15,0));CHECK(vi[5]==8);CHECK(vf[3][0]==0);
    /* E-bit flush and per-unit independence through real production entry. */
    vu1_init();vu1_state_t *s=vu1_get_state();s->vf[1][0]=bits(8);s->vf[2][0]=bits(2);
    vu1_micro_write32(0,lower(14,1,2,0,0));vu1_micro_write32(4,NOP|0x40000000u);
    vu1_micro_write32(8,0);vu1_micro_write32(12,NOP);vu1_exec_micro(0);
    CHECK(s->vi[22]==bits(4));CHECK(!s->pipeline.q_pending);CHECK(s->instructions_executed==2);
    reset();vi[21]=bits(3);vf[1][0]=bits(2);
    step(upper(2,1,0,8,34)|0x80000000u,bits(4));CHECK(vf[2][0]==bits(5));CHECK(vi[21]==bits(4));
    step(upper(2,1,0,8,34),0);CHECK(vf[2][0]==bits(6));
    /* Nested taken branches retain both redirects: the younger delay
     * slot executes at the older target, not at the sequential PC. */
    reset();step(NOP,(32u<<25)|2);CHECK(pc==8&&bd==1&&bt==24);
    step(NOP,(32u<<25)|3);CHECK(pc==24&&bd==1&&bt==40);
    step(NOP,lower(12,0,5,8,0));CHECK(pc==40&&bd==0);
    reset();step(NOP,(32u<<25)|2);vi[1]=1;vi[2]=2;
    step(NOP,(40u<<25)|(1u<<11)|(2u<<16)|3);CHECK(pc==24&&bd==0);
    reset();step(NOP,(32u<<25)|2);vi[1]=9;
    step(NOP,(37u<<25)|(1u<<11)|(1u<<16));CHECK(pc==24&&bd==1&&bt==72&&vi[1]==4);
    step(NOP,0);CHECK(pc==72&&bd==0);
    /* Shared FBRST trap matrix: D/T gates are per-unit, VU1 must
     * not consult its local VI[28]. Marked pair retires in full. */
    for(unsigned unit=0;unit<2;unit++)for(unsigned gates=0;gates<4;gates++)
    for(unsigned marked=1;marked<4;marked++)for(unsigned e=0;e<2;e++) {
        reset();ee_intc_init();unsigned shift=unit?8:0;
        uint32_t *ctrl=unit?ee_core_get_state()->cop2_ctrl:vi;
        ctrl[28]=gates<<(shift+2);ctrl[29]=0x80000000u;
        if(unit)vi[28]=0xc; /* Poison the non-shared register. */
        put(0,0x80000000u|(2u<<16)|(1u<<11)|(3u<<6)|48u);
        put(4,upper(3,1,2,8,40)|(marked&1?0x10000000u:0)|
            (marked&2?0x08000000u:0)|(e?0x40000000u:0));
        vi[1]=7;vi[2]=9;vf[1][0]=bits(2);vf[2][0]=bits(3);
        int stop=vu_micro_step_pipeline(vf,vi,acc,mem,unit?16383:4095,
            micro,unit?16383:4095,&pc,&bd,&bt,&ed,&retired,&unknown,&pipe);
        unsigned enabled=gates&marked;
        CHECK(stop==!!enabled);CHECK(retired==1&&pc==8);
        CHECK(vf[3][0]==bits(5)&&vi[3]==16);
        CHECK(ctrl[29]==(0x80000000u|(enabled<<(shift+1))));
        CHECK(ee_intc_get_state()->stat==(enabled?(1u<<(unit?7:6)):0));
        CHECK(ee_intc_get_raise_count(unit?7:6)==((enabled&1)!=0)+((enabled&2)!=0));
        CHECK(ed==(enabled?0:e));
    }
    puts("PASS 48 VU0/VU1 D/T gate, shared control, E priority and pair-retirement oracles");
    /* All 13 EFU operations: independent simple inputs and issue latencies. */
    const unsigned sub[13]={28,28,28,28,29,29,29,30,30,30,31,31,31};
    const unsigned bc[13]={0,1,2,3,0,1,2,0,1,2,0,1,2};
    const unsigned latency[13]={11,18,18,24,54,54,12,12,18,12,29,54,44};
    const float answer[13]={25,0.04f,5,0.2f,0,0,16,3,1.f/3,1.f/9,0,0.785398185253143f,1};
    for(unsigned k=0;k<13;k++) {
        reset();vi[23]=bits(99);vf[1][0]=bits(3);vf[1][1]=bits(4);vf[1][2]=0;
        vf[1][3]=bits(k>=10?0:9);
        if(k==4||k==5)vf[1][0]=0;
        step(NOP,lower(sub[k],1,0,3,bc[k]));CHECK(pipe.p_pending);CHECK(pipe.p_ready==latency[k]);CHECK(vi[23]==bits(99));
        step(NOP,lower(30,0,0,0,3));CHECK(!pipe.p_pending);CHECK(pipe.cycle==latency[k]);
        if(vi[23]!=bits(answer[k]))printf("EFU k=%u got=%08x expected=%08x\n",k,vi[23],bits(answer[k]));CHECK(vi[23]==bits(answer[k]));
        step(NOP,lower(25,0,5,8,0));CHECK(vf[5][0]==bits(answer[k]));CHECK(unknown==0);
    }
    /* MFP without WAITP reads the old visible value. */
    reset();vi[23]=bits(99);vf[1][0]=bits(3);step(NOP,lower(28,1,0,0,0));
    step(NOP,lower(25,0,5,8,0));CHECK(vf[5][0]==bits(99));
    for(unsigned n=0;n<9;n++)step(NOP,0);
    step(NOP,lower(25,0,5,8,0));CHECK(vf[5][0]==bits(9));
    /* Flag query destinations and the split 12-bit status immediate. */
    reset();vi[18]=0xabcdef;step(NOP,(16u<<25)|0xabcdef);CHECK(vi[1]==1);
    step(NOP,(18u<<25)|0x100);CHECK(vi[1]==1);step(NOP,(19u<<25)|0x543210);CHECK(vi[1]==1);
    step(NOP,(28u<<25)|(3u<<16));CHECK(vi[3]==0xdef);
    vi[16]=0xabc;step(NOP,(20u<<25)|(4u<<16)|(1u<<21)|0x2bc);CHECK(vi[4]==1);
    step(NOP,(22u<<25)|(4u<<16)|0x123);CHECK(vi[4]==(0xabc&0x123));
    step(NOP,(23u<<25)|(4u<<16)|0x123);CHECK(vi[4]==(0xabc|0x123));
    step(NOP,(21u<<25)|(1u<<21)|0x500);CHECK(vi[16]==0xd3c);
    vi[17]=0x5a5a;vi[2]=0x5a5a;step(NOP,(24u<<25)|(4u<<16)|(2u<<11));CHECK(vi[4]==1);
    step(NOP,(26u<<25)|(4u<<16)|(2u<<11));CHECK(vi[4]==0x5a5a);
    step(NOP,(27u<<25)|(4u<<16)|(2u<<11));CHECK(vi[4]==0x5a5a);
    /* CLIP upper wins a same-pair FCSET, queries see old CLIP. */
    reset();vi[18]=7;vf[1][0]=bits(2);vf[2][3]=bits(1);
    step(upper(7,1,2,0,63),(16u<<25)|7);CHECK(vi[1]==1);CHECK(vi[18]==(7u<<6|1));
    step(upper(7,1,2,0,63),(17u<<25)|0xffffffu);CHECK(vi[18]==((7u<<6|1)<<6|1));
    /* R1330-D: the generic shared step is not a production VU1 and must
     * not inject PATH1 merely because its synthetic memory is 16 KiB. */
    reset();gif_init();vi[1]=1023;
    unsigned addr=16368;mem[addr]=1;mem[addr+1]=0x80;mem[addr+7]=0x10;mem[addr+8]=0x0e;
    memset(mem,0,16);mem[0]=0x46;mem[8]=0;
    step(NOP,lower(27,1,0,0,0));CHECK(gif_get_state()->gif_path1_transfers==0);

    /* Production VU1 XGKICK latches PATH1, preserves ring wrap, and is
     * drained by the runner boundary before vu1_exec_micro() returns. */
    vu1_init();gif_init();s=vu1_get_state();s->vi[1]=1023;addr=16368;
    s->mem[addr]=1;s->mem[addr+1]=0x80;s->mem[addr+7]=0x10;s->mem[addr+8]=0x0e;
    memset(s->mem,0,16);s->mem[0]=0x46;s->mem[8]=0; /* PRIM=0x46 */
    vu1_micro_write32(0,lower(27,1,0,0,0));
    vu1_micro_write32(4,NOP|0x40000000u);
    vu1_micro_write32(8,0);vu1_micro_write32(12,NOP);
    vu1_exec_micro(0);
    CHECK(!s->xgkick_pending);CHECK(s->xgkick_addr==16368);
    CHECK(gif_get_state()->prim==0x46);CHECK(gif_get_state()->gif_path1_transfers==1);
    puts("PASS EFU/P, flag queries, CLIP ordering and deferred wrapped PATH1");
    puts("PASS VU selectors, Q issue/WAITQ/reissue/flags/E-bit and VF pair hazards");return 0;
}
