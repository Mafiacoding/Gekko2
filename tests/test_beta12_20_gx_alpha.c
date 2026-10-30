/* Targeted GX import parity tests: DATE, source ATE/AFAIL, 24-bit mask.
 * No Wii hardware or texture units are mocked as evidence of gameplay. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "hw/gs_gx.c"
static unsigned tests,failures;
#define CHECK(x) do {tests++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)
static uint32_t pixel(uint8_t *v){return (uint32_t)v[0]|((uint32_t)v[1]<<8)|((uint32_t)v[2]<<16)|((uint32_t)v[3]<<24);}
static void set_pixel(uint8_t *v,uint32_t x){for(unsigned i=0;i<4;i++)v[i]=(uint8_t)(x>>(8*i));}
static int run(uint8_t *vram,gs_gx_texture_draw *d,unsigned dest,unsigned src_alpha)
{
    uint8_t pixels[64]={0},tex[64]={0};
    pixels[1]=0x11;pixels[32]=0x22;pixels[33]=0x33;tex[0]=(uint8_t)src_alpha;
    set_pixel(vram,dest);
    return gs_gx_import_texture(vram,GS_MEM_SIZE,pixels,sizeof pixels,tex,sizeof tex,d);
}
int main(void)
{
    uint8_t *vram=calloc(1,GS_MEM_SIZE);CHECK(vram!=0);if(!vram)return 1;
    gs_gx_texture_draw d={0};
    d.tw=4;d.th=4;d.columns=4;d.rows=4;
    d.coverage.bp=0;d.coverage.bw=64;d.coverage.psm=0;
    d.coverage.width=4;d.coverage.height=4;d.coverage.right[0]=1;
    CHECK(run(vram,&d,0x00112233u,0x20u));CHECK(pixel(vram)==0x20332211u);
    d.pipeline.date=1;d.pipeline.datm=1;
    CHECK(run(vram,&d,0x00112233u,0x20u));CHECK(pixel(vram)==0x00112233u);
    CHECK(run(vram,&d,0x80112233u,0x20u));CHECK(pixel(vram)==0x20332211u);
    d.pipeline.datm=0;
    CHECK(run(vram,&d,0x00112233u,0x20u));CHECK(pixel(vram)==0x20332211u);
    d.coverage.psm=1;d.pipeline.date=1;
    CHECK(run(vram,&d,0x00112233u,0x20u));CHECK(pixel(vram)==0x00112233u);
    d.coverage.psm=0;d.pipeline.date=0;
    d.pipeline.ate=1;d.pipeline.atst=2;d.pipeline.aref=0x40; /* LESS */
    for(unsigned afail=0;afail<4;afail++){
        d.pipeline.afail=afail;
        CHECK(run(vram,&d,0xaabbccddu,0x80u));
        CHECK(pixel(vram)==(afail==0||afail==2?0xaabbccddu:
              afail==3?0xaa332211u:0x80332211u));
    }
    d.pipeline.afail=3;d.pipeline.fba=1;
    CHECK(run(vram,&d,0x0abbccddu,0x80u));CHECK(pixel(vram)==0x0a332211u);
    d.pipeline.fba=0;d.pipeline.ate=0;d.coverage.psm=1;
    CHECK(run(vram,&d,0xaabbccddu,0x55u));CHECK(pixel(vram)==0xaa332211u);
    d.coverage.psm=0;d.pipeline.fbmask=0x00ff0000u;
    CHECK(run(vram,&d,0xaabbccddu,0x55u));CHECK(pixel(vram)==0x55bb2211u);
    /* AFAIL ZB_ONLY writes Z but not RGB. DATE rejection suppresses BOTH. */
    d.pipeline.fbmask=0;d.pipeline.zbp=8192u;d.pipeline.zpsm=0u;
    d.pipeline.ztest=0;d.pipeline.zwrite=1;d.pipeline.z=0x12345678u;
    d.pipeline.ate=1;d.pipeline.atst=0;d.pipeline.afail=2;
    uint32_t zoff=gs_mem_z_offset(d.pipeline.zbp,d.coverage.bw,0,0,d.pipeline.zpsm);
    CHECK(zoff!=UINT32_MAX&&zoff+4u<=GS_MEM_SIZE);
    memset(vram+zoff,0,4);
    CHECK(run(vram,&d,0xaabbccddu,0x80u));
    CHECK(pixel(vram)==0xaabbccddu);
    CHECK(pixel(vram+zoff)==0x12345678u);
    d.pipeline.date=1;d.pipeline.datm=1;
    memset(vram+zoff,0,4);
    CHECK(run(vram,&d,0x0abbccddu,0x80u));
    CHECK(pixel(vram+zoff)==0u);
    d.pipeline.zwrite=0;d.pipeline.date=0;
    /* Seven source-alpha predicates across boundaries. */
    for(unsigned op=0;op<8;op++)for(unsigned alpha=0;alpha<=255;alpha+=17){
        d.pipeline.atst=op;d.pipeline.aref=128;d.pipeline.ate=1;
        int expected=op==0?0:op==1?1:op==2?alpha<128:op==3?alpha<=128:
                     op==4?alpha==128:op==5?alpha>=128:op==6?alpha>128:alpha!=128;
        CHECK(gx_alpha_test_pass(&d.pipeline,alpha)==expected);
    }
    free(vram);
    printf("BETA12.20 GX TEST/DATE/AFAIL/FBMSK: %u checks, %u failures\n",tests,failures);
    return !!failures;
}
