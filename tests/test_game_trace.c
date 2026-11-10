#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/hw/game_trace.h"
#include "core/ee/ee_core.h"
static ee_state_t state;
ee_state_t *ee_core_get_state(void){return &state;}
int main(void){
 game_trace_reset();uint64_t cursor=0;FILE*f=tmpfile();assert(f);game_trace_dump(f,&cursor);assert(ftell(f)==0);
 for(unsigned i=0;i<100;i++){state.pc=0x1000+i*4;state.instructions_executed=i;game_trace_record(GT_TAG,i,2,3,4);}
 ee_state_t before=state;game_trace_dump(f,&cursor);assert(!memcmp(&before,&state,sizeof state));assert(cursor==100);
 fflush(f);long size=ftell(f);rewind(f);char*text=calloc(1,size+1);assert(text);assert(fread(text,1,size,f)==(size_t)size);
 assert(strstr(text,"begin=37 end=100 overwritten=36"));unsigned count=0;for(char*p=text;(p=strstr(p,"GAME_EVENT "));p+=11)count++;assert(count==64);assert(!strstr(text,"seq=36 "));free(text);
 fseek(f,0,SEEK_END);game_trace_dump(f,&cursor);assert(ftell(f)==size);
 game_trace_reset();game_trace_record(GT_SIGNAL_WAIT,0,1,2,3);game_trace_dump(f,&cursor);assert(cursor==1);
 game_trace_reset();for(unsigned i=0;i<8;i++)game_trace_record(GT_VU_UNKNOWN,i,2,3,4);
 for(unsigned i=0;i<100;i++)game_trace_record(GT_TAG,0,0,0,0);
 FILE *pinned=tmpfile();assert(pinned);uint64_t next=0;game_trace_dump(pinned,&next);fflush(pinned);long length=ftell(pinned);rewind(pinned);char *saved=calloc(1,length+1);assert(saved);assert(fread(saved,1,length,pinned)==(size_t)length);
 unsigned unknowns=0;for(char*q=saved;(q=strstr(q,"GAME_UNKNOWN "));q+=13)unknowns++;assert(unknowns==8);free(saved);fclose(pinned);
 fclose(f);puts("PASS trace empty/overflow/order/no-repeat/reset/CPU-observational checks");
}
