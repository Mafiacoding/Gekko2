#include <stdio.h>
#include "frontend.h"
static unsigned char pixels[480][640][3];
static void rect(int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b){for(int yy=y;yy<y+h&&yy<480;yy++)for(int xx=x;xx<x+w&&xx<640;xx++)if(xx>=0&&yy>=0){pixels[yy][xx][0]=r;pixels[yy][xx][1]=g;pixels[yy][xx][2]=b;}}
int main(int argc,char **argv){ui_draw(rect,0,0,0,0,1,1,0,"INTERPRETER","SD: pcsx2/bios/ and pcsx2/games/");FILE*f=fopen(argc>1?argv[1]:"frontend.ppm","wb");if(!f)return 1;fprintf(f,"P6\n640 480\n255\n");fwrite(pixels,1,sizeof(pixels),f);fclose(f);return 0;}
