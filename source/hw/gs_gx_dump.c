/* Durable operation trace. No exception handler or DSI handler replacement. */
#include "core/hw/gs_gx_dump.h"
#include "build_version.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
static struct {const char *stage;uint32_t seq,a,b,c,d;} ring[16];
static uint32_t sequence;
void gs_gx_dump_reset(void){sequence=0;memset(ring,0,sizeof ring);}
void gs_gx_dump_record(const char *stage,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
 uint32_t n=++sequence,i=(n-1u)&15u;
 ring[i].stage=stage;ring[i].seq=n;ring[i].a=a;ring[i].b=b;ring[i].c=c;ring[i].d=d;
}
int gs_gx_dump_flush(const char *path)
{
 if(!sequence)return 1;
 const char *target=path?path:"sd:/pcsx2/logs/" GEKKO2_RELEASE "-gx-dump.txt";
 char temporary[512];int n=snprintf(temporary,sizeof temporary,"%s.tmp",target);
 if(n<0||(unsigned)n>=sizeof temporary)return 0;
 FILE *f=fopen(temporary,"wb");
 if(!f)return 0;
 int ok=fprintf(f,"GX_DUMP build=" GEKKO2_RELEASE " last_sequence=%lu resident_blend=disabled\n",(unsigned long)sequence)>0;
 uint32_t first=sequence>16u?sequence-15u:1u;
 for(uint32_t n=first;n<=sequence;n++) {
  unsigned i=(n-1u)&15u;
  if(fprintf(f,"seq=%lu stage=%s a=%08lx b=%08lx c=%08lx d=%08lx\n",(unsigned long)n,ring[i].stage,
   (unsigned long)ring[i].a,(unsigned long)ring[i].b,(unsigned long)ring[i].c,(unsigned long)ring[i].d)<0)ok=0;
 }
 if(fflush(f)!=0)ok=0;
 if(fsync(fileno(f))!=0)ok=0;
 if(fclose(f)!=0)ok=0;
 if(ok&&rename(temporary,target)!=0) {
  /* libfat can refuse replacing an existing file. Retain the previous
   * complete dump as .bak, matching the project's config save protocol. */
  char backup[512];int count=snprintf(backup,sizeof backup,"%s.bak",target);
  if(count<0||(unsigned)count>=sizeof backup)ok=0;
  else {
   unlink(backup);
   if(rename(target,backup)!=0)ok=0;
   else if(rename(temporary,target)!=0){rename(backup,target);ok=0;}
  }
 }
 if(!ok)unlink(temporary);
 return ok;
}
