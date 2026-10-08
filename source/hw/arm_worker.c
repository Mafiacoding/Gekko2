/* GPL-3.0+. Optional asynchronous IOS client; absence is a CPU fallback.
 * The buffers outlive cold boot and every pending request. An old epoch
 * can never publish pixels into a new IPU command/session. */
#include "core/hw/arm_worker.h"
#include "core/recompiler/optimization.h"
#include <string.h>
static uint64_t stats[5]; /* probes, submissions, successes, errors, stale */
#ifdef GEKKO
#include <gccore.h>
#include <ogc/ipc.h>
static int fd=-1,probed;
static volatile int pending,done,result;
static uint32_t epoch,job_epoch,sequence;
static unsigned expected;
static uint8_t header[32] __attribute__((aligned(32)));
static uint8_t input[384] __attribute__((aligned(32)));
static uint8_t output[1024] __attribute__((aligned(32)));
static ioctlv vectors[3] __attribute__((aligned(32)));
static uint32_t rd(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void wr(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static s32 completed(s32 r,void *unused){(void)unused;result=r;__asm__ volatile("sync":::"memory");done=1;return 0;}
#endif
int arm_worker_available(void)
{
#ifdef GEKKO
 return fd>=0&&gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER);
#else
 return 0;
#endif
}
uint64_t arm_worker_stat(unsigned n){return n<5?stats[n]:0;}
void arm_worker_reset(void)
{
 memset(stats,0,sizeof stats);
#ifdef GEKKO
 if(++epoch==0)epoch=1;
 /* Never reuse the static vectors while an earlier IOS request owns them. */
 if(gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER)&&!probed){
  probed=1;stats[0]++;fd=IOS_Open("/dev/gekko2",0);
  if(fd>=0){
   uint8_t caps[32] __attribute__((aligned(32)));memset(caps,0,sizeof caps);
   int r=IOS_Ioctl(fd,0,0,0,caps,sizeof caps);
   if(r!=32||rd(caps)!=ARM_WORKER_MAGIC||rd(caps+4)!=ARM_WORKER_VERSION||!(rd(caps+8)&1)||rd(caps+12)<1){IOS_Close(fd);fd=-1;stats[3]++;}
  }
 }
#endif
}
int arm_worker_submit_csc(const uint8_t in[384],uint32_t command,uint16_t th0,uint16_t th1)
{
#ifdef GEKKO
 if(pending&&done&&job_epoch!=epoch){pending=0;done=0;stats[4]++;}
 if(!gekko2_opt_enabled(GEKKO2_OPT_ARM_WORKER)||fd<0||pending)return 0;
 job_epoch=epoch;expected=(command&(1u<<27))?512:1024;
 wr(header,ARM_WORKER_MAGIC);wr(header+4,ARM_WORKER_VERSION);wr(header+8,epoch);wr(header+12,++sequence);
 wr(header+16,1);wr(header+20,((command>>27)&1)|(((command>>26)&1)<<1));wr(header+24,th0);wr(header+28,th1);
 memcpy(input,in,384);memset(output,0,expected);
 vectors[0]=(ioctlv){header,32};vectors[1]=(ioctlv){input,384};vectors[2]=(ioctlv){output,expected};
 DCFlushRange(header,32);DCFlushRange(input,384);DCFlushRange(output,expected);
 DCFlushRange(vectors,sizeof vectors);
 done=0;pending=1;
 int r=IOS_IoctlvAsync(fd,1,2,1,vectors,completed,0);
 if(r<0){pending=0;stats[3]++;return 0;}stats[1]++;return 1;
#else
 (void)in;(void)command;(void)th0;(void)th1;return 0;
#endif
}
int arm_worker_take(uint8_t *out,unsigned size)
{
#ifdef GEKKO
 if(!pending)return -1;
 if(!done)return 0;
 __asm__ volatile("sync":::"memory");pending=0;done=0;
 if(job_epoch!=epoch){stats[4]++;return -1;}
 if(result!=(int)expected||size!=expected){stats[3]++;return -1;}
 DCInvalidateRange(output,expected);memcpy(out,output,expected);stats[2]++;return 1;
#else
 (void)out;(void)size;return -1;
#endif
}
