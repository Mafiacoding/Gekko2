/* Persistence across fresh processes plus FAT-style replacement/rollback. */
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
static int fat_mode,fail_commit;
int __real_rename(const char *,const char *);
int __wrap_rename(const char *src,const char *dst)
{
 if(fat_mode&&access(dst,F_OK)==0){errno=EEXIST;return -1;}
 if(fail_commit&&strstr(src,".tmp")&&access(dst,F_OK)!=0){errno=EIO;return -1;}
 return __real_rename(src,dst);
}
int main(int argc,char **argv)
{
 assert(argc==3);const char *path=argv[2];
 unsigned bits=GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_WORKER)|GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_IOS222);
 if(!strcmp(argv[1],"write")){
  assert(gekko2_opt_toggle(GEKKO2_OPT_ARM_WORKER));assert(gekko2_opt_toggle(GEKKO2_OPT_ARM_IOS222));
  assert(gekko2_opt_save(path)==0);
 }else{
  assert(!(gekko2_optimization_mask&bits));
  assert(gekko2_opt_load(path)==0&&(gekko2_opt_requested()&bits)==bits);
  assert(!(gekko2_optimization_mask&bits));gekko2_opt_apply();
  assert((gekko2_optimization_mask&bits)==bits); /* startup selection now sees both */
  unsigned saved=gekko2_opt_requested();fat_mode=1;
  assert(gekko2_opt_toggle(GEKKO2_OPT_FASTMEM));assert(gekko2_opt_save(path)==0);
  unsigned changed=gekko2_opt_requested();assert(changed!=saved);
  gekko2_opt_toggle(GEKKO2_OPT_FASTMEM);fail_commit=1;
  assert(gekko2_opt_save(path)==-1);fail_commit=0;
  assert(gekko2_opt_load(path)==0&&gekko2_opt_requested()==changed);
  char backup[256];snprintf(backup,sizeof backup,"%s.bak",path);
  assert(__real_rename(path,backup)==0);
  assert(gekko2_opt_load(path)==0&&gekko2_opt_requested()==changed);
  remove(backup);
 }
 puts("PASS ARM/IOS fresh-process persistence, FAT replacement, rollback and backup recovery");
}
