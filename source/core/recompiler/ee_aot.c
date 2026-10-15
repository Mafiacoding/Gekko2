/* GPL-3.0-or-later. Generated C is compiled by the target compiler, never
 * installed as executable input from a disc. Live source and retirement
 * checks retain the existing precise EE execution contract. */
#include "core/recompiler/ee_aot.h"
#include <stddef.h>
typedef unsigned (*ee_aot_fn)(ee_state_t *, unsigned);
typedef struct { uint32_t pc, first_word; ee_aot_fn fn; } ee_aot_entry;
static uint32_t ee_aot_prepare_memory(ee_state_t *st,uint32_t pc,uint32_t word)
{
    uint32_t live;
    if(!ee_core_block_peek(st,pc,&live)||live!=word)return 0;
    if(!((word>>26)==0x1eu&&!((word>>16)&31u)) &&
       !ee_core_block_memory_resolve_precise(st,word))return 0;
    return st->branch_pending ? ee_core_block_prepare_delay(st,pc,word) :
        ee_core_block_prepare_fetched(st,pc);
}
static uint32_t ee_aot_prepare(ee_state_t *st,uint32_t pc,uint32_t word,
                               unsigned delay_safe,unsigned memory)
{
    if(st->branch_pending&&!delay_safe)return 0;
    if(memory)return ee_aot_prepare_memory(st,pc,word);
    return st->branch_pending ? ee_core_block_prepare_delay(st,pc,word) :
        ee_core_block_prepare(st,pc,word);
}
#include "ee_aot_generated.inc"
static uint64_t retired_count;
/* The table is immutable for the lifetime of this executable. Cache both
 * hits and misses by full PC; live instruction checks still run on every
 * execution. A guest code write cannot turn a missing static entry into
 * an entry, and cannot bypass the first-word or per-instruction guards. */
typedef struct { uint32_t pc; const ee_aot_entry *entry; unsigned valid; } ee_aot_lookup;
static ee_aot_lookup lookup_cache[128];
static const ee_aot_entry *ee_aot_find(uint32_t pc)
{
    ee_aot_lookup *slot=&lookup_cache[((pc>>2)^(pc>>10)^(pc>>16))&127u];
    if(slot->valid && slot->pc==pc) return slot->entry;
    unsigned lo=0,hi=ee_aot_entries_count;
    const ee_aot_entry *entry=NULL;
    while(lo<hi) {
        unsigned mid=lo+(hi-lo)/2;
        if(pc<ee_aot_entries[mid].pc) hi=mid;
        else if(pc>ee_aot_entries[mid].pc) lo=mid+1;
        else { entry=&ee_aot_entries[mid]; break; }
    }
    slot->pc=pc;slot->entry=entry;slot->valid=1;
    return entry;
}
unsigned ee_aot_block_count(void) { return ee_aot_entries_count; }
uint64_t ee_aot_retired_count(void) { return retired_count; }
unsigned ee_aot_try_execute_fetched(ee_state_t *st, unsigned budget, uint32_t first_word)
{
    if(!st || st->halted || st->idle)
        return 0;
    budget=ee_core_interleave_limit(budget);
    if(!budget) return 0;
    const ee_aot_entry *entry=ee_aot_find(st->pc);
    if(!entry || first_word!=entry->first_word) return 0;
    unsigned n=entry->fn(st,budget);
    retired_count+=n;
    return n;
}
