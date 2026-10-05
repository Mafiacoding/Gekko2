#!/usr/bin/env python3
"""Add serial/generation-safe direct successor links to the precise EE cache."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def rep(path,old,new):
    p=ROOT/path; s=p.read_text(); n=s.count(old)
    if n!=1: raise SystemExit(f"{path}: anchor count {n}, wanted 1")
    p.write_text(s.replace(old,new,1))

rep("source/core/recompiler/ee_jit.c",
"""typedef struct {
 uint32_t pc,words[8],count;
 uint32_t source_page,source_generation,mapping_generation;
 uint32_t serial; /* R1318: non-zero identity of this installed allocation. */
 ee_precise_fn fn;
} ee_precise_slot;
static ee_precise_slot precise_cache[256];
static uint32_t precise_page_generation[EE_PRECISE_RAM_PAGES];
static uint32_t precise_mapping_generation;
static uint32_t precise_serial_source;
static uint64_t precise_runs,precise_retired,precise_evictions;
static int precise_active;
""",
"""typedef struct {
 uint32_t pc,words[8],count;
 uint32_t source_page,source_generation,mapping_generation;
 uint32_t serial; /* R1318: non-zero identity of this installed allocation. */
 /* R1326: lazily learned successor identity.  The serial and live target
  * generation are revalidated before every use, so collision eviction,
  * source writes, TLB/ASID changes and checkpoint/reset cannot jump into a
  * stale executable buffer.  This is a native function link, not a patched
  * code-buffer branch, keeping executable ownership simple and reversible. */
 uint32_t link_pc,link_serial;
 uint16_t link_index;
 ee_precise_fn link_fn;
 ee_precise_fn fn;
} ee_precise_slot;
static ee_precise_slot precise_cache[256];
static uint32_t precise_page_generation[EE_PRECISE_RAM_PAGES];
static uint32_t precise_mapping_generation;
static uint32_t precise_serial_source;
static uint64_t precise_runs,precise_retired,precise_evictions,precise_direct_link_hits;
static ee_precise_slot *precise_chain_source;
static uint32_t precise_chain_source_serial;
static int precise_active;
""")

rep("source/core/recompiler/ee_jit.c",
"""uint64_t ee_jit_get_block_count(void){return precise_runs;}
uint64_t ee_jit_get_block_retired(void){return precise_retired;}
uint64_t ee_jit_get_block_evictions(void){return precise_evictions;}
typedef unsigned (*ee_cached_chain_fn)(ee_state_t *,unsigned,uint32_t,unsigned,ee_precise_fn,unsigned);
static ee_cached_chain_fn precise_chain_fn;
static uint64_t precise_native_successors;
uint64_t ee_jit_get_native_successors(void){return precise_native_successors;}
""",
"""uint64_t ee_jit_get_block_count(void){return precise_runs;}
uint64_t ee_jit_get_block_retired(void){return precise_retired;}
uint64_t ee_jit_get_block_evictions(void){return precise_evictions;}
uint64_t ee_jit_get_direct_link_hits(void){return precise_direct_link_hits;}
typedef unsigned (*ee_cached_chain_fn)(ee_state_t *,unsigned,uint32_t,unsigned,ee_precise_fn,unsigned);
static ee_cached_chain_fn precise_chain_fn;
static uint64_t precise_native_successors;
uint64_t ee_jit_get_native_successors(void){return precise_native_successors;}
""")

rep("source/core/recompiler/ee_jit.c",
"""static uint64_t ee_precise_cached_next(ee_state_t *st,unsigned remaining)
{
 if(!precise_active||!st||remaining<2u||st->halted||st->idle||st->branch_pending||st->next_pc!=st->pc+4u)return 0;
 ee_precise_slot *slot=&precise_cache[ee_precise_cache_index(st->pc)];
 if(!slot->fn||!slot->serial||slot->pc!=st->pc||slot->count<2u||slot->count>remaining)return 0;
 uint32_t serial=slot->serial;ee_precise_fn fn=slot->fn;
 if(!ee_precise_slot_generation_current(slot))return 0;
 uint32_t word;
 if(!ee_core_block_peek(st,st->pc,&word)||word!=slot->words[0])return 0;
 /* Identity is rechecked after every live source lookup. This is redundant
  * today because precise_active forbids eviction, and makes that invariant
  * explicit for future cache/link changes. */
 if(slot->serial!=serial||slot->fn!=fn||slot->pc!=st->pc)return 0;
 precise_native_successors++;
 return ((uint64_t)(uint32_t)(uintptr_t)fn<<32)|slot->count;
}
""",
"""static uint64_t ee_precise_cached_next(ee_state_t *st,unsigned remaining)
{
 if(!precise_active||!st||remaining<2u||st->halted||st->idle||st->branch_pending||st->next_pc!=st->pc+4u)return 0;
 ee_precise_slot *source=precise_chain_source,*slot=0;
 /* Fast edge: use the predecessor's learned direct successor when every
  * identity field still matches.  No allocation or compilation can occur
  * while precise_active pins the chain, so the pointer remains owned. */
 if(source&&source->serial==precise_chain_source_serial&&source->link_fn&&source->link_pc==st->pc) {
  unsigned li=source->link_index;
  if(li<256u) {
   ee_precise_slot *linked=&precise_cache[li];
   if(linked->serial==source->link_serial&&linked->fn==source->link_fn&&linked->pc==st->pc)
    slot=linked;
  }
 }
 if(!slot)slot=&precise_cache[ee_precise_cache_index(st->pc)];
 if(!slot->fn||!slot->serial||slot->pc!=st->pc||slot->count<2u||slot->count>remaining)return 0;
 uint32_t serial=slot->serial;ee_precise_fn fn=slot->fn;
 if(!ee_precise_slot_generation_current(slot))return 0;
 uint32_t word;
 if(!ee_core_block_peek(st,st->pc,&word)||word!=slot->words[0])return 0;
 if(slot->serial!=serial||slot->fn!=fn||slot->pc!=st->pc)return 0;
 if(source&&source->serial==precise_chain_source_serial) {
  unsigned idx=ee_precise_cache_index(slot->pc);
  if(source->link_fn==fn&&source->link_pc==slot->pc&&source->link_serial==serial&&source->link_index==idx)
   precise_direct_link_hits++;
  else {
   source->link_pc=slot->pc;source->link_serial=serial;source->link_index=(uint16_t)idx;source->link_fn=fn;
  }
 }
 precise_chain_source=slot;precise_chain_source_serial=serial;
 precise_native_successors++;
 return ((uint64_t)(uint32_t)(uintptr_t)fn<<32)|slot->count;
}
""")

rep("source/core/recompiler/ee_jit.c",
""" if(native_chain&&slot->count+2u<=budget)ee_precise_make_chain();
 precise_active=1;
 unsigned n=(native_chain&&precise_chain_fn&&slot->count+2u<=budget)?
  precise_chain_fn(st,fetched,first_physical,budget,slot->fn,slot->count):
  slot->fn(st,fetched,first_physical);
 precise_active=0;
""",
""" if(native_chain&&slot->count+2u<=budget)ee_precise_make_chain();
 int use_chain=native_chain&&precise_chain_fn&&slot->count+2u<=budget;
 precise_chain_source=use_chain?slot:0;
 precise_chain_source_serial=use_chain?slot->serial:0;
 precise_active=1;
 unsigned n=use_chain?
  precise_chain_fn(st,fetched,first_physical,budget,slot->fn,slot->count):
  slot->fn(st,fetched,first_physical);
 precise_active=0;
 precise_chain_source=0;precise_chain_source_serial=0;
""")

rep("source/core/recompiler/ee_jit.c",
""" precise_chain_fn=0;precise_native_successors=0;
 for(unsigned n=0;n<256;n++)ee_precise_release_slot(&precise_cache[n]);
 memset(precise_cache,0,sizeof(precise_cache));
 memset(precise_page_generation,0,sizeof(precise_page_generation));
 precise_mapping_generation=0;precise_serial_source=0;
 precise_runs=precise_retired=precise_evictions=0;
""",
""" precise_chain_fn=0;precise_native_successors=0;precise_direct_link_hits=0;
 precise_chain_source=0;precise_chain_source_serial=0;
 for(unsigned n=0;n<256;n++)ee_precise_release_slot(&precise_cache[n]);
 memset(precise_cache,0,sizeof(precise_cache));
 memset(precise_page_generation,0,sizeof(precise_page_generation));
 precise_mapping_generation=0;precise_serial_source=0;
 precise_runs=precise_retired=precise_evictions=0;
""")

rep("include/core/recompiler/ee_jit.h",
"""/* R1318: number of valid precise blocks displaced by direct-map collisions. */
uint64_t ee_jit_get_block_evictions(void);
""",
"""/* R1318: number of valid precise blocks displaced by direct-map collisions. */
uint64_t ee_jit_get_block_evictions(void);
/* R1326: warm serial/generation-safe direct successor-link hits. */
uint64_t ee_jit_get_direct_link_hits(void);
""")

print("R1326 direct precise-block links applied")
