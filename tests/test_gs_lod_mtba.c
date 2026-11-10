/* Synthetic GS texture layout, independently computed colors and Q powers. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned checks;
static uint32_t color(unsigned l,int x,int y)
{return 0x80000000u|((l*25u+x*3u+y*5u)&255u)|(((x*7u+l*11u)&255u)<<8)|(((y*13u+l*17u)&255u)<<16);}
static int coord(int v,unsigned size){v%=(int)size;return v<0?v+(int)size:v;}
static uint32_t oracle_level(unsigned level,double u,double v,int linear)
{
 double scale=ldexp(1.0,-(int)level);u*=scale;v*=scale;
 unsigned size=64u>>level;if(!size)size=1;
 if(!linear)return color(level,coord((int)floor(u),size),coord((int)floor(v),size));
 u-=0.5;v-=0.5;int x=(int)floor(u),y=(int)floor(v);
 unsigned fx=(unsigned)((u-x)*16),fy=(unsigned)((v-y)*16);uint32_t out=0;
 for(unsigned channel=0;channel<4;channel++){
  unsigned tap[4];for(unsigned i=0;i<4;i++)tap[i]=(color(level,coord(x+(i%2),size),coord(y+(i/2),size))>>(8*channel))&255;
  unsigned top=(tap[0]*(16-fx)+tap[1]*fx)/16,bottom=(tap[2]*(16-fx)+tap[3]*fx)/16;
  out|=((top*(16-fy)+bottom*fy)/16)<<(channel*8);
 }
 return out;
}
static uint32_t oracle(double u,double v,double lod,unsigned mxl,unsigned mode,unsigned mag)
{
 int linear=lod<=0?mag:(mode==1||mode==4||mode==5);
 if(!mxl||mode<2||mode>5||lod<=0)return oracle_level(0,u,v,linear);
 if(lod>mxl)lod=mxl;
 unsigned level=(unsigned)(mode%2?floor(lod):floor(lod+0.5));
 uint32_t a=oracle_level(level,u,v,linear);
 if(!(mode%2)||level==mxl)return a;
 uint32_t b=oracle_level(level+1,u,v,linear),out=0;unsigned f=(unsigned)((lod-level)*32768);
 for(unsigned c=0;c<4;c++)out|=((((a>>(8*c))&255)*(32768-f)+((b>>(8*c))&255)*f)/32768)<<(8*c);
 return out;
}
static void sampling(void)
{
 gif_init();gs_mem_init();g_gif.tex_tw=g_gif.tex_th=6;g_gif.tex_tbp0=1024;g_gif.tex_tbw=64;g_gif.clamp_configured=1;
 for(unsigned l=0;l<7;l++){
  if(l){g_gif.tex_mip_tbp[l-1]=1024+l*256;g_gif.tex_mip_tbw[l-1]=64;}
  unsigned size=64>>l;
  for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++)gs_mem_write_psmct32_blk(1024+l*256,64,x,y,color(l,x,y));
 }
 const double uv[][2]={{12.75,6.5},{-3.25,17.25},{63.75,-0.25},{0.0,0.5}};
 for(unsigned mode=0;mode<6;mode++)for(unsigned mag=0;mag<2;mag++)for(unsigned mxl=0;mxl<=6;mxl++)for(int k=-16;k<=104;k+=4)for(unsigned pos=0;pos<4;pos++){
  g_gif.tex1_lcm=1;g_gif.tex1_mmin=mode;g_gif.tex1_mmag=mag;g_gif.tex1_mxl=mxl;g_gif.tex1_k=k;
  uint32_t expected=oracle(uv[pos][0],uv[pos][1],k/16.0,mxl,mode,mag);
  gif_state_t before=g_gif;
  uint32_t actual=gs_sample_texture_q(uv[pos][0],uv[pos][1],0.125);
  if(actual!=expected){fprintf(stderr,"mode=%u mag=%u mxl=%u k=%d pos=%u got=%08x expected=%08x clamp=%u/%u\n",mode,mag,mxl,k,pos,actual,expected,g_gif.clamp_wms,g_gif.clamp_wmt);abort();}
  assert(!memcmp(&before,&g_gif,sizeof before));checks++;
 }
 g_gif.prim=PRIM_TME_MASK;g_gif.tex1_lcm=0;g_gif.tex1_mxl=6;g_gif.tex1_mmin=3;
 for(unsigned l=0;l<4;l++)for(int exp=-2;exp<=5;exp++)for(int k=-16;k<=32;k+=8){
  g_gif.tex1_l=l;g_gif.tex1_k=k;double q=ldexp(1.0,-exp),lod=exp*(1<<l)+k/16.0;
  assert(gs_sample_texture_q(12.75,6.5,q)==oracle(12.75,6.5,lod,6,3,g_gif.tex1_mmag));
  assert(gs_sample_texture_q(12.75,6.5,-q)==oracle(12.75,6.5,lod,6,3,g_gif.tex1_mmag));checks+=2;
 }
 /* FST uses constant K and does not infer LOD from primitive size or Q. */
 g_gif.prim|=PRIM_FST_MASK;g_gif.tex1_k=24;
 assert(gs_sample_texture_q(12.75,6.5,1.0/256)==oracle(12.75,6.5,1.5,6,3,g_gif.tex1_mmag));checks++;
}
static void raster(void)
{
 gif_init();gs_mem_init();
 for(unsigned l=0;l<7;l++)for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)
  gs_mem_write_psmct32_blk(1024+l*256,64,x,y,color(l,0,0));
 apply_ad_write(GS_REG_FRAME_1,1u<<16,0);
 apply_ad_write(GS_REG_TEX0_1,1024u|(1u<<14)|(6u<<26)|(2u<<30),1u|4u|8u);
 apply_ad_write(GS_REG_TEX1_1,(6u<<2)|(2u<<6),0);
 apply_ad_write(GS_REG_CLAMP_1,0,0);
 for(unsigned l=0;l<6;l++){g_gif.ctx1_tex_mip_tbp[l]=1280+l*256;g_gif.ctx1_tex_mip_tbw[l]=64;}
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_TRIANGLE|PRIM_TME_MASK,0);
 rasterize_triangle(0,0,8,0,0,8,0x80808080,0x80808080,0x80808080,
  0,0,0,0,0,0, 0,0,1,0,0,0.125f,0,0,1, 0,0,0,255,255,255);
 assert(gs_mem_read_psmct32(0,64,1,1)==color(0,0,0));
 assert(gs_mem_read_psmct32(0,64,6,1)==color(2,0,0));
 apply_ad_write(GS_REG_SCISSOR_1,5u|(7u<<16),7u<<16);
 rasterize_triangle(0,0,8,0,0,8,0x80808080,0x80808080,0x80808080,
  0,0,0,0,0,0, 0,0,1,0,0,0.125f,0,0,1, 0,0,0,255,255,255);
 assert(gs_mem_read_psmct32(0,64,6,1)==color(2,0,0));
 apply_ad_write(GS_REG_SCISSOR_1,7u<<16,7u<<16);
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_SPRITE|PRIM_TME_MASK,0);
 rasterize_sprite(0,0,8,8,0,0,0,0,0,0,1,0,0,0.25f,0,0,255,255);
 assert(gs_mem_read_psmct32(0,64,1,1)==color(2,0,0));checks+=4;
}
static void mtba(void)
{
 const unsigned psm[]={0,1,2,10,0x13,0x14,0x1b,0x24,0x2c,0x30,0x31,0x32,0x3a};
 for(unsigned context=0;context<2;context++)for(unsigned f=0;f<13;f++)for(unsigned tw=0;tw<16;tw++)for(unsigned enable=0;enable<2;enable++){
  gif_init();unsigned bpp=psm[f]==2||psm[f]==10?16:psm[f]==0x13?8:psm[f]==0x14?4:32;
  uint32_t *bp=context?g_gif.ctx2_tex_mip_tbp:g_gif.ctx1_tex_mip_tbp;
  uint32_t *bw=context?g_gif.ctx2_tex_mip_tbw:g_gif.ctx1_tex_mip_tbw;
  for(unsigned i=0;i<6;i++){bp[i]=111+i;bw[i]=192+i;}
  unsigned reg=context?GS_REG_TEX0_2:GS_REG_TEX0_1;
  apply_ad_write(context?GS_REG_TEX1_2:GS_REG_TEX1_1,(enable<<9)|(6<<2),0);
  apply_ad_write(reg,16380u|(3u<<14)|(psm[f]<<20)|(tw<<26),3);
  int triggered=enable&&f<9&&tw>=5&&tw<=(bpp==32?9:10);
  unsigned width=1u<<tw,base=16380,bytes=width*width*bpp/8,stride=width/64;if(!stride)stride=1;
  for(unsigned i=0;i<6;i++){
   if(triggered&&i<3){base=(base+bytes/256)&16383;stride=stride>1?stride/2:1;
    assert(bp[i]==base&&bw[i]==stride*64);bytes=bytes/4;if(bytes<256)bytes=256;
   }else assert(bp[i]==111+i&&bw[i]==192+i);
  }
  uint32_t saved[6];memcpy(saved,bp,sizeof saved);
  apply_ad_write(context?GS_REG_TEX2_2:GS_REG_TEX2_1,0,0);assert(!memcmp(saved,bp,sizeof saved));checks++;
 }
}
int main(void){sampling();raster();mtba();printf("PASS GS Q/K/L, six filters, mip coordinates/trilinear/state preservation and MTBA/TEX2/context/wrap: %u checks\n",checks);return 0;}
