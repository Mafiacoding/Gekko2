/* R1335 IPU streaming command engine. See docs/R1335-IPU-HLE-ARM.md.
 * Hardware protocol informed by PCSX2 IPU.cpp/IPU.h/IPU_MultiISA.cpp
 * at 3c8df07e8e367caef8386fd949d6ca44bf2f7ad0; GPL-3.0+. */
#include "core/hw/ipu.h"
#include "core/hw/ee_intc.h"
#include "core/hw/arm_worker.h"
#include "core/runtime_profile.h"
#include <string.h>
extern int dma_ipu_service(void) __attribute__((weak));
static ipu_state_t s;
static ipu_profile_t profile;
static int servicing,worker_pending;
ipu_state_t *ipu_get_state(void){return &s;}
void ipu_get_profile(ipu_profile_t *out)
{if(out){*out=profile;out->fifo_count=s.in_count;out->output_available=s.out_count;out->busy=s.busy;}}
int ipu_state_valid(const ipu_state_t *p)
{
 return p&&p->bp<128&&p->fp<=2&&p->in_head<8&&p->in_count<=8&&
 p->out_head<8&&p->out_count<=8&&p->busy<=1&&p->skip<=63&&
 p->pos<=1024&&p->blocks<=2047&&p->out_size<=1024&&
 !(p->out_size&15)&&p->out_pos<=p->out_size&&!(p->out_pos&15)&&p->th0<=511&&p->th1<=511;
}
void ipu_restore(const ipu_state_t *p){if(ipu_state_valid(p)){s=*p;servicing=worker_pending=0;arm_worker_reset();}}
void ipu_init(void){memset(&s,0,sizeof s);memset(&profile,0,sizeof profile);servicing=worker_pending=0;arm_worker_reset();}
static void complete(void){s.busy=0;profile.completed_commands++;ee_intc_raise(8);}
static int fill(unsigned bits)
{
 while(s.fp*128<s.bp+bits){
  if(!s.in_count){profile.input_stalls++;return 0;}
  if(s.fp==2)return 0;
  memcpy(s.internal[s.fp++],s.input[s.in_head],16);
  s.in_head=(s.in_head+1)&7;s.in_count--;
 }
 return 1;
}
static void advance(unsigned bits)
{
 s.bp+=bits;
 if(s.bp>=128){s.bp-=128;s.fp--;if(s.fp)memcpy(s.internal[0],s.internal[1],16);}
}
static int bits_read(unsigned bits,int consume,uint32_t *value)
{
 if(!fill(bits))return 0;
 const uint8_t *p=(const uint8_t*)s.internal;uint32_t v=0;
 /* Extract bytes instead of one branch/shift per bit. The two internal
  * QWCs make the possible fifth byte of an unaligned 32-bit peek safe. */
 unsigned at=s.bp,left=bits;
 while(left){unsigned n=8-(at&7);if(n>left)n=left;
  v=(v<<n)|((p[at>>3]>>(8-(at&7)-n))&((1u<<n)-1u));
  at+=n;left-=n;
 }
 if(consume)advance(bits);
 *value=v;return 1;
}
uint32_t ipu_input_write(const uint8_t *data,uint32_t qwc)
{
 uint32_t n=qwc;if(n>8-s.in_count)n=8-s.in_count;
 profile.input_qwc+=qwc;profile.accepted_qwc+=n;
 for(uint32_t i=0;i<n;i++)memcpy(s.input[(s.in_head+s.in_count+i)&7],data+i*16,16);
 s.in_count+=n;return n;
}
uint32_t ipu_output_read(uint8_t *data,uint32_t qwc)
{
 uint32_t n=qwc;if(n>s.out_count)n=s.out_count;
 for(uint32_t i=0;i<n;i++)memcpy(data+i*16,s.output[(s.out_head+i)&7],16);
 s.out_head=(s.out_head+n)&7;s.out_count-=n;profile.output_qwc+=n;return n;
}
static int run(void)
{
 if(!s.busy)return 0;
 uint32_t v;unsigned cmd=s.command>>28;int progress=0;
 if(s.skip){if(!fill(s.skip))return 0;advance(s.skip);s.skip=0;progress=1;}
 if(cmd==3){
  if(!s.pos){
   if(!bits_read(32,0,&v))return progress;
   unsigned consumed=0;s.data=ipu_vlc_decode(v,(s.command>>26)&3,s.ctrl,&consumed);
   if(consumed)advance(consumed);
   if(!s.data)s.ctrl|=0x4000u;
   s.pos=1;progress=1;
  }
  /* TOP can starve after the VLC was consumed. Resume this phase without
   * decoding or advancing the same symbol a second time. */
  if(!bits_read(32,0,&v))return progress;
  s.top=v;complete();return 1;
 }
 if(cmd==4){if(!bits_read(32,0,&v))return progress;s.data=s.top=v;complete();return 1;}
 if(cmd==5||cmd==6){
  unsigned bytes=cmd==5?64:32;
  uint8_t *dst=cmd==5?s.iq[(s.command>>27)&1]:s.vq;
  while(s.pos<bytes){if(!bits_read(8,1,&v))return progress;dst[s.pos++]=(uint8_t)v;progress=1;}
  complete();return 1;
 }
 if(cmd==7||cmd==8){
  if(s.out_size){
   while(s.out_pos<s.out_size&&s.out_count<8){
    memcpy(s.output[(s.out_head+s.out_count)&7],s.converted+s.out_pos,16);
    s.out_pos+=16;s.out_count++;progress=1;
   }
   if(s.out_pos<s.out_size){profile.output_stalls++;return progress;}
   s.out_size=s.out_pos=0;s.pos=0;s.blocks--;progress=1;
  }
  if(!s.blocks){complete();return 1;}
  unsigned bytes=cmd==7?384:1024;
  while(s.pos<bytes){if(!bits_read(8,1,&v))return progress;s.block[s.pos++]=(uint8_t)v;progress=1;}
  unsigned size=(s.command&(1u<<27))?512:(cmd==7?1024:128);
  if(cmd==8){
   ipu_pack_convert(s.block,s.converted,(s.command>>27)&1,(s.command>>26)&1,s.vq);
   s.out_size=size;return 1;
  }
  if(worker_pending){
   int result=arm_worker_take(s.converted,size);if(!result)return progress;
   worker_pending=0;
   if(result<0)ipu_csc_convert(s.block,s.converted,(s.command>>27)&1,(s.command>>26)&1,s.th0,s.th1);
  }else if(arm_worker_submit_csc(s.block,s.command,s.th0,s.th1)){worker_pending=1;return progress;}
  else ipu_csc_convert(s.block,s.converted,(s.command>>27)&1,(s.command>>26)&1,s.th0,s.th1);
  s.out_size=size;profile.csc_macroblocks++;return 1;
 }
 return progress;
}
void ipu_service(void)
{
 if(servicing)return;
 servicing=1;
 unsigned previous=gp_enter(GP_IPU);
 /* Bounded work; only IPU MMIO/DMA triggers call this, no per-EE tax. */
 for(unsigned guard=0;guard<16384;guard++){
  int progress=run();if(dma_ipu_service)progress|=dma_ipu_service();
  if(!progress)break;
 }
 gp_leave(previous);servicing=0;
}
static void command(uint32_t value)
{
 if(worker_pending){worker_pending=0;arm_worker_reset();}
 unsigned cmd=value>>28;
 profile.commands[cmd]++;profile.last_command=value;
 s.command=value;s.ctrl&=~0x0000c000u;s.busy=1;s.pos=s.out_pos=s.out_size=0;s.skip=0;
 if(cmd==0){s.in_head=s.in_count=s.fp=0;s.bp=value&127;complete();}
 else if(cmd==9){s.th0=value&511;s.th1=(value>>16)&511;complete();}
 else if(cmd==3||cmd==4||cmd==5){s.skip=value&63;}
 else if(cmd==6){}
 else if(cmd==7||cmd==8){s.blocks=value&2047;}
 else{profile.unimplemented_commands++;complete();} /* Legacy incomplete MPEG path; diagnostics explicit. */
 ipu_service();
}
int ipu_mmio_read32(uint32_t addr,uint32_t *out)
{
 if((addr&~255u)!=0x10002000u)return 0;
 ipu_service();
 switch(addr&255){
 case 0: if((s.command>>28)!=3&&(s.command>>28)!=4){uint32_t v;if(bits_read(32,0,&v))s.data=v;}*out=s.data;break;
 case 4:*out=(s.busy&&((s.command>>28)==3||(s.command>>28)==4))?0x80000000u:0;break;
 case 0x10:*out=(s.ctrl&0x07ffffffu)|s.in_count|(s.out_count<<4)|(s.busy?0x80000000u:0);break;
 case 0x20:*out=s.bp|(s.in_count<<8)|(s.fp<<16);break;
 case 0x30:*out=s.top;break;
 case 0x34:*out=(s.busy&&((s.command>>28)==3||(s.command>>28)==4))?0x80000000u:0;break;
 default:*out=0;break;
 }
 return 1;
}
int ipu_mmio_write32(uint32_t addr,uint32_t value)
{
 if((addr&~255u)!=0x10002000u)return 0;
 if((addr&255)==0)command(value);
 else if((addr&255)==0x10){
  s.ctrl=(value&0x47f30000u)|(s.ctrl&0x8000ffffu);
  if(((s.ctrl>>16)&3)==3)s.ctrl=(s.ctrl&~0x30000u)|0x10000u;
  if(value&0x40000000u){worker_pending=0;arm_worker_reset();s.ctrl&=0x07f33f00u;s.data=s.top=s.command=s.busy=s.bp=s.fp=0;
   s.in_head=s.in_count=s.out_head=s.out_count=s.skip=s.pos=s.blocks=s.out_pos=s.out_size=0;
   s.th0=s.th1=0;ee_intc_raise(8);}
 }
 return 1; /* BP/TOP/BUSY read-only. */
}
int ipu_mmio_read64(uint32_t addr,uint64_t *out)
{uint32_t lo,hi;if(!ipu_mmio_read32(addr,&lo))return 0;ipu_mmio_read32(addr+4,&hi);*out=lo|((uint64_t)hi<<32);return 1;}
int ipu_mmio_write64(uint32_t addr,uint64_t value)
{return ipu_mmio_write32(addr,(uint32_t)value);}
int ipu_fifo_read128(uint32_t addr,uint8_t out[16])
{if(addr!=0x10007000u)return 0;ipu_service();if(!ipu_output_read(out,1))memset(out,0,16);ipu_service();return 1;}
int ipu_fifo_write128(uint32_t addr,const uint8_t in[16])
{if(addr!=0x10007010u)return 0;ipu_service();if(!ipu_input_write(in,1))profile.discarded_qwc++;ipu_service();return 1;}
void ipu_process_quadwords(int channel,const uint8_t *data,uint32_t qwc)
{(void)channel;uint32_t n=ipu_input_write(data,qwc);profile.discarded_qwc+=qwc-n;ipu_service();}
