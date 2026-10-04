#include <string.h>
#include "core/hw/frontend_text.h"
#include "core/hw/frontend_font_data.h"
#include "core/hw/gs_wii_output.h"
static unsigned coverage(const char *s,unsigned length,int lx,int ly,unsigned cw,unsigned ch,int large)
{
    if(lx<0||ly<0||(unsigned)ly>=ch||(unsigned)lx/cw>=length)return 0;
    unsigned c=(unsigned char)s[(unsigned)lx/cw];
    if(c<32||c>126)return 0;
    unsigned p=(unsigned)ly*cw+(unsigned)lx%cw;
    uint8_t v=large?frontend_font_1[c-32][p/2]:frontend_font_0[c-32][p/2];
    return p&1?v&15:v>>4;
}
static unsigned mix(unsigned old,unsigned ink,unsigned a)
{return (old*(15-a)+ink*a+7)/15;}
void frontend_text_draw(void *xfb,uint32_t width,uint32_t height,int x,int y,
                       int scale,const char *text,uint8_t r,uint8_t g,uint8_t b)
{
    if(!xfb||!width||!height||(width&1)||!text||(scale!=1&&scale!=3))return;
    unsigned cw=8u*scale,ch=14u*scale,n=(unsigned)strlen(text);
    if(!n||n>512u)return;
    int left=x*(int)width/640,top=y*(int)height/480;
    int right=(x+(int)(cw*n))*(int)width/640,bottom=(y+(int)ch)*(int)height/480;
    if(left<0)left=0;if(top<0)top=0;
    if(right>(int)width)right=width;if(bottom>(int)height)bottom=height;
    if(right<=left||bottom<=top)return;
    left&=~1;right=(right+1)&~1;
    uint32_t ink=gs_rgb8_pair_to_ycbcr(r,g,b,r,g,b),*out=xfb;
    for(int py=top;py<bottom;py++) {
        int ly=(int)(((uint64_t)(2u*py+1u)*480u)/(2u*height))-y;
        for(int px=left;px<right;px+=2) {
            int lx=(int)(((uint64_t)(2u*px+1u)*640u)/(2u*width))-x;
            int rx=(int)(((uint64_t)(2u*px+3u)*640u)/(2u*width))-x;
            unsigned a=coverage(text,n,lx,ly,cw,ch,scale==3);
            unsigned b=coverage(text,n,rx,ly,cw,ch,scale==3);
            if(!(a|b))continue;
            unsigned chroma=(a+b+1)/2;
            uint32_t *p=out+(unsigned)py*(width/2)+(unsigned)px/2,old=*p;
            *p=(mix(old>>24,ink>>24,a)<<24)|
               (mix((old>>16)&255,(ink>>16)&255,chroma)<<16)|
               (mix((old>>8)&255,(ink>>8)&255,b)<<8)|
                mix(old&255,ink&255,chroma);
        }
    }
}
