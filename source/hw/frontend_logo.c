#include "core/hw/frontend_logo.h"
#include "core/hw/frontend_logo_data.h"
#include "core/hw/gs_wii_output.h"
static uint32_t sample(int x,int y)
{
 if(x<0||y<0||(unsigned)x>=FRONTEND_LOGO_WIDTH||(unsigned)y>=FRONTEND_LOGO_HEIGHT)return 0;
 return frontend_logo_pixels[(unsigned)y*FRONTEND_LOGO_WIDTH+(unsigned)x];
}
static unsigned blend(unsigned old,unsigned ink,unsigned a)
{return (old*(255u-a)+ink*a+127u)/255u;}
static unsigned chroma(unsigned old,unsigned a,unsigned b,unsigned ca,unsigned cb)
{return (old*(510u-a-b)+ca*a+cb*b+255u)/510u;}
void frontend_logo_draw(void *xfb,uint32_t width,uint32_t height,int x,int y)
{
 if(!xfb||!width||!height||(width&1u))return;
 int left=x*(int)width/640,top=y*(int)height/480;
 int right=(x+(int)FRONTEND_LOGO_WIDTH)*(int)width/640;
 int bottom=(y+(int)FRONTEND_LOGO_HEIGHT)*(int)height/480;
 if(left<0)left=0;if(top<0)top=0;
 if(right>(int)width)right=width;if(bottom>(int)height)bottom=height;
 if(right<=left||bottom<=top)return;
 left&=~1;right=(right+1)&~1;
 uint32_t *out=xfb;
 for(int py=top;py<bottom;py++) {
  int sy=(int)(((uint64_t)(2u*py+1u)*480u)/(2u*height))-y;
  for(int px=left;px<right;px+=2) {
   int sx=(int)(((uint64_t)(2u*px+1u)*640u)/(2u*width))-x;
   int tx=(int)(((uint64_t)(2u*px+3u)*640u)/(2u*width))-x;
   uint32_t a=sample(sx,sy),b=sample(tx,sy);unsigned aa=a&255u,ab=b&255u;
   if(!(aa|ab))continue;
   uint8_t ar=a>>24,ag=a>>16,az=a>>8,br=b>>24,bg=b>>16,bz=b>>8;
   uint32_t ca=gs_rgb8_pair_to_ycbcr(ar,ag,az,ar,ag,az);
   uint32_t cb=gs_rgb8_pair_to_ycbcr(br,bg,bz,br,bg,bz);
   uint32_t *p=out+(unsigned)py*(width/2)+(unsigned)px/2,old=*p;
   *p=(blend(old>>24,ca>>24,aa)<<24)|
       (chroma((old>>16)&255,aa,ab,(ca>>16)&255,(cb>>16)&255)<<16)|
       (blend((old>>8)&255,(cb>>8)&255,ab)<<8)|
        chroma(old&255,aa,ab,ca&255,cb&255);
  }
 }
}
