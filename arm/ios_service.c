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
  ios_invalidate(ptr(a),12);const char *p=ptr(a);
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
  int r=gekko2_arm_dispatch(0,0,0,0,0,ptr(a),32);
  ios_flush(ptr(a),32);return r;
 }
 if(m->command==7){
  if(m->args.ioctlv.request!=1||m->args.ioctlv.num_in!=2||m->args.ioctlv.num_out!=1)return -101;
  uint32_t a=m->args.ioctlv.vectors;
  if((a&31)||!buffer(a,24))return -101;
  ios_invalidate(ptr(a),24);ios_vector *v=ptr(a);
  /* Snapshot descriptors: subsequent DMA/requests cannot change the spans
   * used by this job. The client keeps its private input buffers pinned. */
  ios_vector d[3];for(unsigned i=0;i<3;i++)d[i]=v[i];
  if(d[0].length!=32)return -101;
  for(unsigned i=0;i<3;i++)if((d[i].data&31)||!buffer(d[i].data,d[i].length))return -101;
  for(unsigned i=0;i<3;i++)for(unsigned j=i+1;j<3;j++)
   if(d[i].data<d[j].data+d[j].length&&d[j].data<d[i].data+d[i].length)return -101;
  ios_invalidate(ptr(d[0].data),d[0].length);ios_invalidate(ptr(d[1].data),d[1].length);
  int r=gekko2_arm_dispatch(1,ptr(d[0].data),32,ptr(d[1].data),d[1].length,ptr(d[2].data),d[2].length);
  if(r>=0)ios_flush(ptr(d[2].data),d[2].length);
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
