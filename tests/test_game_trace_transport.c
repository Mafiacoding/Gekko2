#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/hw/game_trace.h"
#include "core/hw/gif.h"
#include "core/hw/gs.h"
static void put(unsigned char*p,unsigned v){for(unsigned n=0;n<4;n++)p[n]=v>>(8*n);}
static unsigned occurrences(const char*s,const char*key){unsigned n=0;for(const char*p=s;(p=strstr(p,key));p+=strlen(key))n++;return n;}
int main(void){
 gif_init();gs_init();game_trace_reset();unsigned char packet[48]={0};
 put(packet,0x8002);put(packet+4,1u<<28);put(packet+8,14);
 for(unsigned i=1;i<=2;i++){put(packet+16*i,i);put(packet+16*i+4,0xffffffff);put(packet+16*i+8,GS_REG_SIGNAL);}
 assert(gif_try_process_quadwords(GIF_PATH_1,packet,2)==2);
 for(unsigned i=0;i<1000;i++)assert(!gif_try_process_quadwords(GIF_PATH_1,packet+32,1));
 assert(gs_get_state()->csr&1u);assert(gif_get_state()->register_carry_path[0].remaining==1);assert(gif_signal_stall_count()==1000);
 gs_mmio_write64(0x12001000,1);assert(gif_try_process_quadwords(GIF_PATH_1,packet+32,1)==1);
 assert(gs_get_state()->csr&1u);assert((unsigned)gs_get_state()->siglblid==2);
 FILE*f=tmpfile();assert(f);uint64_t cursor=0;game_trace_dump(f,&cursor);fflush(f);long size=ftell(f);rewind(f);char*s=calloc(1,size+1);assert(s);assert(fread(s,1,size,f)==(size_t)size);
 assert(occurrences(s,"kind=SIGNAL_WAIT ")==1);assert(occurrences(s,"kind=SIGNAL_RESUME ")==1);assert(occurrences(s,"kind=CSR_ACK ")==1);assert(occurrences(s,"kind=SIGNAL ")==2);assert(strstr(s,"kind=TAG_PATH1 "));
 free(s);fclose(f);puts("PASS SIGNAL first/wait/1000 retries/CSR-W1C/resume trace; exact producer acceptance retained");
}
