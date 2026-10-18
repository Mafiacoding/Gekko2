/* Standalone Wii microbenchmarks. No emulator, IOS reload or clock changes. */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/ios.h>
#include <ogc/lwp_watchdog.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "kernels.h"
#ifndef LIMITS_COMMIT
#define LIMITS_COMMIT "unknown"
#endif
#define VERSION "GEKKO2-WII-LIMITS-1.1"
#define BLOCK (2u*1024u*1024u)
#define SAMPLES 5u
#define TARGET_US 40000u
#define GX_BATCH 16u
static volatile int quit;
static int gx_failed;
static FILE *log_file;
static GXRModeObj *mode;
static uint32_t sink;
static unsigned failures;
static GXColor draw_color={64,128,192,128};
static void stop(void) { quit=1; }
static void reset_stop(u32 irq,void *ctx) { (void)irq;(void)ctx;quit=1; }
static int input(void)
{
    WPAD_ScanPads();PAD_ScanPads();
    for(int i=0;i<4;i++) if((WPAD_ButtonsDown(i)&WPAD_BUTTON_HOME)||(PAD_ButtonsDown(i)&PAD_BUTTON_START))quit=1;
    return quit;
}
static uint64_t now(void) { __asm__ volatile("sync":::"memory");return gettime(); }
static void result(const char *name,unsigned sample,const char *pool,size_t size,
                   uint64_t iterations,uint64_t units,const char *unit,uint64_t ticks,
                   uint64_t submit,uint64_t wait,int verified,const char *status,uint32_t check)
{
    if(strcmp(status,"ok")==0&&!verified)failures++;
    if(log_file) {
        fprintf(log_file,"%s,%u,%s,%lu,%llu,%llu,%s,%llu,%llu,%llu,%d,%s,%08lx\n",
         name,sample,pool,(unsigned long)size,(unsigned long long)iterations,
         (unsigned long long)units,unit,(unsigned long long)ticks,(unsigned long long)submit,
         (unsigned long long)wait,verified,status,(unsigned long)check);
        if(fflush(log_file)) { fclose(log_file);log_file=NULL;puts("SD write failed; screen results only."); }
    }
    printf("%-23s %u %8.3f ms %s\n",name,sample,(double)ticks_to_microsecs(ticks)/1000.0,status);
}
static int in_pool(void *p,size_t size,uintptr_t low,uintptr_t high)
{ return p&&(uintptr_t)p>=low&&(uintptr_t)p<=high-size; }
static void *mem2_reserve(size_t bytes)
{
    uint32_t level=IRQ_Disable();
    uintptr_t low=((uintptr_t)SYS_GetArena2Lo()+31)&~(uintptr_t)31;
    uintptr_t high=(uintptr_t)SYS_GetArena2Hi();
    void *p=NULL;
    if(low>=0x90000000u&&high<=0x94000000u&&low<=high&&bytes<=high-low) {
        p=(void*)low;SYS_SetArena2Lo((void*)(low+bytes));
    }
    IRQ_Restore(level);return p;
}
static void cpu_tests(void)
{
    for(unsigned kind=0;kind<2&&!input();kind++) {
        uint32_t n=4096;
        for(unsigned trial=0;trial<12;trial++) {
            uint64_t start=now();
            if(kind)sink=(uint32_t)(limits_float(0.75f,n)*1000000.0f);else sink=limits_integer(7,n);
            if(ticks_to_microsecs(now()-start)>=TARGET_US||n>=4194304)break;
            n*=2;
        }
        for(unsigned s=0;s<SAMPLES&&!input();s++) {
            uint32_t value=0;float f=0;
            uint64_t start=now();
            if(kind)f=limits_float(0.75f,n);else value=limits_integer(7,n);
            uint64_t ticks=now()-start;
            int ok=kind?(isfinite(f)&&f>0.0f&&f<3.0f):(value==limits_integer(7,n));
            sink=kind?(uint32_t)(f*1000000.0f):value;
            result(kind?"cpu_float_recurrence":"cpu_integer_recurrence",s,"registers",0,n,n,"iterations",ticks,0,0,ok,"ok",sink);
        }
    }
}
static void cpu_overlap_reference(void)
{
    for(unsigned sample=0;sample<SAMPLES&&!input();sample++) {
        uint64_t begin=now();sink=limits_integer(7,262144);uint64_t elapsed=now()-begin;
        result("cpu_overlap_reference",sample,"registers",0,262144,262144,"iterations",elapsed,0,0,sink==limits_integer(7,262144),"ok",sink);
    }
}
static void memory_tests(uint32_t *a,uint32_t *b,const char *pool)
{
    if(!a||!b){result("memory",0,pool,BLOCK,0,0,"bytes",0,0,0,0,"allocation_failed",0);return;}
    const size_t sizes[]={8192,32768,262144,BLOCK};
    for(unsigned k=0;k<4&&!input();k++) {
        size_t size=sizes[k];limits_fill(a,size);memset(b,0,size);
        /* Warm caches. memcpy has -fno-builtin to preserve every call. */
        memcpy(b,a,size);
        uint32_t repeats=1;
        while(repeats<16384) {
            uint64_t start=now();
            for(uint32_t j=0;j<repeats;j++)memcpy(b,a,size);
            if(ticks_to_microsecs(now()-start)>=TARGET_US)break;
            repeats*=2;
        }
        for(unsigned s=0;s<SAMPLES&&!input();s++) {
            uint64_t start=now();
            for(uint32_t j=0;j<repeats;j++)memcpy(b,a,size);
            uint64_t ticks=now()-start;
            result("memcpy_warm",s,pool,size,repeats,(uint64_t)size*repeats,"payload_bytes",ticks,0,0,limits_verify(b,size),"ok",b[size/4-1]);
        }
        limits_ring(a,size);uint32_t count=65536,index=0;
        while(count<8388608) {
            uint64_t start=now();index=limits_chase(a,0,count);
            if(ticks_to_microsecs(now()-start)>=TARGET_US)break;
            count*=2;
        }
        for(unsigned s=0;s<SAMPLES&&!input();s++) {
            uint64_t start=now();index=limits_chase(a,0,count);uint64_t ticks=now()-start;
            /* Each measured count is divisible by every ring's full period. */
            result("dependent_load_32B",s,pool,size,count,count,"loads",ticks,0,0,index==0,"ok",index);
        }
        for(unsigned s=0;s<SAMPLES&&!input();s++) {
            uint64_t start=now();DCFlushRange(a,size);uint64_t ticks=now()-start;
            result("dc_flush_range",s,pool,size,1,size,"range_bytes",ticks,0,0,0,"timing_only",0);
            /* Dirty every line before the next flush, outside measured interval. */
            for(size_t i=0;i<size/4;i+=8)a[i]^=1u;
        }
    }
}
/* Separate source/destination pools and 32B-aligned versus +4B pointers.
 * Buffers have 32 spare bytes. Counts always mean payload, not bus traffic. */
static void cross_copy_tests(uint32_t *mem1,uint32_t *mem2)
{
    if(!mem1||!mem2){result("cross_copy",0,"MEM1_MEM2",0,0,0,"payload_bytes",0,0,0,0,"allocation_failed",0);return;}
    const size_t sizes[]={32,8192,32768,262144,BLOCK};
    for(unsigned direction=0;direction<2&&!input();direction++)
    for(unsigned offset=0;offset<2&&!input();offset++)
    for(unsigned k=0;k<5&&!input();k++) {
        uint32_t *src=(direction?mem2:mem1)+offset,*dst=(direction?mem1:mem2)+offset;
        size_t size=sizes[k];limits_fill(src,size);memset(dst,0,size);memcpy(dst,src,size);
        unsigned repeats=1;
        while(repeats<16384) {
            uint64_t begin=now();for(unsigned n=0;n<repeats;n++)memcpy(dst,src,size);
            if(ticks_to_microsecs(now()-begin)>=TARGET_US)break;
            repeats*=2;
        }
        for(unsigned sample=0;sample<SAMPLES&&!input();sample++) {
            uint64_t begin=now();for(unsigned n=0;n<repeats;n++)memcpy(dst,src,size);
            uint64_t elapsed=now()-begin;
            result(offset?"cross_copy_offset4":"cross_copy_aligned32",sample,
                direction?"MEM2_to_MEM1":"MEM1_to_MEM2",size,repeats,(uint64_t)size*repeats,
                "payload_bytes",elapsed,0,0,limits_verify(dst,size),"ok",dst[size/4-1]);
        }
    }
}
/* Each batch is <4 KiB; no unbounded stream is fed to a stalled GP. A token
   following DrawDone proves pixel completion even if its callback is lost. */
static int fence(void)
{
    uint16_t token=(uint16_t)(GX_GetDrawSync()+1u);
    GX_SetDrawDone();GX_SetDrawSync(token);GX_Flush();uint64_t start=now();
    while(GX_GetDrawSync()!=token) {
        if(ticks_to_microsecs(now()-start)>500000) {gx_failed=1;return 0;}
    }
    return 1;
}
static void vertex(float x,float y)
{ GX_Position3f32(x,y,0);GX_Color4u8(draw_color.r,draw_color.g,draw_color.b,draw_color.a);GX_TexCoord2f32(x/256.0f,y/256.0f); }
static void quad(int small)
{
    float end=small?8.0f:256.0f;
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    vertex(0,0);vertex(end,0);vertex(end,end);vertex(0,end);GX_End();
}
static void gx_setup(void *fifo)
{
    memset(fifo,0,256*1024);DCFlushRange(fifo,256*1024);GX_Init(fifo,256*1024);
    GX_SetViewport(0,0,256,256,0,1);GX_SetScissor(0,0,256,256);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetCullMode(GX_CULL_NONE);
    Mtx identity;Mtx44 projection;guMtxIdentity(identity);guOrtho(projection,0,256,0,256,-1,1);
    GX_LoadPosMtxImm(identity,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GX_SetNumChans(1);GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
    GX_SetNumTexGens(1);GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetNumTevStages(1);GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);GX_SetDither(GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}
static int color_ok(GXColor c,int blended)
{
    int tolerance=blended?3:1;
    int red=64,green=128,blue=192;
    if(blended) {
        /* Approximate independent blend smoke oracle, not GX rounding proof. */
        red=green=blue=0;
        for(unsigned n=0;n<GX_BATCH;n++) {
            red=(64*32+red*223+127)/255;green=(128*32+green*223+127)/255;blue=(192*32+blue*223+127)/255;
        }
    }
    return abs((int)c.r-red)<=tolerance&&abs((int)c.g-green)<=tolerance&&abs((int)c.b-blue)<=tolerance;
}
static void gx_tests(void)
{
    void *fifo=memalign(32,256*1024),*texture=memalign(32,256*256*4),*copy=memalign(32,256*256*4);
    if(!fifo||!texture||!copy){result("gx",0,"EFB",0,0,0,"pixels",0,0,0,0,"allocation_failed",0);free(fifo);free(texture);free(copy);return;}
    memset(texture,255,256*256*4);DCFlushRange(texture,256*256*4);gx_setup(fifo);
    if(!fence()){result("gx_init",0,"EFB",0,0,0,"pixels",0,0,0,0,"timeout",0);return;}
    const char *names[]={"gx_small_quads","gx_fill_256","gx_blend_256","gx_texture_nearest","gx_texture_linear","gx_cpu_overlap","gx_alpha_reject"};
    for(unsigned kind=0;kind<7&&!gx_failed&&!input();kind++) {
        GX_SetBlendMode(kind==2?GX_BM_BLEND:GX_BM_NONE,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_COPY);
        GX_SetTevOrder(GX_TEVSTAGE0,kind==3||kind==4?GX_TEXCOORD0:GX_TEXCOORDNULL,kind==3||kind==4?GX_TEXMAP0:GX_TEXMAP_NULL,GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0,kind==3||kind==4?GX_MODULATE:GX_PASSCLR);
        if(kind==3||kind==4) {
            GXTexObj obj;GX_InitTexObj(&obj,texture,256,256,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
            GX_InitTexObjLOD(&obj,kind==3?GX_NEAR:GX_LINEAR,kind==3?GX_NEAR:GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
            GX_InvalidateTexAll();GX_LoadTexObj(&obj,GX_TEXMAP0);
        }
        if(!fence()){gx_failed=1;result(names[kind],0,"EFB",0,0,0,"submitted_pixels",0,0,0,0,"timeout",0);break;}
        for(unsigned s=0;s<SAMPLES&&!gx_failed&&!input();s++) {
            if(kind==2||kind==6) {
                GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
                GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
                draw_color=(GXColor){0,0,0,255};quad(0);
                if(!fence()){gx_failed=1;result(names[kind],s,"EFB",0,0,0,"submitted_pixels",0,0,0,0,"timeout",0);break;}
                if(kind==2)GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_COPY);
                else GX_SetAlphaCompare(GX_GEQUAL,192,GX_AOP_AND,GX_ALWAYS,0);
            }
            draw_color=(GXColor){64,128,192,kind==2?32:128};
            uint64_t begin=now();for(unsigned i=0;i<GX_BATCH;i++)quad(kind==0);
            GX_Flush();uint64_t submitted=now();
            if(kind==5)sink=limits_integer(7,262144);
            uint64_t wait_start=now();int ok=fence();uint64_t end=now();
            GXColor pixel={0};if(ok)GX_PeekARGB(2,2,&pixel);
            uint32_t check=(uint32_t)pixel.r<<16|(uint32_t)pixel.g<<8|pixel.b;
            result(names[kind],s,"EFB",0,GX_BATCH,(uint64_t)GX_BATCH*(kind==0?64:65536),"submitted_pixels",end-begin,submitted-begin,end-wait_start,ok&&(kind==6?pixel.r==0&&pixel.g==0&&pixel.b==0:color_ok(pixel,kind==2)),ok?"ok":"timeout",check);
        }
    }
    if(!gx_failed&&!input()) {
        GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
        GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
        draw_color=(GXColor){64,128,192,128};quad(0);
        if(!fence()){result("gx_copy_setup",0,"EFB",0,0,0,"pixels",0,0,0,0,"timeout",0);return;}
        GX_SetTexCopySrc(0,0,256,256);GX_SetTexCopyDst(256,256,GX_TF_RGBA8,GX_FALSE);
        /* Destination dirty cache lines must never overwrite a GP result. */
        DCFlushRange(copy,256*256*4);
        for(unsigned s=0;s<SAMPLES&&!gx_failed&&!input();s++) {
            uint64_t begin=now();GX_CopyTex(copy,GX_FALSE);GX_PixModeSync();GX_Flush();uint64_t submitted=now();
            int ok=fence();uint64_t end=now();if(ok)DCInvalidateRange(copy,256*256*4);
            uint8_t *p=copy;int valid=ok&&abs((int)p[1]-64)<=1&&abs((int)p[32]-128)<=1&&abs((int)p[33]-192)<=1;
            result("gx_copy_rgba8",s,"EFB_to_RAM",256*256*4,1,256*256*4,"payload_bytes",end-begin,submitted-begin,end-submitted,valid,ok?"ok":"timeout",ok?((uint32_t)p[1]<<16|(uint32_t)p[32]<<8|p[33]):0);
        }
        for(unsigned s=0;s<SAMPLES&&!gx_failed&&!input();s++) {
            GXColor c;uint64_t begin=now();for(unsigned i=0;i<1024;i++)GX_PeekARGB(i%256,i/256,&c);uint64_t end=now();
            result("gx_cpu_peek",s,"EFB",0,1024,1024,"pixels",end-begin,0,0,color_ok(c,0),"ok",c.r);
        }
    }
    /* Keep live FIFO and texture allocations until process exit. A timeout
       does not prove GP has stopped using them. No recovery work is submitted. */
}
static void open_log(void)
{
    if(!fatInitDefault()){puts("No FAT device; results remain on screen.");return;}
    if(mkdir("sd:/gekko2-bench",0777)&&errno!=EEXIST){puts("Cannot create sd:/gekko2-bench");return;}
    char path[96];
    for(unsigned i=1;i<10000;i++) {
        snprintf(path,sizeof path,"sd:/gekko2-bench/wii-limits-%04u.csv",i);
        /* Exclusive creation, never overwrite an earlier measurement. */
        log_file=fopen(path,"wx");if(log_file)break;
        if(errno!=EEXIST)break;
    }
    if(!log_file){puts("Cannot create result file.");return;}
    fprintf(log_file,"# schema=1 build=%s source=%s compiler=%s tb_hz=%lu\n",VERSION,LIMITS_COMMIT,__VERSION__,(unsigned long)PPC_TIMER_CLOCK);
    fprintf(log_file,"# ios=%ld video=%lu width=%u height=%u environment=wii_target_hardware_unconfirmed\n",(long)IOS_GetVersion(),(unsigned long)mode->viTVMode,mode->fbWidth,mode->efbHeight);
    fprintf(log_file,"# arena1_bytes=%lu arena2_bytes=%lu samples=%u memory_cache=warm gx_texture_cache=first_cold_then_warm gx_surface=256x256 interrupts=enabled\n",(unsigned long)SYS_GetArena1Size(),(unsigned long)SYS_GetArena2Size(),SAMPLES);
    fputs("test,sample,pool,size_bytes,iterations,work_units,unit,ticks,submit_ticks,wait_ticks,verified,status,checksum\n",log_file);fflush(log_file);
    printf("Results: %s\n",path);
}
int main(void)
{
    VIDEO_Init();mode=VIDEO_GetPreferredMode(NULL);if(!mode)return 1;
    void *xfb=SYS_AllocateFramebuffer(mode);if(!xfb)return 1;xfb=MEM_K0_TO_K1(xfb);
    VIDEO_Configure(mode);VIDEO_SetNextFramebuffer(xfb);VIDEO_SetBlack(FALSE);VIDEO_Flush();VIDEO_WaitVSync();
    CON_Init(xfb,16,16,mode->fbWidth-32,mode->xfbHeight-32,mode->fbWidth*VI_DISPLAY_PIX_SZ);
    WPAD_Init();PAD_Init();SYS_SetResetCallback(reset_stop);SYS_SetPowerCallback(stop);
    puts(VERSION "\nCPU / MEM1 / MEM2 / GX microbenchmarks\nNo clock changes. No game or BIOS needed.\nA: start, HOME / GC START: cancel / exit.");
    while(!input()) {
        int start=0;for(int i=0;i<4;i++)start|=(WPAD_ButtonsDown(i)&WPAD_BUTTON_A)||(PAD_ButtonsDown(i)&PAD_BUTTON_A);
        if(start)break;
        VIDEO_WaitVSync();
    }
    if(quit)return 0;
    open_log();cpu_tests();cpu_overlap_reference();
    uint32_t *a=memalign(32,BLOCK+32),*b=memalign(32,BLOCK+32);
    if(!in_pool(a,BLOCK+32,0x80000000u,0x81800000u)||!in_pool(b,BLOCK+32,0x80000000u,0x81800000u)) {
        free(a);free(b);a=b=NULL;
    }
    memory_tests(a,b,"MEM1_cached");
    uint32_t *second=mem2_reserve(2*(BLOCK+32));
    memory_tests(second,second?second+BLOCK/4+8:NULL,"MEM2_cached");
    cross_copy_tests(a,second);free(a);free(b);
    if(!quit)gx_tests();
    printf("\n%s. Check failures=%u, GX timeout=%d\n",quit?"Cancelled":"Finished",failures,gx_failed);
    if(log_file) {fprintf(log_file,"# end=%s check_failures=%u gx_timeout=%d sink=%08lx\n",quit?"cancelled":"finished",failures,gx_failed,(unsigned long)sink);fclose(log_file);}
    puts("Send the CSV privately. HOME / GC START: exit.");
    while(!input())VIDEO_WaitVSync();
    return 0;
}
