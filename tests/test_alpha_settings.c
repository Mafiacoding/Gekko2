#include "core/hw/frontend_settings.h"
#include "core/recompiler/optimization.h"
#include "core/recompiler/experimental_options.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(void)
{
 char path[128],backup[132];snprintf(path,sizeof path,"/tmp/gekko2-alpha-settings-%ld",(long)getpid());
 snprintf(backup,sizeof backup,"%s.bak",path);
 frontend_settings s;frontend_settings_defaults(&s);assert(!s.hud&&s.throughput&&s.fps&&!s.gx);
 for(unsigned mask=0;mask<16;mask++){
  s=(frontend_settings){mask&1,(mask>>1)&1,(mask>>2)&1,(mask>>3)&1};
  assert(!frontend_settings_save(&s,path));frontend_settings t={0};
  assert(!frontend_settings_load(&t,path)&&!memcmp(&s,&t,sizeof s));
 }
 const char *bad[]={"GEKKO2_FRONTEND 1 1 1 1 2","GEKKO2_FRONTEND 2 0 0 0 0","GEKKO2_FRONTEND 1 0 0 0 0 junk","GEKKO2_FRONTEND 1 -0 0 0 0","GEKKO2_FRONTEND 1 0 0 0","GEKKO2_FRONTEND 1 0 0 0 00"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
  FILE *f=fopen(path,"w");assert(f);fputs(bad[i],f);fclose(f);
  frontend_settings t=s;assert(frontend_settings_load(&s,path)<0&&!memcmp(&s,&t,sizeof s));
 }
 assert(!frontend_settings_save(&s,path));assert(!rename(path,backup));
 frontend_settings t={0};assert(!frontend_settings_load(&t,path)&&!memcmp(&s,&t,sizeof s));remove(backup);
 assert(frontend_settings_save(&s,"/nonexistent/gekko2.cfg")<0);
 FILE *f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 9 00000000\n",f);fclose(f);
 assert(!gekko2_opt_load(path));gekko2_opt_apply();assert(!gekko2_optimization_mask);
 unsigned active=gekko2_optimization_mask;assert(gekko2_opt_request_profile(GEKKO2_PROFILE_BASE));
 assert(gekko2_optimization_mask==active&&!strcmp(gekko2_opt_profile_name(),"WII BASE"));
 assert(!gekko2_opt_request_profile(99));gekko2_opt_apply();assert(gekko2_optimization_mask==gekko2_opt_requested());
 assert(gekko2_opt_request_profile(GEKKO2_PROFILE_INTERPRETER));gekko2_opt_apply();
 assert(!gekko2_opt_enabled(GEKKO2_OPT_EE_JIT)&&!gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT)&&!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT));
 gekko2_experimental_apply_cold_boot();
 for(unsigned i=0;i<GEKKO2_EXP_COUNT;i++)assert(!gekko2_experimental_requested(i)&&!gekko2_experimental_enabled(i));
 remove(path);puts("PASS alpha settings: 16 persisted frontend modes, strict parser/backup, All-OFF under GX test define, profile cold-boot isolation");
}
