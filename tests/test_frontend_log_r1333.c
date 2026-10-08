#define _POSIX_C_SOURCE 200809L
#include "core/hw/frontend_log.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void)
{
 char root[]="/tmp/gekko2-logs-XXXXXX";assert(mkdtemp(root));
 frontend_log a={0},b={0};
 assert(frontend_log_begin(&a,root,0,1)==0);char first[256];strcpy(first,a.path);
 FILE *f=frontend_log_open(&a);assert(f);fputs("PAUSED EE=123\n",f);assert(!frontend_log_close(&a,f));
 f=frontend_log_open(&a);assert(f);fputs("RESUMED EE=123\n",f);assert(!frontend_log_close(&a,f));
 /* A fresh application starts from zero: O_EXCL preserves the older run. */
 assert(frontend_log_begin(&b,root,0,1)==0&&b.sequence==2&&strcmp(first,b.path));
 assert(frontend_log_begin(&a,root,1,1)==0&&a.sequence==3&&strstr(a.path,"disc-gx"));
 char game[256];strcpy(game,a.path);
 f=fopen(first,"r");assert(f);char text[256]={0};assert(fread(text,1,sizeof(text)-1,f)>0);fclose(f);
 assert(strstr(text,"PAUSED EE=123")&&strstr(text,"RESUMED EE=123"));
 assert(frontend_log_begin(&a,first,0,0)<0&&a.errors==1&&!a.path[0]);
 char oversized[300];memset(oversized,'x',sizeof(oversized)-1);oversized[sizeof(oversized)-1]=0;
 assert(frontend_log_begin(&a,oversized,0,0)<0&&!a.path[0]);
 /* Buffered write failure must be reported at close instead of claimed saved. */
 f=fopen("/dev/full","w");assert(f);fputs("test",f);
 assert(frontend_log_close(&b,f)<0&&b.errors&&b.last_error);
 unlink(first);unlink(b.path);
 /* Remove the game log created before the deliberate invalid-path test. */
 unlink(game);
 assert(rmdir(root)==0);
 puts("PASS unique cold-boot logs, same-file resume, restart collisions and write failures");
}
