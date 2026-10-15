#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
int main(void){char path[128];snprintf(path,sizeof path,"/tmp/gekko2-options-%ld.cfg",(long)getpid());
 FILE *f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 8 00002000\n",f);fclose(f);
 assert(!gekko2_opt_load(path));assert(!(gekko2_opt_requested()&GEKKO2_OPT_BIT(GEKKO2_OPT_SOUND)));
 assert(gekko2_opt_toggle(GEKKO2_OPT_SOUND));assert(!gekko2_opt_save(path));
 assert(gekko2_opt_toggle(GEKKO2_OPT_SOUND));assert(!gekko2_opt_load(path));gekko2_opt_apply();assert(gekko2_opt_enabled(GEKKO2_OPT_SOUND));
 f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 8 80002000\n",f);fclose(f);uint32_t old=gekko2_opt_requested();
 assert(gekko2_opt_load(path)<0&&gekko2_opt_requested()==old);unlink(path);
 puts("PASS old options keep sound OFF; version9 persists bit31 and rejects old-version sound masks");return 0;
}
