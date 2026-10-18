#include "build_version.h"
#include "core/recompiler/optimization.h"
/* R1269 native launcher. Font rasterized from DejaVu Sans (see license). */
#ifndef FRONTEND_H
#define FRONTEND_H
#include <stdint.h>
#include <stdio.h>
#include "core/hw/frontend_browser.h"
#include "core/hw/runtime_telemetry.h"
#include "core/recompiler/experimental_options.h"
typedef void (*ui_rect_fn)(int,int,int,int,uint8_t,uint8_t,uint8_t);
enum {UI_BIOS,UI_DISC,UI_COLD_BOOT,UI_SELECT_DISC,UI_SETTINGS,UI_ABOUT,UI_EXIT,UI_MENU_COUNT};
enum {UI_SET_RENDERER,UI_SET_SOUND,UI_SET_HUD,UI_SET_UPDATE,UI_SET_FPS,
 UI_SET_PROFILE,UI_SET_EXPERIMENTS,UI_SET_DEVELOPER,UI_SET_NETWORK,UI_SET_DEFAULTS,UI_SET_COUNT};
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
/* The active mask stays frozen until cold boot; requested settings may differ. */
static const char *ui_cpu_engine(uint32_t mask){
 const uint32_t jit=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);
 return !(mask&jit)?"INTERPRETER":(mask&jit)==jit?"JIT ENABLED":"MIXED CPU";
}
static void ui_draw(ui_rect_fn rect,int selected,int page,int running,int hud,int fast,int fps,int gx,uint32_t cpu_mask,const char *notice){
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 /* Architectural light pillars; an original angular wordmark. */
 for(int i=0;i<7;i++){int x=408+i*28;rect(x,76+i*11,2,240-i*17,12,35+i*5,66+i*9);}
 rect(36,35,4,51,67,182,255);
 if(ui_logo_renderer)ui_logo_renderer(54,8);
 else ui_text(rect,54,32,3,"GEKKO2",204,232,255);
 ui_text(rect,344,84,1,"W I I  /  " GEKKO2_RELEASE "",78,163,229);
 /* Geometric 2 motif, inspired by the console's blue line art. */
 rect(526,35,72,3,55,133,225);rect(595,35,3,20,55,133,225);
 rect(526,52,72,3,55,133,225);rect(526,52,3,20,55,133,225);
 rect(526,69,72,3,55,133,225);
 rect(36,110,568,2,24,61,99);
 ui_text(rect,38,129,1,page==1?"SYSTEM SETTINGS":page==2?"ABOUT THIS PROJECT":"PLAYSTATION 2 EMULATION",100,166,215);
 const char *labels[UI_MENU_COUNT]={"BIOS / OSDSYS","START DISC","COLD BOOT / NEW LOG","SELECT DISC","SETTINGS","ABOUT","EXIT TO HBC"};
 const char *desc[UI_MENU_COUNT]={"New BIOS session and log","New selected ISO / BIN session","Restart current mode; keep old logs","Browse files on the SD card","Display and scheduling options","Build and controller information","Return to the Homebrew Channel"};
 if(page==0){
 for(int i=0;i<UI_MENU_COUNT;i++){int y=152+i*32;int on=i==selected;
 rect(36,y,370,31,on?16:9,on?43:22,on?76:41);rect(36,y,3,31,on?65:20,on?183:49,on?255:76);
 ui_text(rect,51,y+3,1,labels[i],on?228:148,on?242:176,on?255:207);
 ui_text(rect,51,y+17,1,desc[i],on?115:75,on?188:116,on?235:157);}
 ui_text(rect,433,173,1,running?"CPU ACTIVE":"CPU NEXT BOOT",91,144,188);ui_text(rect,433,194,1,ui_cpu_engine(cpu_mask),185,221,250);
 ui_text(rect,433,240,1,"SESSION",91,144,188);ui_text(rect,433,261,1,running?"PAUSED":"READY",185,221,250);
 }else if(page==1){
 ui_text(rect,48,171,1,hud?"> HUD: ON":"> HUD: OFF",221,237,255);
 ui_text(rect,48,204,1,fast?"> UPDATE: THROUGHPUT":"> UPDATE: RESPONSIVE",221,237,255);
 ui_text(rect,48,237,1,fps?"> FPS COUNTER: ON":"> FPS COUNTER: OFF",221,237,255);
 ui_text(rect,48,269,1,gx?"> GX (EXPERIMENTAL): ON":"> GX (EXPERIMENTAL): OFF",221,237,255);
 ui_text(rect,48,299,1,"A: HUD    1 / GC X: update    2 / GC Y: FPS",106,171,223);
 ui_text(rect,48,324,1,"LEFT / RIGHT: toggle GX",106,171,223);
 ui_text(rect,48,349,1,"DOWN: CPU / GX OPTIMIZATIONS",106,171,223);
 }else{
 ui_text(rect,48,170,1,"Experimental PS2 emulator for Nintendo Wii",221,237,255);
 ui_text(rect,48,207,1,"Native launcher / devkitPPC / libogc",106,171,223);
 ui_text(rect,48,244,1,"This screen is the emulator launcher.",106,171,223);
 ui_text(rect,48,269,1,"Early alpha: BIOS stability comes first.",106,171,223);
 ui_text(rect,48,310,1,"PLUS/START: pause/resume. MINUS+A: PS2 START.",106,171,223);
 ui_text(rect,48,337,1,"HOME: pause. HOME+MINUS: exit to HBC.",106,171,223);
 }
 rect(36,381,568,1,24,61,99);
 ui_text(rect,38,397,1,notice?notice:"",117,186,237);
 ui_text(rect,38,437,1,page?"B  BACK":"D-PAD/STICK SELECT  A OPEN  +/START RESUME",209,227,246);
}
static void ui_settings_draw(ui_rect_fn rect,int selected,int hud,int throughput,int fps,int gx,const char *message)
{
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 ui_text(rect,36,28,2,"WII SETTINGS",204,232,255);
 ui_text(rect,36,65,1,"Saved on SD. CPU / GX changes: next cold boot.",106,171,223);
 const char *labels[UI_SET_COUNT]={"Renderer","Sound output","HUD","Input update budget","Rate counters","CPU profile","Experiments","Developer switches","Network logger","Restore defaults"};
 const char *values[UI_SET_COUNT]={gx?"GX HYBRID":"SOFTWARE REFERENCE",gekko2_opt_requested()&GEKKO2_OPT_BIT(GEKKO2_OPT_SOUND)?"ON":"OFF",
  hud?"ON":"OFF",throughput?"50 ms (throughput)":"20 ms (responsive)",fps?"ON":"OFF",gekko2_opt_profile_name(),"OPEN","OPEN","OPEN","APPLY"};
 for(int n=0;n<UI_SET_COUNT;n++){
  int y=96+n*24,on=n==selected;char row[100];
  rect(32,y-2,576,23,on?16:9,on?43:22,on?76:41);
  snprintf(row,sizeof row,"%s %-23s %s",on?">":" ",labels[n],values[n]);
  ui_text(rect,40,y,1,row,on?228:148,on?242:176,on?255:207);
 }
 ui_text(rect,36,355,1,selected==UI_SET_PROFILE?
  "INTERPRETER REFERENCE disables EE / IOP / VU JIT.":selected==UI_SET_RENDERER?
  "Renderer choice does not change CPU profile.":
  "Sound OFF mutes output; guest hardware stays active.",117,186,237);
 ui_text(rect,36,375,1,"Rates: guest VBlanks / output presents, not game FPS.",117,186,237);
 ui_text(rect,36,403,1,message?message:"",117,186,237);
 ui_text(rect,36,437,1,"UP/DOWN SELECT   A/LEFT/RIGHT CHANGE   B BACK",209,227,246);
}
static void ui_network_draw(ui_rect_fn rect,int selected,const runtime_telemetry_settings *settings,
 const runtime_telemetry_settings *active,const runtime_telemetry_status *status,const char *message)
{
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 ui_text(rect,32,20,2,"NETWORK LOGGER",204,232,255);
 char row[120];uint32_t ip=settings->endpoint.ip;
 snprintf(row,sizeof row,"PC target: %u.%u.%u.%u : %u (editing)",(unsigned)(ip>>24),(unsigned)((ip>>16)&255),(unsigned)((ip>>8)&255),(unsigned)(ip&255),(unsigned)settings->endpoint.port);
 ui_text(rect,32,58,1,row,117,186,237);
 const char *names[]={"Enable logger","PC IPv4 octet 1","PC IPv4 octet 2","PC IPv4 octet 3","PC IPv4 octet 4","UDP port","SAVE / APPLY","SEND TEST PACKET"};
 for(unsigned n=0;n<RT_EDIT_COUNT;n++){
  int y=85+(int)n*26,on=(int)n==selected;char value[24];
  if(n==RT_EDIT_ENABLE)snprintf(value,sizeof value,"%s",settings->enabled?"ON":"OFF");
  else if(n<=RT_EDIT_IP4)snprintf(value,sizeof value,"%u",(unsigned)((ip>>((RT_EDIT_IP4-n)*8))&255));
  else if(n==RT_EDIT_PORT)snprintf(value,sizeof value,"%u",(unsigned)settings->endpoint.port);
  else snprintf(value,sizeof value,"A");
  rect(28,y-2,580,24,on?16:9,on?43:22,on?76:41);
  snprintf(row,sizeof row,"%s %-25s %s",on?">":" ",names[n],value);ui_text(rect,36,y,1,row,221,237,255);
 }
 ip=status->local_ip;
 snprintf(row,sizeof row,"Wii IP: %u.%u.%u.%u | %s",(unsigned)(ip>>24),(unsigned)((ip>>16)&255),(unsigned)((ip>>8)&255),(unsigned)(ip&255),runtime_telemetry_status_text(status));
 ui_text(rect,32,301,1,row,117,186,237);
 ip=active->endpoint.ip;
 snprintf(row,sizeof row,"Active PC: %u.%u.%u.%u:%u | %s error=%d",(unsigned)(ip>>24),(unsigned)((ip>>16)&255),(unsigned)((ip>>8)&255),(unsigned)(ip&255),(unsigned)active->endpoint.port,runtime_telemetry_stage_text(status->stage),status->error);
 ui_text(rect,32,323,1,row,117,186,237);
 snprintf(row,sizeof row,"Sent=%llu dropped=%llu errors=%llu",(unsigned long long)status->sent,(unsigned long long)status->dropped,(unsigned long long)status->errors);
 ui_text(rect,32,345,1,row,117,186,237);
 ui_text(rect,32,369,1,"UP/DOWN row  LEFT/RIGHT +/-1  1/2 or X/Y +/-10",117,186,237);
 ui_text(rect,32,391,1,"SAVE applies now. Test needs Windows logger 1.1.",117,186,237);
 ui_text(rect,32,413,1,message?message:"",221,237,255);
 ui_text(rect,32,447,1,"Hold LEFT/RIGHT to repeat. B BACK",209,227,246);
}
static void ui_experiments_draw(ui_rect_fn rect,int selected,const char *message)
{
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 ui_text(rect,36,28,2,"EXPERIMENTS - DEFAULT OFF",227,197,114);
 ui_text(rect,36,65,1,"Independent options. Applied only at cold boot.",106,171,223);
 const char *names[GEKKO2_EXP_COUNT]={"128-BIT JIT CHAIN RESIDENCY - EXPERIMENTAL","EXPERIMENTAL PSM CONVERSION","GX RISK MODE - FAST TRIANGLES"};
 const char *desc[GEKKO2_EXP_COUNT]={"Needs EE JIT / blocks / links / GPR residency.","Unverified color-depth transfers; may alter colors.","Direct GX edges can add or lose edge pixels."};
 for(unsigned n=0;n<GEKKO2_EXP_COUNT;n++){
  int y=106+(int)n*81,on=(int)n==selected;char state[100];
  rect(32,y-4,576,72,on?35:9,on?34:22,on?20:41);
  ui_text(rect,40,y,1,names[n],227,197,114);
  snprintf(state,sizeof state,"%s SAVED %s   SESSION %s",on?">":" ",
   !(gekko2_experimental_available()&(1u<<n))?"UNAVAILABLE":gekko2_experimental_requested(n)?"ON":"OFF",
   gekko2_experimental_enabled(n)?"ON":"OFF");
  ui_text(rect,40,y+21,1,state,221,237,255);ui_text(rect,40,y+42,1,desc[n],117,186,237);
 }
 ui_text(rect,36,381,1,"START resumes with the old session configuration.",117,186,237);
 ui_text(rect,36,403,1,message?message:"",117,186,237);
 ui_text(rect,36,437,1,"UP/DOWN SELECT   A/LEFT/RIGHT TOGGLE   B BACK",209,227,246);
}
static void ui_optimization_draw(ui_rect_fn rect,int selected,uint32_t requested,uint32_t available,const char *message)
{
 if(selected<0||selected>=GEKKO2_OPT_COUNT)selected=0;
 for(int y=0;y<480;y++)rect(0,y,640,1,4+y/120,9+y/80,22+y/25);
 ui_text(rect,36,28,2,"DEVELOPER CPU / GX SWITCHES",204,232,255);
 ui_text(rect,36,65,1,"CPU/GX: cold boot. ARM/IOS: exit and restart app.",106,171,223);
 unsigned first=((unsigned)selected/13u)*13u;
 for(unsigned n=first;n<GEKKO2_OPT_COUNT&&n<first+13u;n++){
  int y=92+(int)(n-first)*19,on=(int)n==selected;char row[90];
  rect(32,y-2,576,18,on?16:9,on?43:22,on?76:41);
  snprintf(row,sizeof(row),"%s %-28s %s",on?">":" ",gekko2_opt_name(n),
      !(available&GEKKO2_OPT_BIT(n))?"UNAVAILABLE":n==GEKKO2_OPT_ARM_IOS222?(requested&GEKKO2_OPT_BIT(n)?"IOS222":"CURRENT"):(requested&GEKKO2_OPT_BIT(n)?"ON":"OFF"));
  ui_text(rect,40,y,1,row,on?228:148,on?242:176,on?255:207);
 }
 char session[100];snprintf(session,sizeof session,"SAVED %s   SESSION %s   (CPU/GX: cold boot)",
  requested&GEKKO2_OPT_BIT(selected)?"ON":"OFF",gekko2_opt_enabled((unsigned)selected)?"ON":"OFF");
 ui_text(rect,36,350,1,session,227,197,114);
 ui_text(rect,36,370,1,gekko2_opt_dependency((unsigned)selected,gekko2_optimization_mask),227,197,114);
 char desc[72];snprintf(desc,sizeof desc,"%.70s",gekko2_opt_description((unsigned)selected));
 ui_text(rect,36,389,1,desc,117,186,237);
 ui_text(rect,36,413,1,message?message:"",117,186,237);
 ui_text(rect,36,437,1,"UP/DOWN SELECT   A/LEFT/RIGHT TOGGLE   B BACK",209,227,246);
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
 ui_text(rect,36,438,1,"A SELECT  B PARENT/BACK  C/Z PAGE",209,227,246);
}
#endif
