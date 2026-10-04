/* Host rendering of the exact native launcher; not a Wii screenshot. */
#include <stdio.h>
#include <stdlib.h>
#include "frontend.h"
#include "core/hw/frontend_text.h"
#include "core/hw/gs_wii_output.h"
static unsigned *fb,w,h;
static void rect(int x,int y,int rw,int rh,unsigned char r,unsigned char g,unsigned char b)
{
 int left=x*(int)w/640,top=y*(int)h/480,right=(x+rw)*(int)w/640,bottom=(y+rh)*(int)h/480;
 if(right<=left)right=left+2;if(bottom<=top)bottom=top+1;
 int pw=right-left;if(pw&1)pw++;left&=~1;right=left+pw;
 unsigned ink=gs_rgb8_pair_to_ycbcr(r,g,b,r,g,b);
 for(int yy=top;yy<bottom&&yy<(int)h;yy++)for(int xx=left;xx<right&&xx<(int)w;xx+=2)
  if(xx>=0&&yy>=0)fb[yy*(w/2)+xx/2]=ink;
}
static void text(int x,int y,int scale,const char *s,int r,int g,int b)
{frontend_text_draw(fb,w,h,x,y,scale,s,r,g,b);}
static unsigned char clip(int x){return x<0?0:x>255?255:x;}
int main(int n,char **v)
{
 if(n<4)return 2;w=640;h=atoi(v[3]);fb=calloc(w*h,2);if(!fb)return 3;
 ui_text_renderer=atoi(v[2])?text:NULL;
 if(n>4) {
 frontend_browser browser={0};frontend_browser_entry entries[3]={{"..",1},{"PS2 Games",1},{"Tekken Tag Tournament.BIN",0}};
 browser.entries=entries;browser.count=3;browser.selected=2;
 snprintf(browser.path,sizeof(browser.path),"sd:/pcsx2/games/");
 ui_browser_draw(rect,&browser,"Choose an ISO / BIN file.");
 }else ui_draw(rect,0,0,0,0,1,1,0,"JIT","ALPHA / BIOS FIRST");
 FILE*f=fopen(v[1],"wb");if(!f)return 4;fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
 unsigned p=fb[y*w/2+x/2];int yy=(p>>(x&1?8:24))&255,cb=((p>>16)&255)-128,cr=(p&255)-128;
 unsigned char rgb[3]={clip(yy+((359*cr)>>8)),clip(yy-((88*cb+183*cr)>>8)),clip(yy+((454*cb)>>8))};fwrite(rgb,1,3,f);
 }
 fclose(f);free(fb);return 0;
}
