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
 /* Reload before mounting FAT; preserve HBC compatibility by reporting failures. */
 if(IOS_GetVersion()!=222){
  printf("Attempting IOS222 reload...\\n");
  int rr=IOS_ReloadIOS(222);
  printf("IOS reload result=%d, active=%u\\n",rr,(unsigned)IOS_GetVersion());
 }
 int mounted=fatInitDefault();FILE *f=NULL;
 if(mounted){f=fopen("sd:/gekko2-profiler.csv","w");if(!f)f=fopen("usb:/gekko2-profiler.csv","w");}
 if(f)fprintf(f,"test,bytes,ms,verified\n");
 u8 *mem1=memalign(32,1024*1024),*mem2=memalign(32,1024*1024);
 u8 *gx_fifo=memalign(32,256*1024);
 if(mem1){memset(mem1,0,1024*1024);cpu_test(f,mem1,1024*1024);arm_test(f,mem1,1024*1024);}
 if(mem2){memset(mem2,0,1024*1024);cpu_test(f,mem2,1024*1024);}
 if(gx_fifo&&mem1)gx_test(f,gx_fifo,256*1024,mem1);
 if(mem1&&mem2){
 u64 a=ticks();for(unsigned i=0;i<64;i++)memcpy(mem2,mem1,1024*1024);
 u64 b=ticks();row(f,"memory_copy",64*1024*1024,ms(a,b),memcmp(mem1,mem2,1024*1024)==0);
 }
 if(f){fflush(f);fclose(f);}
 printf("Finished. CSV on SD/USB if mounted.\n");
 free(mem1);free(mem2);/* GX FIFO remains allocated until process exit */sleep(12);return 0;
}
