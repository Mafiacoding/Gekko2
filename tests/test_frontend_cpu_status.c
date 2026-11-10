/* Synthetic launcher rendering: saved changes must not relabel a paused CPU. */
#include "frontend.h"
#include <assert.h>
#include <string.h>
static char heading[40],engine[40],hint[120];
static void rect(int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b)
{(void)x;(void)y;(void)w;(void)h;(void)r;(void)g;(void)b;}
static void text(int x,int y,int scale,const char *s,int r,int g,int b)
{
 (void)x;(void)scale;(void)r;(void)g;(void)b;
 if(y==173)snprintf(heading,sizeof heading,"%s",s);
 if(y==194)snprintf(engine,sizeof engine,"%s",s);
 if(y==355)snprintf(hint,sizeof hint,"%s",s);
}
int main(void)
{
 ui_text_renderer=text;
 assert(gekko2_opt_request_profile(GEKKO2_PROFILE_BASE));gekko2_opt_apply();
 uint32_t active=gekko2_optimization_mask;
 assert(!strcmp(ui_cpu_engine(active),"JIT ENABLED"));
 assert(gekko2_opt_request_profile(GEKKO2_PROFILE_INTERPRETER));
 assert(gekko2_optimization_mask==active);
 ui_draw(rect,0,0,1,0,0,0,0,gekko2_optimization_mask,"");
 assert(!strcmp(heading,"CPU ACTIVE")&&!strcmp(engine,"JIT ENABLED"));
 ui_draw(rect,0,0,0,0,0,0,0,gekko2_opt_requested(),"");
 assert(!strcmp(heading,"CPU NEXT BOOT")&&!strcmp(engine,"INTERPRETER"));
 gekko2_opt_apply();
 ui_draw(rect,0,0,1,0,0,0,1,gekko2_optimization_mask,"");
 assert(!strcmp(engine,"INTERPRETER")); /* GX choice cannot relabel CPU */
 for(unsigned bits=1;bits<7;bits++)assert(!strcmp(ui_cpu_engine(bits),"MIXED CPU"));
 ui_settings_draw(rect,UI_SET_PROFILE,0,0,0,0,"");assert(strstr(hint,"disables EE / IOP / VU JIT"));
 ui_settings_draw(rect,UI_SET_RENDERER,0,0,0,1,"");assert(strstr(hint,"does not change CPU profile"));
 puts("PASS launcher active/requested CPU status, frozen cold-boot state, mixed engines and independent renderer hints");
}
