/* Software-extension oracle. No claim of PS2 cross-depth conversion. */
#include "core/recompiler/experimental_options.h"
#include <stdio.h>
#include "hw/gs_mem.c"
static unsigned failures,cases;static uint32_t seed=12345;
static uint32_t rnd(void){seed=1664525u*seed+1013904223u;return seed;}
static unsigned width(unsigned p)
{return p==0||p==0x30?32:p==1||p==0x31?24:p==2||p==10||p==0x32||p==0x3a?16:p==0x13||p==0x1b?8:4;}
static unsigned group(unsigned p){return p>=0x30?1:p==0x13||p==0x1b||p==0x14||p==0x24||p==0x2c?2:0;}
static uint32_t bits_mask(unsigned b){return (uint32_t)((1ULL<<b)-1);}
static uint32_t convert(uint32_t v,unsigned a,unsigned b,unsigned g)
{
 if(a==b)return v;
 if(g)return v&bits_mask(b);
 if(a==16){unsigned r=v&31,gr=v>>5&31,bl=v>>10&31;
  /* Existing extension uses bit replication, not normalized division. */
  unsigned alpha=(v&0x8000)?0x80000000u:0;
  v=((r<<3)|(r>>2))|(((gr<<3)|(gr>>2))<<8)|(((bl<<3)|(bl>>2))<<16)|alpha;
 }else if(a==24)v|=0x80000000u;
 if(b==16)return ((v>>3)&31)|(((v>>11)&31)<<5)|(((v>>19)&31)<<10)|((v>>24)>=64?0x8000:0);
 return v&bits_mask(b);
}
int main(void)
{
 const unsigned formats[]={0,1,2,10,0x30,0x31,0x32,0x3a,0x13,0x1b,0x14,0x24,0x2c};
 gs_mem_init();
 for(unsigned chain=0;chain<2;chain++)for(unsigned ext=0;ext<2;ext++){
  if(!gekko2_experimental_request(0,chain)&&chain)continue;
  gekko2_experimental_request(1,ext);gekko2_experimental_apply_cold_boot();
  gs_mem_set_local_extended_conversion(gekko2_experimental_enabled(1));
  for(unsigned a=0;a<13;a++)for(unsigned b=0;b<13;b++)for(unsigned n=0;n<32;n++){
   unsigned sp=formats[a],dp=formats[b],sw=width(sp),dw=width(dp),g=group(sp);
   uint32_t v=rnd()&bits_mask(sw),old=rnd()&bits_mask(dw);unsigned x=(n*7)%64,y=(n*13)%64;
   local_store_pixel(0,128,x,y,sp,v);local_store_pixel(512,128,x,y,dp,old);
   if(dp==1)gs_mem_write_psmct32(512*64,128,x,y,old|0xaa000000u);
   gs_local_copy_desc_t d={0};d.sbp=0;d.dbp=512;d.sbw=d.dbw=128;d.spsm=sp;d.dpsm=dp;
   d.ssax=d.dsax=x;d.ssay=d.dsay=y;d.rrw=d.rrh=1;
   unsigned allowed=sw==dw||(ext&&g==group(dp));
   int status=gs_mem_copy_local(&d);uint32_t actual=local_load_pixel(512,128,x,y,dp);
   uint32_t expected=allowed?convert(v,sw,dw,g):old;
   cases++;if(!!status!=!!allowed||actual!=expected||(dp==1&&(gs_mem_read_psmct32(512*64,128,x,y)>>24)!=0xaa)){
    if(failures<8)printf("FAIL chain=%u psm=%u %x->%x status=%d/%u pixel=%08x/%08x\n",chain,ext,sp,dp,status,allowed,actual,expected);failures++;
   }
  }
 }
 printf("BETA12.21 PSM native/extension and independent chain modes: %u cases, %u failures\n",cases,failures);
 return failures?1:0;
}
