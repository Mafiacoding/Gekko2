#include "core/recompiler/experimental_options.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void write_file(const char *path,const char *s)
{FILE *f=fopen(path,"w");assert(f&&fputs(s,f)>=0&&!fclose(f));}
int main(void)
{
 char dir[]="/tmp/gekko2-options-XXXXXX",chain[256],psm[256],backup[260];assert(mkdtemp(dir));
 snprintf(chain,sizeof chain,"%s/chain.cfg",dir);snprintf(psm,sizeof psm,"%s/gs-compat.cfg",dir);
 unsigned built=gekko2_experimental_available()&1u;
 uint32_t normal=gekko2_optimization_mask;
 assert(!gekko2_experimental_enabled(0)&&!gekko2_experimental_enabled(1));
 assert(!gekko2_experimental_requested(0)&&!gekko2_experimental_requested(1));
 for(unsigned c=0;c<2;c++)for(unsigned p=0;p<2;p++){
  assert(gekko2_experimental_request(0,c)==(!c||built));
  assert(gekko2_experimental_request(1,p));
  gekko2_experimental_apply_cold_boot();
  assert(gekko2_experimental_enabled(0)==(int)(c&&built));
  assert(gekko2_experimental_enabled(1)==(int)p);
  assert(!gekko2_experimental_save(0,chain)&&!gekko2_experimental_save(1,psm));
  assert(gekko2_experimental_request(0,0)&&gekko2_experimental_request(1,!p));
  assert(gekko2_experimental_enabled(1)==(int)p); /* paused session unchanged */
  assert(!gekko2_experimental_load(0,chain)&&!gekko2_experimental_load(1,psm));
  assert(gekko2_experimental_requested(0)==(int)(c&&built));
  assert(gekko2_experimental_requested(1)==(int)p);
  assert(gekko2_optimization_mask==normal);
 }
 const char *bad[]={"GEKKO2_GS_EXTENDED_PSM 2","GEKKO2_GS_EXTENDED_PSM -1", "GEKKO2_GS_EXTENDED_PSM 4294967297", "GEKKO2_GS_EXTENDED_PSM 01", "GEKKO2_GS_EXTENDED_PSM 1 extra", "WRONG 1", ""};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
  assert(gekko2_experimental_request(1,1));write_file(psm,bad[i]);
  assert(gekko2_experimental_load(1,psm)==-1&&!gekko2_experimental_requested(1));
 }
 write_file(psm,"GEKKO2_GS_EXTENDED_PSM 1\n");
 snprintf(backup,sizeof backup,"%s.bak",psm);assert(!rename(psm,backup));
 assert(!gekko2_experimental_load(1,psm)&&gekko2_experimental_requested(1));
 assert(!gekko2_experimental_save(1,psm)&&access(backup,F_OK));
 assert(!gekko2_experimental_request(2,1));
 assert(gekko2_experimental_load(2,psm)==-1);
 assert(gekko2_experimental_save(2,psm)==-1);
 remove(chain);remove(psm);rmdir(dir);
 printf("PASS experimental settings: compiled_chain=%u, four mode combinations, cold-boot isolation, persistence, strict parsing and backup recovery\n",built);
 return 0;
}
