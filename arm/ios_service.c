// SPDX-License-Identifier: GPL-3.0+
/* IOS resource-manager shell for the portable CSC worker. ABI/syscall
 * numbers checked against wiidev/d2x-cios source/cios-lib. This module
 * needs a compatible RAM loader; the Wii DOL does not install/reload IOS. */
#include <stdint.h>
#include "core/hw/arm_worker.h"
typedef struct {uint32_t data,length;} ios_vector;
typedef struct {
 uint32_t command,result,fd;
 union {
  struct {uint32_t path,mode,resultfd;} open;
  struct {uint32_t request,in,in_size,out,out_size;} ioctl;
  struct {uint32_t request,num_in,num_out,vectors;} ioctlv;
 } args;
} ios_message;
extern int ios_queue_create(void *,unsigned);
extern int ios_queue_receive(int,ios_message **,unsigned);
extern int ios_register(const char *,int);
extern void ios_ack(ios_message *,int);
extern void ios_invalidate(void *,unsigned);
extern void ios_flush(void *,unsigned);
/* ARM may only access shared MEM1 through aligned words. The codec's
 * byte operations run exclusively in this module's private MEM2 buffers. */
static uint32_t stage_header[8] __attribute__((aligned(32)));
static uint32_t stage_input[768] __attribute__((aligned(32)));
static uint32_t stage_output[2048] __attribute__((aligned(32)));
static uint32_t stage_path[3] __attribute__((aligned(32)));
static void shared_words(void *dst,const void *src,unsigned bytes)
{
 volatile uint32_t *d=dst;const volatile uint32_t *s=src;
 for(unsigned i=0;i<bytes/4u;i++)d[i]=s[i];
}
static uint32_t messages[16] __attribute__((aligned(32)));
/* IPC buffers must fit entirely in the IOS shared MEM1/MEM2 mapping.
 * Reject wraparound, NULL, misaligned headers/vectors and oversized data. */
static int buffer(uint32_t address,unsigned length)
{
 if(!address||!length||length>8192)return 0;
 return (address<0x01800000u&&length<=0x01800000u-address)||
  (address>=0x10000000u&&address<0x14000000u&&length<=0x14000000u-address);
}
static void *ptr(uint32_t a){return (void *)(uintptr_t)a;}
__attribute__((noinline,used)) int gekko2_ios_handle(ios_message *m)
{
 if(m->command==1){
  uint32_t a=m->args.open.path;
  if(!buffer(a,12))return -101;
  ios_invalidate(ptr(a),12);shared_words(stage_path,ptr(a),12);const char *p=(const char*)stage_path;
  static const char name[]="/dev/gekko2";
  for(unsigned i=0;i<sizeof name;i++)if(p[i]!=name[i])return -6;
  return 0;
 }
 if(m->command==2)return m->fd==0?0:-101;
 if(m->fd)return -101;
 if(m->command==6){
  uint32_t a=m->args.ioctl.out;
  if(m->args.ioctl.request||m->args.ioctl.in_size||m->args.ioctl.out_size!=32||
   (a&31)||!buffer(a,32))return -101;
  int r=gekko2_arm_dispatch(0,0,0,0,0,(uint8_t*)stage_output,32);
  if(r==32)shared_words(ptr(a),stage_output,32);
  ios_flush(ptr(a),32);return r;
 }
 /* Capability bit 1: bounded shared-RAM fill, CPU-owned guest RAM only.
  * Private IOS/module RAM is excluded; destinations require word alignment. */
 if(m->command==7&&m->args.ioctlv.request==2){
  if(m->args.ioctlv.num_in!=1||m->args.ioctlv.num_out!=1)return -101;
  uint32_t a=m->args.ioctlv.vectors;if((a&31)||!buffer(a,16))return -101;
  ios_invalidate(ptr(a),16);ios_vector *v=ptr(a),d[2]={v[0],v[1]};
  uint32_t dest=d[1].data,n=d[1].length;
  if(d[0].length!=32||(d[0].data&31)||!buffer(d[0].data,32)||
     (dest&31)||(n&31)||n<65536u||n>1048576u||
     !((dest&&dest<0x01800000u&&n<=0x01800000u-dest)||
       (dest>=0x10000000u&&dest<0x13000000u&&n<=0x13000000u-dest)))return -101;
  if(d[0].data<dest+n&&dest<d[0].data+32u)return -101;
  if(a<dest+n&&dest<a+16u)return -101;
  ios_invalidate(ptr(d[0].data),32);shared_words(stage_header,ptr(d[0].data),32);
  const uint8_t *h=(const uint8_t*)stage_header;
  uint32_t magic=(uint32_t)h[0]<<24|(uint32_t)h[1]<<16|(uint32_t)h[2]<<8|h[3];
  uint32_t version=(uint32_t)h[4]<<24|(uint32_t)h[5]<<16|(uint32_t)h[6]<<8|h[7];
  if(magic!=ARM_WORKER_MAGIC||version!=ARM_WORKER_VERSION||h[8]||h[9]||h[10])return -101;
  uint32_t value=(uint32_t)h[11]*0x01010101u;
  ios_invalidate(ptr(dest),n);volatile uint32_t *out=ptr(dest);
  for(unsigned i=0;i<n/4u;i++)out[i]=value;
  ios_flush(ptr(dest),n);return (int)n;
 }
 if(m->command==7){
  if(m->args.ioctlv.request!=1||m->args.ioctlv.num_in!=2||m->args.ioctlv.num_out!=1)return -101;
  uint32_t a=m->args.ioctlv.vectors;
  if((a&31)||!buffer(a,24))return -101;
  ios_invalidate(ptr(a),24);ios_vector *v=ptr(a);
  /* Snapshot descriptors: subsequent DMA/requests cannot change the spans
   * used by this job. The client keeps its private input buffers pinned. */
  ios_vector d[3];for(unsigned i=0;i<3;i++)d[i]=v[i];
  if(d[0].length!=32||d[1].length>sizeof stage_input||d[2].length>sizeof stage_output||
     (d[1].length&3u)||(d[2].length&3u))return -101;
  for(unsigned i=0;i<3;i++)if((d[i].data&31)||!buffer(d[i].data,d[i].length))return -101;
  for(unsigned i=0;i<3;i++)for(unsigned j=i+1;j<3;j++)
   if(d[i].data<d[j].data+d[j].length&&d[j].data<d[i].data+d[i].length)return -101;
  ios_invalidate(ptr(d[0].data),d[0].length);ios_invalidate(ptr(d[1].data),d[1].length);
  shared_words(stage_header,ptr(d[0].data),32);
  shared_words(stage_input,ptr(d[1].data),d[1].length);
  int r=gekko2_arm_dispatch(1,(uint8_t*)stage_header,32,(uint8_t*)stage_input,d[1].length,(uint8_t*)stage_output,d[2].length);
  if(r>=0){shared_words(ptr(d[2].data),stage_output,d[2].length);ios_flush(ptr(d[2].data),d[2].length);}
  return r<0?-101:r;
 }
 return -101;
}
int gekko2_ios_main(void)
{
 int q=ios_queue_create(messages,16);if(q<0)return q;
 int r=ios_register("/dev/gekko2",q);if(r<0)return r;
 for(;;){ios_message *m=0;if(!ios_queue_receive(q,&m,0)&&m)ios_ack(m,gekko2_ios_handle(m));}
}
