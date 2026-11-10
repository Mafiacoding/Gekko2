/* Independent write-mask oracle: RGB_ONLY forbids alpha writes, including
 * FBA. Synthetic pixels only; no modeled GX rasterization claim. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw/gif.c"
static unsigned cases,failures;
static uint32_t word(const uint8_t *p)
{return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
    uint8_t pixels[64]={0},tex[64]={0};
    const uint32_t masks[]={0,0xff000000u,0x80000000u,0x00ff0000u};
    const unsigned alphas[]={0,64,128,255};
    pixels[1]=0x11;pixels[32]=0x22;pixels[33]=0x33;
    uint8_t *vram=malloc(GS_MEM_SIZE);
    if(!vram)return 1;
    for(unsigned psm=0;psm<2;psm++)for(unsigned atst=0;atst<8;atst++)
    for(unsigned afail=0;afail<4;afail++)for(unsigned a=0;a<4;a++)
    for(unsigned da=0;da<2;da++)for(unsigned fba=0;fba<2;fba++)
    for(unsigned m=0;m<4;m++) {
        unsigned alpha=alphas[a],ref=64;
        int pass[]={0,1,alpha<ref,alpha<=ref,alpha==ref,alpha>=ref,alpha>ref,alpha!=ref};
        int color=pass[atst]||afail==1||afail==3;
        int depth=pass[atst]||afail==2;
        uint32_t old=(da?0x8a000000u:0x0a000000u)|0x00556677u;
        uint32_t mask=masks[m];
        if(psm==1u||(!pass[atst]&&afail==3u))mask|=0xff000000u;
        uint32_t src=((alpha|(fba?128u:0u))<<24)|0x00332211u;
        uint32_t want=color?((src&~mask)|(old&mask)):old;
        uint32_t want_z=depth?0x12345678u:0x77777777u;
        gif_init();gs_mem_init();
        g_gif.fbw=64;g_gif.frame_psm=psm;g_gif.prim=6;g_gif.prmodecont_ac=1;
        g_gif.zbp=16384;g_gif.zpsm=0;
        g_gif.ate=1;g_gif.atst=atst;g_gif.aref=ref;g_gif.afail=afail;
        g_gif.fba=fba;g_gif.fbmask=masks[m];
        gs_mem_write_psmct32(0,64,0,0,old);
        gs_mem_write_z(16384,64,0,0,0,0x77777777u);
        gs_finish_pixel(0,0,(alpha<<24)|0x00332211u,0x12345678u,1);
        uint32_t cpu=gs_mem_read_psmct32(0,64,0,0);
        uint32_t cpu_z=gs_mem_read_z(16384,64,0,0,0);
        gs_gx_texture_draw d={0};
        d.tw=d.th=d.columns=d.rows=d.coverage.width=d.coverage.height=4;
        d.coverage.bw=64;d.coverage.psm=psm;d.coverage.right[0]=1;
        d.pipeline.zbp=16384;d.pipeline.zwrite=1;d.pipeline.z=0x12345678u;
        d.pipeline.ate=1;d.pipeline.atst=atst;d.pipeline.aref=ref;d.pipeline.afail=afail;
        d.pipeline.fba=fba;d.pipeline.fbmask=masks[m];
        memset(vram,0,GS_MEM_SIZE);
        for(unsigned b=0;b<4;b++)vram[b]=(uint8_t)(old>>(b*8));
        uint32_t off=gs_mem_z_offset(16384,64,0,0,0);
        for(unsigned b=0;b<4;b++)vram[off+b]=0x77;
        tex[0]=(uint8_t)alpha;
        if(!gs_gx_import_texture(vram,GS_MEM_SIZE,pixels,sizeof pixels,tex,sizeof tex,&d))return 1;
        if(cpu!=want||word(vram)!=want||cpu_z!=want_z||word(vram+off)!=want_z){
            if(failures<4)fprintf(stderr,"FAIL psm=%u atst=%u afail=%u a=%u da=%u fba=%u mask=%08x CPU=%08x import=%08x want=%08x\n",psm,atst,afail,alpha,da,fba,masks[m],cpu,word(vram),want);
            failures++;
        }
        cases++;
    }
    free(vram);
    printf("AFAIL/FBA write-mask oracle: %u cases, %u failures\n",cases,failures);
    return failures!=0;
}
