/* R1269 native launcher. Font rasterized from DejaVu Sans (see license). */
#ifndef FRONTEND_H
#define FRONTEND_H
#include <stdint.h>
#include <stdio.h>
#include "core/hw/frontend_browser.h"
typedef void (*ui_rect_fn)(int,int,int,int,uint8_t,uint8_t,uint8_t);
static const uint8_t ui_font[95][14]={
{0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,4,4,4,4,4,4,0,4,4,0,0,0,0},
{0,10,10,10,0,0,0,0,0,0,0,0,0,0},
{0,0,144,80,252,72,72,254,40,36,0,0,0,0},
{0,16,56,84,20,28,112,80,84,56,16,16,0,0},
{0,134,73,73,41,182,80,72,72,132,0,0,0,0},
{0,24,36,4,12,20,34,194,70,188,0,0,0,0},
{0,2,2,2,0,0,0,0,0,0,0,0,0,0},
{12,4,4,2,2,2,2,2,4,4,12,0,0,0},
{6,4,4,8,8,8,8,8,4,4,6,0,0,0},
{0,8,42,28,28,42,8,0,0,0,0,0,0,0},
{0,0,0,16,16,16,254,16,16,16,0,0,0,0},
{0,0,0,0,0,0,0,0,2,2,2,0,0,0},
{0,0,0,0,0,0,14,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,2,2,0,0,0,0},
{0,8,8,4,4,4,2,2,2,1,1,0,0,0},
{0,60,36,66,66,66,66,66,36,60,0,0,0,0},
{0,14,8,8,8,8,8,8,8,62,0,0,0,0},
{0,60,98,64,64,32,16,8,4,126,0,0,0,0},
{0,60,66,64,64,56,64,64,66,60,0,0,0,0},
{0,48,48,40,36,36,34,126,32,32,0,0,0,0},
{0,62,2,2,62,96,64,64,98,60,0,0,0,0},
{0,56,68,2,58,102,66,66,100,60,0,0,0,0},
{0,126,64,32,32,16,16,8,8,4,0,0,0,0},
{0,60,66,66,66,60,66,66,66,60,0,0,0,0},
{0,60,38,66,66,102,92,64,34,28,0,0,0,0},
{0,0,0,0,2,2,0,0,2,2,0,0,0,0},
{0,0,0,0,2,2,0,0,2,2,2,0,0,0},
{0,0,0,128,240,14,14,240,128,0,0,0,0,0},
{0,0,0,0,0,254,0,254,0,0,0,0,0,0},
{0,0,0,6,60,192,192,60,6,0,0,0,0,0},
{0,14,17,16,8,4,4,0,4,4,0,0,0,0},
{0,240,8,4,226,18,18,18,226,4,8,240,0,0},
{0,24,24,36,36,36,66,126,66,129,0,0,0,0},
{0,62,66,66,66,62,66,66,66,62,0,0,0,0},
{0,56,68,2,2,2,2,2,68,56,0,0,0,0},
{0,62,66,130,130,130,130,130,66,62,0,0,0,0},
{0,126,2,2,2,126,2,2,2,126,0,0,0,0},
{0,62,2,2,2,62,2,2,2,2,0,0,0,0},
{0,120,132,2,2,226,130,130,132,120,0,0,0,0},
{0,130,130,130,130,254,130,130,130,130,0,0,0,0},
{0,2,2,2,2,2,2,2,2,2,0,0,0,0},
{0,2,2,2,2,2,2,2,2,2,2,1,0,0},
{0,66,34,18,10,6,10,18,34,66,0,0,0,0},
{0,2,2,2,2,2,2,2,2,62,0,0,0,0},
{0,2,134,134,74,74,50,50,2,2,0,0,0,0},
{0,134,134,138,138,146,162,162,194,194,0,0,0,0},
{0,56,68,130,130,130,130,130,68,56,0,0,0,0},
{0,62,66,66,66,62,2,2,2,2,0,0,0,0},
{0,56,68,130,130,130,130,130,68,56,32,64,0,0},
{0,62,66,66,66,62,34,66,66,130,0,0,0,0},
{0,60,66,2,2,60,64,64,66,60,0,0,0,0},
{0,127,8,8,8,8,8,8,8,8,0,0,0,0},
{0,130,130,130,130,130,130,130,198,124,0,0,0,0},
{0,129,129,66,66,66,36,36,24,24,0,0,0,0},
{0,33,34,34,82,84,84,84,136,136,0,0,0,0},
{0,99,34,20,20,8,20,20,34,65,0,0,0,0},
{0,65,34,34,20,20,8,8,8,8,0,0,0,0},
{0,254,128,64,32,16,8,4,2,254,0,0,0,0},
{0,12,4,4,4,4,4,4,4,4,4,12,0,0},
{0,1,1,2,2,2,4,4,4,8,8,0,0,0},
{0,6,4,4,4,4,4,4,4,4,4,6,0,0},
{0,48,72,132,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,63,0},
{4,8,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,60,66,64,124,66,98,92,0,0,0,0},
{2,2,2,62,102,66,66,66,102,62,0,0,0,0},
{0,0,0,28,38,2,2,2,38,28,0,0,0,0},
{64,64,64,124,102,66,66,66,102,124,0,0,0,0},
{0,0,0,60,102,66,126,2,70,60,0,0,0,0},
{12,2,2,15,2,2,2,2,2,2,0,0,0,0},
{0,0,0,124,102,66,66,66,102,124,64,100,56,0},
{2,2,2,58,70,66,66,66,66,66,0,0,0,0},
{0,2,0,2,2,2,2,2,2,2,0,0,0,0},
{0,2,0,2,2,2,2,2,2,2,2,2,3,0},
{2,2,2,34,18,10,6,10,18,34,0,0,0,0},
{2,2,2,2,2,2,2,2,2,2,0,0,0,0},
{0,0,0,222,34,34,34,34,34,34,0,0,0,0},
{0,0,0,58,70,66,66,66,66,66,0,0,0,0},
{0,0,0,60,102,66,66,66,102,60,0,0,0,0},
{0,0,0,62,102,66,66,66,102,62,2,2,2,0},
{0,0,0,124,102,66,66,66,102,124,64,64,64,0},
{0,0,0,26,6,2,2,2,2,2,0,0,0,0},
{0,0,0,28,34,2,28,32,34,28,0,0,0,0},
{0,2,2,15,2,2,2,2,2,14,0,0,0,0},
{0,0,0,66,66,66,66,66,98,92,0,0,0,0},
{0,0,0,16,16,9,9,9,6,6,0,0,0,0},
{0,0,0,17,17,170,170,170,68,68,0,0,0,0},
{0,0,0,33,18,18,12,18,18,33,0,0,0,0},
{0,0,0,16,16,9,9,10,6,4,4,2,1,0},
{0,0,0,31,16,8,4,2,1,31,0,0,0,0},
{0,112,16,16,16,16,12,16,16,16,16,112,0,0},
{0,4,4,4,4,4,4,4,4,4,4,4,4,0},
{0,14,8,8,8,8,48,8,8,8,8,14,0,0},
{0,0,0,0,28,226,0,0,0,0,0,0,0,0}};
typedef void (*ui_text_fn)(int,int,int,const char*,int,int,int);
static ui_text_fn ui_text_renderer;
typedef void (*ui_logo_fn)(int,int);
static ui_logo_fn ui_logo_renderer;
static void ui_text(ui_rect_fn rect,int x,int y,int scale,const char *s,int r,int g,int b){
 if(ui_text_renderer){ui_text_renderer(x,y,scale,s,r,g,b);return;}
 for(;*s;s++,x+=8*scale){unsigned c=(unsigned char)*s;if(c<32||c>126)continue;
 for(int row=0;row<14;row++)for(int col=0;col<8;col++)if(ui_font[c-32][row]&(1<<col))rect(x+col*scale,y+row*scale,scale,scale,r,g,b);}}
static void ui_draw(ui_rect_fn rect,int selected,int page,int running,int hud,int fast,int fps,int gx,const char *engine,const char *notice){
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 /* Architectural light pillars; an original angular wordmark. */
 for(int i=0;i<7;i++){int x=408+i*28;rect(x,76+i*11,2,240-i*17,12,35+i*5,66+i*9);}
 rect(36,35,4,51,67,182,255);
 if(ui_logo_renderer)ui_logo_renderer(54,8);
 else ui_text(rect,54,32,3,"GEKKO2",204,232,255);
 ui_text(rect,344,84,1,"W I I  /  R1304",78,163,229);
 /* Geometric 2 motif, inspired by the console's blue line art. */
 rect(526,35,72,3,55,133,225);rect(595,35,3,20,55,133,225);
 rect(526,52,72,3,55,133,225);rect(526,52,3,20,55,133,225);
 rect(526,69,72,3,55,133,225);
 rect(36,110,568,2,24,61,99);
 ui_text(rect,38,129,1,page==1?"SYSTEM SETTINGS":page==2?"ABOUT THIS PROJECT":"PLAYSTATION 2 EMULATION",100,166,215);
 const char *labels[6]={"BIOS / OSDSYS","START DISC","SELECT DISC","SETTINGS","ABOUT","EXIT TO HBC"};
 const char *desc[6]={"Cold boot without a disc","Boot the selected ISO / BIN","Browse files on the SD card","Display and scheduling options","Build and controller information","Return to the Homebrew Channel"};
 if(page==0){
 for(int i=0;i<6;i++){int y=152+i*37;int on=i==selected;
 rect(36,y,370,33,on?16:9,on?43:22,on?76:41);rect(36,y,3,33,on?65:20,on?183:49,on?255:76);
 ui_text(rect,51,y+5,1,labels[i],on?228:148,on?242:176,on?255:207);
 ui_text(rect,51,y+18,1,desc[i],on?115:75,on?188:116,on?235:157);}
 ui_text(rect,433,173,1,"ENGINE",91,144,188);ui_text(rect,433,194,1,engine,185,221,250);
 ui_text(rect,433,240,1,"SESSION",91,144,188);ui_text(rect,433,261,1,running?"PAUSED":"READY",185,221,250);
 }else if(page==1){
 ui_text(rect,48,171,1,hud?"> HUD: ON":"> HUD: OFF",221,237,255);
 ui_text(rect,48,204,1,fast?"> UPDATE: THROUGHPUT":"> UPDATE: RESPONSIVE",221,237,255);
 ui_text(rect,48,237,1,fps?"> FPS COUNTER: ON":"> FPS COUNTER: OFF",221,237,255);
 ui_text(rect,48,269,1,gx?"> GX (EXPERIMENTAL): ON":"> GX (EXPERIMENTAL): OFF",221,237,255);
 ui_text(rect,48,299,1,"A: HUD    1 / GC X: update    2 / GC Y: FPS",106,171,223);
 ui_text(rect,48,324,1,"LEFT / RIGHT: toggle GX",106,171,223);
 ui_text(rect,48,349,1,"MINUS+PLUS: HUD    HOME: pause to launcher.",106,171,223);
 }else{
 ui_text(rect,48,170,1,"Experimental PS2 emulator for Nintendo Wii",221,237,255);
 ui_text(rect,48,207,1,"Native launcher / devkitPPC / libogc",106,171,223);
 ui_text(rect,48,244,1,"This screen is the emulator launcher.",106,171,223);
 ui_text(rect,48,269,1,"Early alpha: BIOS stability comes first.",106,171,223);
 ui_text(rect,48,310,1,"HOME: pause   HOME + MINUS: exit to HBC.",106,171,223);
 }
 rect(36,381,568,1,24,61,99);
 ui_text(rect,38,397,1,notice?notice:"",117,186,237);
 ui_text(rect,38,437,1,page?"B  BACK":"D-PAD/STICK SELECT  A OPEN  +/START RESUME",209,227,246);
}
static void ui_browser_draw(ui_rect_fn rect,const frontend_browser *browser,const char *message)
{
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 ui_text(rect,36,26,3,"SELECT DISC",204,232,255);
 char line[72];snprintf(line,sizeof(line),"%.68s",browser->path);
 ui_text(rect,36,89,1,line,100,166,215);
 unsigned first=browser->selected>=8?browser->selected-7:0;
 for(unsigned i=first;i<browser->count&&i<first+8;i++) {
  int y=122+(i-first)*30;int on=i==browser->selected;
  rect(36,y,568,27,on?16:9,on?43:22,on?76:41);
  snprintf(line,sizeof(line),"%s %.64s",browser->entries[i].directory?"[DIR]":"[IMG]",browser->entries[i].name);
  ui_text(rect,45,y+6,1,line,on?228:148,on?242:176,on?255:207);
 }
 if(!browser->count)ui_text(rect,45,133,1,"No ISO / BIN files in this folder.",148,176,207);
 snprintf(line,sizeof(line),"%u / %u%s",browser->count?browser->selected+1:0,browser->count,browser->truncated?"  (memory limit)":"");
 ui_text(rect,36,373,1,line,100,166,215);
 if(message)ui_text(rect,36,402,1,message,209,227,246);
 ui_text(rect,36,438,1,"A SELECT  B PARENT  C/Z PAGE  +/START BACK",209,227,246);
}
#endif
