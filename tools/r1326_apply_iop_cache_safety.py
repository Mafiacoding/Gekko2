#!/usr/bin/env python3
"""Harden warm IOP precise blocks against DMA/self-modifying later words."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def rep(path,old,new):
    p=ROOT/path; s=p.read_text(); n=s.count(old)
    if n!=1: raise SystemExit(f"{path}: anchor count {n}, wanted 1")
    p.write_text(s.replace(old,new,1))

rep("source/core/recompiler/iop_jit.c",
"""typedef unsigned (*iop_precise_block)(iop_state_t *,unsigned);
typedef struct {uint32_t pc,first; iop_precise_block fn;} precise_slot;
""",
"""typedef unsigned (*iop_precise_block)(iop_state_t *,unsigned);
typedef struct {
 uint32_t pc,first,words[BLOCK_WORDS];
 iop_precise_block fn;
 uint8_t count;
} precise_slot;
""")

rep("source/core/recompiler/iop_jit.c",
""" precise_slot *slot=&block_cache[((pc>>2)^(pc>>5)^(pc>>12))&(BLOCK_SLOTS-1u)];
 if(!slot->fn||slot->pc!=pc||slot->first!=first) {
  uint32_t words[BLOCK_WORDS];unsigned count=0;int delay=0;
""",
""" precise_slot *slot=&block_cache[((pc>>2)^(pc>>5)^(pc>>12))&(BLOCK_SLOTS-1u)];
 int warm=slot->fn&&slot->pc==pc&&slot->first==first&&slot->count>0u&&slot->count<=BLOCK_WORDS;
 for(unsigned n=0;warm&&n<slot->count;n++) {
  uint32_t live;
  if(!block_peek(st,pc+n*4u,&live)||live!=slot->words[n])warm=0;
 }
 if(!warm) {
  uint32_t words[BLOCK_WORDS];unsigned count=0;int delay=0;
""")

rep("source/core/recompiler/iop_jit.c",
"""  if(slot->fn)free((void*)slot->fn);else block_cache_size++;
  *slot=(precise_slot){pc,first,fn};
 }
""",
"""  if(slot->fn)free((void*)slot->fn);else block_cache_size++;
  memset(slot,0,sizeof(*slot));
  slot->pc=pc;slot->first=first;slot->fn=fn;slot->count=(uint8_t)count;
  memcpy(slot->words,words,count*sizeof(uint32_t));
 }
""")

print("R1326 IOP full-prefix SMC validation applied")
