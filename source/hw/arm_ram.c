/* GPL-3.0+. Opt-in MLOAD memory assistance for existing BIOS RAM helpers.
 * ARM reads/writes only proven host guest-RAM spans. Data verification is
 * essential: the owner's IOS reports status 1, not a portable byte count.
 * Synchronous IOS completion owns buffers until return; a failed partial
 * job is safe to overwrite through the caller's ordinary CPU fallback. */
#include "core/hw/arm_ram.h"
#include "core/recompiler/optimization.h"
#include <string.h>
#include <stdio.h>
static uint64_t counters[6];
#ifdef GEKKO
#include <ogc/ipc.h>
#include <ogc/cache.h>
static int fd=-1,probed,worker_fd=-1,worker_probed;
static uint32_t worker_caps[8] __attribute__((aligned(32)));
static uint8_t worker_header[32] __attribute__((aligned(32)));
static ioctlv worker_vectors[2] __attribute__((aligned(32)));
static uint8_t fill[4096] __attribute__((aligned(32)));
static int span(const void *p,unsigned n,uint32_t *physical)
{
 uintptr_t a=(uintptr_t)p;
 if(!p||(a&31u)||(n&31u)||n<65536u||n>1048576u)return 0;
 if(!((a>=0x80000000u&&a<0x81800000u&&n<=0x81800000u-a)||
      (a>=0x90000000u&&a<0x94000000u&&n<=0x94000000u-a)))return 0;
 *physical=(uint32_t)a&0x3fffffffu;return 1;
}
static int ready(void)
{
 if(!probed){probed=1;counters[0]++;fd=IOS_Open("/dev/mload",0);if(fd<0)counters[4]++;}
 return fd>=0;
}
#endif
uint64_t arm_ram_stat(unsigned n){return n<6?counters[n]:0;}
int arm_ram_available(void)
{
#ifdef GEKKO
 return (fd>=0||worker_fd>=0)&&gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM);
#else
 return 0;
#endif
}
void arm_ram_reset(void)
{
 memset(counters,0,sizeof counters);
#ifdef GEKKO
 if(fd>=0)IOS_Close(fd);if(worker_fd>=0)IOS_Close(worker_fd);fd=worker_fd=-1;probed=worker_probed=0;
#endif
}
int arm_ram_copy(uint8_t *d,const uint8_t *s,unsigned n)
{
#ifdef GEKKO
 uint32_t dp,sp;
 if(!gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM))return 0;
 if(!span(d,n,&dp)||!span(s,n,&sp)||!(dp>=sp+n||sp>=dp+n)||!ready()) {counters[5]++;return 0;}
 DCFlushRange((void*)s,n);DCFlushRange(d,n);
 for(unsigned at=0;at<n;at+=65536u) {
  unsigned bytes=n-at;if(bytes>65536u)bytes=65536u;
  if(IOS_Seek(fd,(int)(sp+at),SEEK_SET)<0){counters[4]++;return 0;}
  counters[1]++;int r=IOS_Read(fd,d+at,bytes);
  DCInvalidateRange(d+at,bytes);
  if(r<0||memcmp(d+at,s+at,bytes)){counters[4]++;return 0;}
 }
 counters[2]++;counters[3]+=n;return 1;
#else
 (void)d;(void)s;(void)n;return 0;
#endif
}
/* Bulk worker fill avoids thousands of 4-KiB MLOAD writes. Unlike a
 * firmware patch, this service only writes the caller-owned RAM vector. */
int arm_ram_worker_fill(uint8_t *d,uint8_t value,unsigned n)
{
#ifdef GEKKO
 uint32_t dp;
 if(!gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM)||!span(d,n,&dp))return 0;
 if(!worker_probed){
  worker_probed=1;counters[0]++;worker_fd=IOS_Open("/dev/gekko2",0);
  if(worker_fd>=0){
   memset(worker_caps,0,sizeof worker_caps);DCFlushRange(worker_caps,sizeof worker_caps);
   int result=IOS_Ioctl(worker_fd,0,0,0,worker_caps,sizeof worker_caps);
   DCInvalidateRange(worker_caps,sizeof worker_caps);
   if(result!=32||worker_caps[0]!=0x474b4131u||worker_caps[1]!=1u||!(worker_caps[2]&2u)){
    IOS_Close(worker_fd);worker_fd=-1;
   }
  }
 }
 if(worker_fd<0){counters[5]++;return 0;}
 memset(worker_header,0,sizeof worker_header);worker_header[0]=0x47;worker_header[1]=0x4b;worker_header[2]=0x41;worker_header[3]=0x31;
 worker_header[7]=1;worker_header[11]=value;
 worker_vectors[0]=(ioctlv){worker_header,32};worker_vectors[1]=(ioctlv){d,n};
 DCFlushRange(worker_header,32);DCFlushRange(worker_vectors,sizeof worker_vectors);DCFlushRange(d,n);
 counters[1]++;int result=IOS_Ioctlv(worker_fd,2,1,1,worker_vectors);
 DCInvalidateRange(d,n);
 if(result!=(int)n){counters[4]++;return 0;}
 for(unsigned i=0;i<n;i++)if(d[i]!=value){counters[4]++;return 0;}
 counters[2]++;counters[3]+=n;return 1;
#else
 (void)d;(void)value;(void)n;return 0;
#endif
}
int arm_ram_fill(uint8_t *d,uint8_t value,unsigned n)
{
#ifdef GEKKO
 uint32_t dp;
 if(!gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM))return 0;
 if(!span(d,n,&dp)||!ready()){counters[5]++;return 0;}
 memset(fill,value,sizeof fill);DCFlushRange(fill,sizeof fill);DCFlushRange(d,n);
 for(unsigned at=0;at<n;at+=sizeof fill) {
  unsigned bytes=n-at;if(bytes>sizeof fill)bytes=sizeof fill;
  if(IOS_Seek(fd,(int)(dp+at),SEEK_SET)<0){counters[4]++;return 0;}
  counters[1]++;int r=IOS_Write(fd,fill,bytes);
  DCInvalidateRange(d+at,bytes);
  if(r<0){counters[4]++;return 0;}
  for(unsigned i=0;i<bytes;i++)if(d[at+i]!=value){counters[4]++;return 0;}
 }
 counters[2]++;counters[3]+=n;return 1;
#else
 (void)d;(void)value;(void)n;return 0;
#endif
}
