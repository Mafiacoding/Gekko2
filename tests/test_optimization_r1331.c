#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
int main(void)
{
 char path[128];snprintf(path,sizeof(path),"/tmp/gekko2-options-%ld.cfg",(long)getpid());
 uint32_t old=gekko2_optimization_mask;
 assert(gekko2_opt_toggle(GEKKO2_OPT_FASTMEM));
 assert(gekko2_optimization_mask==old); /* queued, no mutation of active execution */
 assert(gekko2_opt_save(path)==0);
 uint32_t next=gekko2_opt_requested();gekko2_opt_toggle(GEKKO2_OPT_FASTMEM);
 assert(gekko2_opt_load(path)==0&&gekko2_opt_requested()==next);
 assert(gekko2_optimization_mask==old);gekko2_opt_apply();assert(gekko2_optimization_mask==next);
 assert(!gekko2_opt_toggle(GEKKO2_OPT_COUNT)&&!gekko2_opt_toggle(500));
 const char *bad[]={"GEKKO2_OPTIONS 5 0\n","GEKKO2_OPTIONS 1 ffffffff\n","GEKKO2_OPTIONS 1 0 extra\n","bad\n"};
 for(unsigned i=0;i<4;i++){
  FILE *f=fopen(path,"w");assert(f);fputs(bad[i],f);fclose(f);
  assert(gekko2_opt_load(path)==-1&&gekko2_opt_requested()==next);
 }
 /* v1 config migration enables only the new build default; v2 preserves OFF. */
 FILE *f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 1 00001dff\n",f);fclose(f);
 assert(gekko2_opt_load(path)==0);
 assert(!!(gekko2_opt_requested()&GEKKO2_OPT_BIT(GEKKO2_OPT_CACHE_REUSE))==!!GEKKO2_OPT_CACHE_DEFAULT);
 f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 2 0\n",f);fclose(f);
 assert(gekko2_opt_load(path)==0&&gekko2_opt_requested()==GEKKO2_OPT_BIT(GEKKO2_OPT_HLE_RAM));
 f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 3 0\n",f);fclose(f);
 assert(gekko2_opt_load(path)==0&&gekko2_opt_requested()==0);
 assert(gekko2_opt_save(path)==0&&gekko2_opt_save(path)==0);remove(path);
 assert(gekko2_opt_save("/nonexistent/gekko2/optimization.cfg")==-1);
 for(unsigned n=0;n<GEKKO2_OPT_COUNT;n++)assert(*gekko2_opt_name(n)&&*gekko2_opt_description(n));
 puts("PASS frozen active settings, persistence, validation, available options and failed save");
}
