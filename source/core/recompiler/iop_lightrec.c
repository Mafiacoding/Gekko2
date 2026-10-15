#include "core/recompiler/iop_lightrec.h"
#include "core/recompiler/optimization.h"
#include "core/recompiler/ppc_code_cache.h"
#include <string.h>
#ifdef GEKKO2_IOP_LIGHTREC_PILOT
#include "lightrec.h"
#ifdef GEKKO
#include <ogc/cache.h>
#else
#include <sys/mman.h>
#endif
#define CELLS 512u
#define POOL_BYTES (512u*1024u)
#define EXIT_PC 0x001f0000u
static struct lightrec_state *backend;
static struct lightrec_mem_map maps[PSX_MAP_CODE_BUFFER+1];
static uint32_t words[CELLS],valid[CELLS];
static uint8_t programs[CELLS*16u] __attribute__((aligned(32)));
static void *pool;
static uint64_t calls,misses,failures,eligible,fallbacks;
static int disabled;
static lightrec_gte_handler_t no_gte(uint32_t code,_Bool flagless){(void)code;(void)flagless;return NULL;}
static void no_ram(struct lightrec_state *state,_Bool enabled){(void)state;(void)enabled;}
static void put32(uint8_t *p,uint32_t w){p[0]=w;p[1]=w>>8;p[2]=w>>16;p[3]=w>>24;}
static int initialize(void)
{
 if(disabled)return 0;if(backend)return 1;
#ifdef GEKKO
 /* Do not fall back to an unbounded executable heap if the arena is full. */
 if(!gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA))return 0;
 pool=ppc_code_cache_alloc(POOL_BYTES);
#else
 pool=mmap(NULL,POOL_BYTES,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(pool==MAP_FAILED)pool=NULL;
#endif
 if(!pool){failures++;disabled=1;return 0;}
 memset(maps,0,sizeof maps);
 maps[PSX_MAP_KERNEL_USER_RAM]=(struct lightrec_mem_map){.pc=0,.length=sizeof programs,.address=programs};
 maps[PSX_MAP_BIOS].pc=0x1fc00000u;
 maps[PSX_MAP_SCRATCH_PAD].pc=0x1f800000u;
 maps[PSX_MAP_HW_REGISTERS].pc=0x1f801000u;
 maps[PSX_MAP_CODE_BUFFER]=(struct lightrec_mem_map){.length=POOL_BYTES,.address=pool};
 struct lightrec_ops ops={.cop2_hdl=no_gte,.enable_ram=no_ram};
 backend=lightrec_init("Gekko2-IOP-pilot",maps,PSX_MAP_CODE_BUFFER+1,&ops);
 if(!backend){
#ifdef GEKKO
  ppc_code_cache_release(pool);
#else
  munmap(pool,POOL_BYTES);
#endif
  pool=NULL;failures++;disabled=1;return 0;
 }
 lightrec_set_cycles_per_opcode(backend,1);lightrec_set_unsafe_opt_flags(backend,0);
 return 1;
}
int iop_lightrec_prepare(void)
{
 return gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT) && initialize();
}
int iop_lightrec_try_execute(iop_state_t *st,uint32_t word)
{
 unsigned fn=word&63u;
 if(!st||!gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT)||word>>26||fn<0x18u||fn>0x1bu)return 0;
 eligible++;
 if(!initialize()){fallbacks++;return 0;}
 unsigned slot=(word^(word>>11)^(word>>21))&(CELLS-1u);
 uint32_t pc=slot*16u;
 if(!valid[slot]||words[slot]!=word){
  lightrec_invalidate(backend,pc,16);put32(programs+pc,word);
  put32(programs+pc+4,0x08000000u|(EXIT_PC>>2));put32(programs+pc+8,0);
  put32(programs+pc+12,0);words[slot]=word;valid[slot]=1;misses++;
 }
 struct lightrec_registers *r=lightrec_get_registers(backend);
 memcpy(r->gpr,st->gpr,sizeof st->gpr);r->gpr[0]=0;r->gpr[32]=st->lo;r->gpr[33]=st->hi;
 lightrec_reset_cycle_count(backend,0);
 uint32_t next=lightrec_execute(backend,pc,1);
 if(lightrec_exit_flags(backend)||next!=EXIT_PC){failures++;fallbacks++;disabled=1;return 0;}
 /* Only HI/LO may change. No guest memory, PC, COP0 or clock is imported
  * from the synthetic program. One original IOP instruction still retires. */
 st->lo=r->gpr[32];st->hi=r->gpr[33];calls++;return 1;
}
void iop_lightrec_reset(void)
{
 if(backend){lightrec_destroy(backend);backend=NULL;}
 if(pool){
#ifdef GEKKO
  ppc_code_cache_release(pool);
#else
  munmap(pool,POOL_BYTES);
#endif
  pool=NULL;
 }
 memset(valid,0,sizeof valid);calls=misses=failures=eligible=fallbacks=0;disabled=0;
}
uint64_t iop_lightrec_stat(unsigned n)
{
 switch(n){case 0:return backend!=NULL&&!disabled;case 1:return calls;case 2:return misses;case 3:return failures;case 4:return pool?POOL_BYTES:0;case 5:return eligible;case 6:return fallbacks;default:return 0;}
}
#else
int iop_lightrec_prepare(void){return 0;}
int iop_lightrec_try_execute(iop_state_t *st,uint32_t word){(void)st;(void)word;return 0;}
void iop_lightrec_reset(void){}
uint64_t iop_lightrec_stat(unsigned n){(void)n;return 0;}
#endif
