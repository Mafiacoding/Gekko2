#include "core/hw/gs_gx_dump.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
int main(void)
{
 char path[]="/tmp/gx-dump-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 gs_gx_dump_reset();
 for(unsigned n=1;n<=20;n++)gs_gx_dump_record(n==20?"WAIT_ENTER":"DRAW",n,2,3,4);
 assert(gs_gx_dump_flush(path));
 FILE *f=fopen(path,"rb");assert(f);char text[4096]={0};size_t size=fread(text,1,sizeof(text)-1,f);assert(size);fclose(f);
 assert(strstr(text,"last_sequence=20"));assert(strstr(text,"seq=5 stage=DRAW"));
 assert(!strstr(text,"seq=4 stage="));assert(strstr(text,"seq=20 stage=WAIT_ENTER"));
 gs_gx_dump_record("WAIT_DONE",0,0,0,0);assert(gs_gx_dump_flush(path));
 f=fopen(path,"rb");assert(f);memset(text,0,sizeof text);assert(fread(text,1,sizeof(text)-1,f));fclose(f);
 assert(strstr(text,"seq=21 stage=WAIT_DONE"));assert(!strstr(text,"seq=5 stage="));
 assert(!gs_gx_dump_flush("/no-such-directory/gekko2/dump"));unlink(path);
 puts("PASS GX durable ring: ordering, wrap, enter/done, replace and failed-path handling");return 0;
}
