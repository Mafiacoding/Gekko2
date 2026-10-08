/* GPL-3.0+. Portable IOS resource-manager command handler, not an IOS
 * installer/server image. An IOS service must validate/map IPC vectors
 * before calling this function and supply /dev/gekko2 registration. */
#include "core/hw/arm_worker.h"
#include "core/hw/ipu.h"
static uint32_t rd(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void wr(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int gekko2_arm_dispatch(unsigned request,const uint8_t *h,unsigned hs,
 const uint8_t *in,unsigned is,uint8_t *out,unsigned os)
{
 if(request==0){
  if(!out||os!=32||hs||is)return -1;
  for(unsigned i=0;i<32;i++)out[i]=0;
  wr(out,ARM_WORKER_MAGIC);wr(out+4,ARM_WORKER_VERSION);
  wr(out+8,1);wr(out+12,ARM_WORKER_MAX_BLOCKS);return 32;
 }
 if(request!=1||!h||hs!=32||!in||!out||rd(h)!=ARM_WORKER_MAGIC||rd(h+4)!=ARM_WORKER_VERSION)return -1;
 unsigned blocks=rd(h+16),options=rd(h+20),th0=rd(h+24),th1=rd(h+28);
 if(!blocks||blocks>ARM_WORKER_MAX_BLOCKS||options>3||th0>511||th1>511||is!=blocks*384)return -1;
 unsigned stride=(options&1)?512:1024;
 if(os!=blocks*stride)return -1;
 for(unsigned i=0;i<blocks;i++)ipu_csc_convert(in+i*384,out+i*stride,options&1,(options>>1)&1,th0,th1);
 return blocks*stride;
}
