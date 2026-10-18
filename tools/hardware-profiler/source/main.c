#include <gccore.h>
#include <fat.h>
#include <ogc/ipc.h>
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>

static u32 caps[8] __attribute__((aligned(32)));
static u8 header[32] __attribute__((aligned(32)));
static ioctlv vec[2] __attribute__((aligned(32)));
static volatile u32 sink;
static u64 ticks(void){return gettime();}
static double ms(u64 a,u64 b){return (double)ticks_to_microsecs(b-a)/1000.0;}
static void row(FILE *f,const char *name,unsigned bytes,double t,int ok){
 printf("%-23s %9u bytes %9.3f ms %s\n",name,bytes,t,ok?"OK":"FAIL");
 if(f)fprintf(f,"%s,%u,%.4f,%d\n",name,bytes,t,ok);
}
static void cpu_test(FILE *f,u8 *p,unsigned n){
 u64 a=ticks();
 for(unsigned k=0;k<32;k++)for(unsigned i=0;i<n;i+=64)p[i]=(u8)(i+k);
 u64 b=ticks();row(f,"broadway_stride_write",n*32,ms(a,b),1);
 a=ticks();for(unsigned k=0;k<32;k++)for(unsigned i=0;i<n;i+=64)sink+=p[i];
 b=ticks();row(f,"broadway_stride_read",n*32,ms(a,b),1);
}
static void arm_test(FILE *f,u8 *p,unsigned n){
 int fd=IOS_Open("/dev/gekko2",0);
 if(fd<0){row(f,"starlet_device_missing",0,0,0);return;}
 memset(caps,0,sizeof caps);DCFlushRange(caps,sizeof caps);
 int r=IOS_Ioctl(fd,0,NULL,0,caps,sizeof caps);DCInvalidateRange(caps,sizeof caps);
 if(r!=32||caps[0]!=0x474b4131u||caps[1]!=1u||!(caps[2]&2u)){
 row(f,"starlet_caps_unsupported",0,0,0);IOS_Close(fd);return;
 }
 memset(header,0,sizeof header);memcpy(header,"GKA1",4);header[7]=1;header[11]=0xa5;
 vec[0].data=header;vec[0].len=32;vec[1].data=p;vec[1].len=n;
 DCFlushRange(header,32);DCFlushRange(p,n);DCFlushRange(vec,sizeof vec);
 u64 a=ticks();r=IOS_Ioctlv(fd,2,1,1,vec);u64 b=ticks();
 DCInvalidateRange(p,n);
 int ok=r==(int)n;
 for(unsigned i=0;i<n&&ok;i++)if(p[i]!=0xa5)ok=0;
 row(f,"starlet_worker_fill",n,ms(a,b),ok);
 IOS_Close(fd);
}
int main(void){
 VIDEO_Init();GXRModeObj *mode=VIDEO_GetPreferredMode(NULL);
 void *fb=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 console_init(fb,20,20,mode->fbWidth,mode->xfbHeight,mode->fbWidth*VI_DISPLAY_PIX_SZ);
 VIDEO_Configure(mode);VIDEO_SetNextFramebuffer(fb);VIDEO_SetBlack(FALSE);VIDEO_Flush();VIDEO_WaitVSync();
 printf("GEKKO2 hardware profiler / IOS222 GKA1\n");
 int mounted=fatInitDefault();FILE *f=NULL;
 if(mounted){f=fopen("sd:/gekko2-profiler.csv","w");if(!f)f=fopen("usb:/gekko2-profiler.csv","w");}
 if(f)fprintf(f,"test,bytes,ms,verified\n");
 u8 *mem1=memalign(32,1024*1024),*mem2=memalign(32,1024*1024);
 if(mem1){memset(mem1,0,1024*1024);cpu_test(f,mem1,1024*1024);arm_test(f,mem1,1024*1024);}
 if(mem2){memset(mem2,0,1024*1024);cpu_test(f,mem2,1024*1024);}
 if(mem1&&mem2){
 u64 a=ticks();for(unsigned i=0;i<64;i++)memcpy(mem2,mem1,1024*1024);
 u64 b=ticks();row(f,"memory_copy",64*1024*1024,ms(a,b),memcmp(mem1,mem2,1024*1024)==0);
 }
 if(f){fflush(f);fclose(f);}
 printf("Finished. CSV on SD/USB if mounted.\n");
 free(mem1);free(mem2);sleep(12);return 0;
}
