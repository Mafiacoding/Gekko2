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
#include <ogc/system.h>
#include <ogc/ios.h>
#include <ogc/es.h>

static int ios222_installed(void){
 u64 title=0x1000000deULL;u32 size=0;
 if(ES_GetTMDViewSize(title,&size)<0||size<92||size>16384)return 0;
 tmd_view *v=memalign(32,(size+31)&~31u);if(!v)return 0;
 int ok=ES_GetTMDView(title,v,size)>=0;
 if(ok)ok=v->title_id==title&&v->title_version!=65280&&v->num_contents>1;
 free(v);return ok;
}

static u32 caps[8] __attribute__((aligned(32)));
static u8 header[32] __attribute__((aligned(32)));
static ioctlv vec[2] __attribute__((aligned(32)));
static volatile u32 sink;
static const char *region="none";
static unsigned original_ios;
static uintptr_t destination_address,source_address;
static void result(FILE *f,const char *name,unsigned n,double t,const char *status,int rc,int verified){
 printf("%s %s: %s rc=%d verified=%d %.3f ms\n",name,region,status,rc,verified,t);
 if(f)fprintf(f,"%s,%s,%u,%.4f,%s,%d,%d,%u,%u,%u,%08lx,%08lx\n",name,region,n,t,status,rc,verified,(unsigned)IOS_GetVersion(),(unsigned)IOS_GetRevision(),original_ios,(unsigned long)destination_address,(unsigned long)source_address);
}
static u64 ticks(void){return gettime();}
static double ms(u64 a,u64 b){return (double)ticks_to_microsecs(b-a)/1000.0;}
static void row(FILE *f,const char *name,unsigned bytes,double t,int ok){
 printf("%-23s %9u bytes %9.3f ms %s\n",name,bytes,t,ok?"OK":"FAIL");
 result(f,name,bytes,t,ok?"completed":"failed",0,0);
}
/* Same bounded synchronous seek/read/write path as source/hw/arm_ram.c.
 * Status 1 is accepted only after checking every byte, never as a byte count. */
static int span(const u8 *p,unsigned n,u32 *physical){
 uintptr_t a=(uintptr_t)p;
 if(!p||(a&31)||(n&31)||n<65536||n>1048576)return 0;
 if(!((a>=0x80000000u&&a<0x81800000u&&n<=0x81800000u-a)||
      (a>=0x90000000u&&a<0x94000000u&&n<=0x94000000u-a)))return 0;
 *physical=(u32)a&0x3fffffff;return 1;
}
static void mload_test(FILE *f,u8 *d,u8 *s,unsigned n){
 static u8 fill[4096] __attribute__((aligned(32)));
 u32 dp,sp;
 if(!span(d,n,&dp)||!span(s,n,&sp)||!(dp>=sp+n||sp>=dp+n)){
  result(f,"mload_span",n,0,"skipped",-1,0);return;
 }
 int fd=IOS_Open("/dev/mload",0);
 result(f,"mload_open",0,0,fd>=0?"available":"unavailable",fd,0);
 if(fd<0)return;
 for(unsigned copy=0;copy<2;copy++){
  for(unsigned i=0;i<n;i++)s[i]=(u8)((i*37u)^(i>>8)^0x5a);
  memset(d,0x3c,n);memset(fill,0xa5,sizeof fill);
  DCFlushRange(s,n);DCFlushRange(d,n);DCFlushRange(fill,sizeof fill);
  int rc=0,ok=1;u64 a=ticks();
  for(unsigned at=0;at<n;){
   unsigned bytes=copy?65536u:sizeof fill;if(bytes>n-at)bytes=n-at;
   rc=IOS_Seek(fd,(int)((copy?sp:dp)+at),SEEK_SET);
   if(rc<0){ok=0;break;}
   rc=copy?IOS_Read(fd,d+at,bytes):IOS_Write(fd,fill,bytes);
   DCInvalidateRange(d+at,bytes);
   if(rc<0){ok=0;break;}
   at+=bytes;
  }
  u64 b=ticks();
  /* Verification outside timing; no CPU fallback may turn failure into PASS. */
  if(ok)for(unsigned i=0;i<n;i++)if(d[i]!=(copy?s[i]:0xa5)){ok=0;break;}
  result(f,copy?"mload_copy":"mload_fill",n,ms(a,b),ok?"passed":"failed",rc,ok);
  if(!ok)break;
 }
 IOS_Close(fd);
}
static void cpu_test(FILE *f,u8 *p,unsigned n){
 u64 a=ticks();
 for(unsigned k=0;k<32;k++)for(unsigned i=0;i<n;i+=64)p[i]=(u8)(i+k);
 u64 b=ticks();row(f,"broadway_stride_write",n*32,ms(a,b),1);
 a=ticks();for(unsigned k=0;k<32;k++)for(unsigned i=0;i<n;i+=64)sink+=p[i];
 b=ticks();row(f,"broadway_stride_read",n*32,ms(a,b),1);
}
static void gx_test(FILE *f,u8 *fifo,unsigned n,u8 *cpu){
 GXColor clear={0,0,0,255};
 GX_Init(fifo,n);
 GX_SetCopyClear(clear,0x00ffffff);
 GX_SetViewport(0,0,320,240,0,1);
 GX_SetScissor(0,0,320,240);
 GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
 GX_SetColorUpdate(GX_TRUE);
 GX_SetAlphaUpdate(GX_FALSE);
 GX_SetCullMode(GX_CULL_NONE);
 GX_SetNumChans(1);
 GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
 GX_SetNumTexGens(0);
 GX_SetNumTevStages(1);
 GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
 GX_ClearVtxDesc();
 GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
 GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
 GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
 GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
 Mtx44 proj;guOrtho(proj,0,240,0,320,0,100);GX_LoadProjectionMtx(proj,GX_ORTHOGRAPHIC);
 Mtx identity;guMtxIdentity(identity);GX_LoadPosMtxImm(identity,GX_PNMTX0);
 u64 a=ticks();
 for(unsigned k=0;k<128;k++){
  GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
  GX_Position3f32(20,20,-1);GX_Color4u8(255,0,0,255);
  GX_Position3f32(280,20,-1);GX_Color4u8(0,255,0,255);
  GX_Position3f32(160,200,-1);GX_Color4u8(0,0,255,255);
  GX_End();
 }
 GX_DrawDone();u64 b=ticks();
 row(f,"gx_triangles_sync",128*3,ms(a,b),1);
 a=ticks();
 for(unsigned k=0;k<128;k++){
  GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
  GX_Position3f32(20,20,-1);GX_Color4u8(255,0,0,255);
  GX_Position3f32(280,20,-1);GX_Color4u8(0,255,0,255);
  GX_Position3f32(160,200,-1);GX_Color4u8(0,0,255,255);
  GX_End();
  for(unsigned i=0;i<32768;i+=64)sink+=cpu[i];
 }
 GX_DrawDone();b=ticks();
 row(f,"gx_cpu_contention",128*3,ms(a,b),1);
 GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
 a=ticks();
 for(unsigned k=0;k<256;k++){
  GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
  GX_Position3f32(20,20,-1);GX_Color4u8(255,0,0,128);
  GX_Position3f32(280,20,-1);GX_Color4u8(0,255,0,128);
  GX_Position3f32(160,200,-1);GX_Color4u8(0,0,255,128);
  GX_End();
 }
 GX_DrawDone();b=ticks();row(f,"gx_alpha_blend",256*3,ms(a,b),1);
 GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_CLEAR);
 a=ticks();
 GX_SetAlphaCompare(GX_GREATER,127,GX_AOP_AND,GX_ALWAYS,0);
 for(unsigned k=0;k<256;k++){
  GX_Begin(GX_TRIANGLES,GX_VTXFMT0,3);
  GX_Position3f32(20,20,-1);GX_Color4u8(255,0,0,128);
  GX_Position3f32(280,20,-1);GX_Color4u8(0,255,0,128);
  GX_Position3f32(160,200,-1);GX_Color4u8(0,0,255,128);
  GX_End();
 }
 GX_DrawDone();b=ticks();row(f,"gx_alpha_compare",256*3,ms(a,b),1);
 GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
}
static void arm_test(FILE *f,u8 *p,unsigned n){
 int fd=IOS_Open("/dev/gekko2",0);
 result(f,"gka1_open",0,0,fd>=0?"available":"unavailable",fd,0);
 if(fd<0)return;
 memset(caps,0,sizeof caps);DCFlushRange(caps,sizeof caps);
 int r=IOS_Ioctl(fd,0,NULL,0,caps,sizeof caps);DCInvalidateRange(caps,sizeof caps);
 if(r!=32||caps[0]!=0x474b4131u||caps[1]!=1u||!(caps[2]&2u)){
 result(f,"gka1_caps",0,0,"unsupported",r,0);IOS_Close(fd);return;
 }
 memset(header,0,sizeof header);memcpy(header,"GKA1",4);header[7]=1;header[11]=0xa5;
 memset(p,0x3c,n); /* A successful no-op must fail data verification. */
 vec[0].data=header;vec[0].len=32;vec[1].data=p;vec[1].len=n;
 DCFlushRange(header,32);DCFlushRange(p,n);DCFlushRange(vec,sizeof vec);
 u64 a=ticks();r=IOS_Ioctlv(fd,2,1,1,vec);u64 b=ticks();
 DCInvalidateRange(p,n);
 int ok=r==(int)n;
 for(unsigned i=0;i<n&&ok;i++)if(p[i]!=0xa5)ok=0;
 result(f,"gka1_worker_fill",n,ms(a,b),ok?"passed":"failed",r,ok);
 IOS_Close(fd);
}
int main(void){
 VIDEO_Init();GXRModeObj *mode=VIDEO_GetPreferredMode(NULL);
 void *fb=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 console_init(fb,20,20,mode->fbWidth,mode->xfbHeight,mode->fbWidth*VI_DISPLAY_PIX_SZ);
 VIDEO_Configure(mode);VIDEO_SetNextFramebuffer(fb);VIDEO_SetBlack(FALSE);VIDEO_Flush();VIDEO_WaitVSync();
 printf("GEKKO2 hardware profiler / MLOAD + separate GKA1\n");
 unsigned original=IOS_GetVersion();int rr=0;
 original_ios=original;
 /* Reload before mounting FAT; preserve HBC compatibility by reporting failures. */
 if(IOS_GetVersion()!=222&&ios222_installed()){
  printf("Attempting IOS222 reload...\n");
  rr=IOS_ReloadIOS(222);
  printf("IOS reload result=%d, active=%u\n",rr,(unsigned)IOS_GetVersion());
 }
 else if(original!=222){rr=-1;printf("IOS222 missing/stub: retaining current IOS\n");}
 int mounted=fatInitDefault();FILE *f=NULL;
 if(mounted){f=fopen("sd:/gekko2-profiler.csv","w");if(!f)f=fopen("usb:/gekko2-profiler.csv","w");}
 if(f)fprintf(f,"test,region,work_units,ms,status,ios_result,verified,active_ios,ios_revision,original_ios,destination_hex,source_hex\n");
 printf("IOS original=%u active=%u revision=%u reload=%d FAT=%d CSV=%s\n",original,(unsigned)IOS_GetVersion(),(unsigned)IOS_GetRevision(),rr,mounted,f?"open":"unavailable");
 result(f,"original_ios",original,0,"diagnostic",0,0);
 result(f,"ios_reload",0,0,rr<0?"failed":"completed",rr,0);
 u8 *mem1=memalign(32,1024*1024),*mem2=memalign(32,1024*1024);
 u8 *gx_fifo=memalign(32,256*1024);
 destination_address=(uintptr_t)mem1;source_address=(uintptr_t)mem2;
 region="heap_actual_address";
 if(mem1){printf("buffer1=%p buffer2=%p (heap allocations; no assumed MEM2)\n",(void*)mem1,(void*)mem2);memset(mem1,0,1024*1024);cpu_test(f,mem1,1024*1024);}
 if(mem1&&mem2){region=(uintptr_t)mem1<0x81800000u?"MEM1_destination":"MEM2_destination";mload_test(f,mem1,mem2,1024*1024);arm_test(f,mem1,1024*1024);}
 else result(f,"ram_allocation",0,0,"skipped",-1,0);
 if(mem2){memset(mem2,0,1024*1024);cpu_test(f,mem2,1024*1024);}
 if(gx_fifo&&mem1)gx_test(f,gx_fifo,256*1024,mem1);
 if(mem1&&mem2){
 u64 a=ticks();for(unsigned i=0;i<64;i++)memcpy(mem2,mem1,1024*1024);
 u64 b=ticks();int ok=memcmp(mem1,mem2,1024*1024)==0;
 result(f,"memory_copy",64*1024*1024,ms(a,b),ok?"passed":"failed",0,ok);
 }
 if(f){fflush(f);fclose(f);}
 printf("Finished. CSV on SD/USB if mounted.\n");
 free(mem1);free(mem2);/* GX FIFO remains allocated until process exit */sleep(12);return 0;
}
