#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
int main(void){
 char path[128];snprintf(path,sizeof path,"/tmp/gekko2-r1353-%ld.cfg",(long)getpid());
 unsigned bits=GEKKO2_OPT_BIT(GEKKO2_OPT_EVENT_SCHEDULER)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_IDLE_BATCH)|GEKKO2_OPT_BIT(GEKKO2_OPT_LONG_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_SERVICE_HLE);
 FILE *f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 6 01fd7fff\n",f);fclose(f);
 assert(gekko2_opt_load(path)==0&&!(gekko2_opt_requested()&bits));
 unsigned old=gekko2_optimization_mask;
 for(unsigned n=GEKKO2_OPT_EVENT_SCHEDULER;n<GEKKO2_OPT_COUNT;n++)assert(gekko2_opt_toggle(n));
 assert((gekko2_opt_requested()&bits)==bits&&gekko2_optimization_mask==old);
 assert(gekko2_opt_save(path)==0);unsigned saved=gekko2_opt_requested();
 for(unsigned n=GEKKO2_OPT_EVENT_SCHEDULER;n<GEKKO2_OPT_COUNT;n++)assert(gekko2_opt_toggle(n));
 assert(gekko2_opt_load(path)==0&&gekko2_opt_requested()==saved);
 gekko2_opt_apply();assert(gekko2_optimization_mask==saved);
 f=fopen(path,"w");assert(f);fputs("GEKKO2_OPTIONS 6 0ffd7fff\n",f);fclose(f);
 assert(gekko2_opt_load(path)==-1&&gekko2_opt_requested()==saved);
 remove(path);puts("PASS R1353 cold-boot switches, v6 compatibility, v7 persistence and invalid-version mask rejection");
}
