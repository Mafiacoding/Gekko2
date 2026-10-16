#include "core/hw/game_trace.h"
#ifdef GEKKO2_GAME_TRACE
#include "core/ee/ee_core.h"
#include <string.h>
/* Single emulation thread only. No allocation, file access, guest reads or
 * MMIO calls in producers. Never log a retry for each stalled EE tick. */
#define TRACE_CAP 64u
typedef struct {uint64_t seq,retired;uint32_t pc,kind,arg[4];} trace_entry;
static trace_entry ring[TRACE_CAP];
static uint64_t sequence;
static trace_entry unknown[8];
static unsigned unknown_count,unknown_dumped;
void game_trace_reset(void){memset(ring,0,sizeof ring);sequence=0;unknown_count=unknown_dumped=0;}
void game_trace_record(unsigned kind,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    const ee_state_t *ee=ee_core_get_state();
    uint64_t next=++sequence;trace_entry *e=&ring[(next-1u)%TRACE_CAP];
    *e=(trace_entry){next,ee->instructions_executed,ee->pc,kind,{a,b,c,d}};
    if(kind==GT_VU_UNKNOWN&&unknown_count<8u)unknown[unknown_count++]=*e;
}
void game_trace_dump(FILE *out,uint64_t *cursor)
{
    static const char *const names[]={"?","GIF_TAG","XGKICK","SIGNAL","SIGNAL_WAIT","SIGNAL_RESUME","CSR_ACK","IMR","VIF_WAIT","VIF_RESUME","GIF_WAIT","GIF_RESUME","VU_UNKNOWN","INTC_GS_ACK","INTC_GS_MASK"};
    if(!out||!cursor)return;
    uint64_t end=sequence,start=*cursor+1u,lost=0;
    if(*cursor>end)start=1; /* new cold boot */
    uint64_t first=end>=TRACE_CAP?end-TRACE_CAP+1u:1u;
    if(start<first){lost=first-start;start=first;}
    if(start<=end)fprintf(out,"GAME_TRACE schema=1 begin=%llu end=%llu overwritten=%llu capacity=%u clock=EE-retired\n",(unsigned long long)start,(unsigned long long)end,(unsigned long long)lost,TRACE_CAP);
    for(uint64_t n=start;n<=end;n++){
        const trace_entry *e=&ring[(n-1u)%TRACE_CAP];
        fprintf(out,"GAME_EVENT seq=%llu EE=%llu pc=%08lx kind=%s a=%08lx b=%08lx c=%08lx d=%08lx\n",
            (unsigned long long)e->seq,(unsigned long long)e->retired,(unsigned long)e->pc,
            e->kind>=16u&&e->kind<=18u?(e->kind==16u?"TAG_PATH1":e->kind==17u?"TAG_PATH2":"TAG_PATH3"):
            e->kind<sizeof names/sizeof names[0]?names[e->kind]:"?",
            (unsigned long)e->arg[0],(unsigned long)e->arg[1],(unsigned long)e->arg[2],(unsigned long)e->arg[3]);
    }
    for(;unknown_dumped<unknown_count;unknown_dumped++){
        const trace_entry *e=&unknown[unknown_dumped];
        fprintf(out,"GAME_UNKNOWN seq=%llu EE=%llu pc=%08lx vu_pc=%08lx upper=%08lx lower=%08lx decoded_halves=%lu\n",
            (unsigned long long)e->seq,(unsigned long long)e->retired,(unsigned long)e->pc,
            (unsigned long)e->arg[0],(unsigned long)e->arg[1],(unsigned long)e->arg[2],(unsigned long)e->arg[3]);
    }
    *cursor=end;
}
#endif
