#include "core/recompiler/ps2recomp_bridge.h"
#include <string.h>
static ee_state_t *bridge_state(ee_recomp_bridge_t *b)
{
 if(ee_recomp_bridge_validate(b)!=EE_RECOMP_BRIDGE_OK)return 0;
 return (ee_state_t *)b->opaque_state;
}
static ee_recomp_bridge_result_t access_begin(ee_recomp_bridge_t *b,ee_state_t **out)
{
 ee_state_t *s;
 if(!out)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT;
 s=bridge_state(b);if(!s)return EE_RECOMP_BRIDGE_BAD_HANDLE;
 if(s->mem_tlb_miss||s->exc_raised_this_step)return EE_RECOMP_BRIDGE_STATE_NOT_CLEAN;
 *out=s;return EE_RECOMP_BRIDGE_OK;
}
static ee_recomp_bridge_result_t access_end(ee_state_t *s)
{return (s->mem_tlb_miss||s->exc_raised_this_step)?EE_RECOMP_BRIDGE_GUEST_FAULT:EE_RECOMP_BRIDGE_OK;}
ee_recomp_bridge_result_t ee_recomp_bridge_bind(ee_recomp_bridge_t *b,ee_state_t *s)
{
 if(!b||!s)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT;
 b->magic=EE_RECOMP_BRIDGE_MAGIC;b->abi_version=EE_RECOMP_BRIDGE_ABI_VERSION;
 b->struct_size=(uint32_t)sizeof(*b);b->opaque_state=s;return EE_RECOMP_BRIDGE_OK;
}
ee_recomp_bridge_result_t ee_recomp_bridge_validate(const ee_recomp_bridge_t *b)
{
 if(!b)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT;
 if(b->magic!=EE_RECOMP_BRIDGE_MAGIC||b->abi_version!=EE_RECOMP_BRIDGE_ABI_VERSION||
    b->struct_size!=(uint32_t)sizeof(*b)||!b->opaque_state)return EE_RECOMP_BRIDGE_BAD_HANDLE;
 return EE_RECOMP_BRIDGE_OK;
}
#define BRIDGE_READ(name,type,fn) \
ee_recomp_bridge_result_t name(ee_recomp_bridge_t *b,uint32_t a,type *out) \
{ee_state_t *s;type v;ee_recomp_bridge_result_t r;if(!out)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT; \
 r=access_begin(b,&s);if(r!=EE_RECOMP_BRIDGE_OK)return r;v=fn(s,a);r=access_end(s);if(r==EE_RECOMP_BRIDGE_OK)*out=v;return r;}
BRIDGE_READ(ee_recomp_bridge_read8,uint8_t,ee_mem_read8)
BRIDGE_READ(ee_recomp_bridge_read16,uint16_t,ee_mem_read16)
BRIDGE_READ(ee_recomp_bridge_read32,uint32_t,ee_mem_read32)
BRIDGE_READ(ee_recomp_bridge_read64,uint64_t,ee_mem_read64)
ee_recomp_bridge_result_t ee_recomp_bridge_read128(ee_recomp_bridge_t *b,uint32_t a,ee_reg128_t *out)
{ee_state_t *s;ee_reg128_t v;ee_recomp_bridge_result_t r;if(!out)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT;
 r=access_begin(b,&s);if(r!=EE_RECOMP_BRIDGE_OK)return r;ee_mem_read128(s,a,&v);r=access_end(s);if(r==EE_RECOMP_BRIDGE_OK)*out=v;return r;}
#define BRIDGE_WRITE(name,type,fn) \
ee_recomp_bridge_result_t name(ee_recomp_bridge_t *b,uint32_t a,type v) \
{ee_state_t *s;ee_recomp_bridge_result_t r=access_begin(b,&s);if(r!=EE_RECOMP_BRIDGE_OK)return r;fn(s,a,v);return access_end(s);}
BRIDGE_WRITE(ee_recomp_bridge_write8,uint8_t,ee_mem_write8)
BRIDGE_WRITE(ee_recomp_bridge_write16,uint16_t,ee_mem_write16)
BRIDGE_WRITE(ee_recomp_bridge_write32,uint32_t,ee_mem_write32)
BRIDGE_WRITE(ee_recomp_bridge_write64,uint64_t,ee_mem_write64)
ee_recomp_bridge_result_t ee_recomp_bridge_write128(ee_recomp_bridge_t *b,uint32_t a,const ee_reg128_t *v)
{ee_state_t *s;ee_recomp_bridge_result_t r;if(!v)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT;
 r=access_begin(b,&s);if(r!=EE_RECOMP_BRIDGE_OK)return r;ee_mem_write128(s,a,v);return access_end(s);}


ee_recomp_bridge_result_t ee_recomp_bridge_snapshot_integer_leaf(
 ee_recomp_bridge_t *b,ee_recomp_ee_state_snapshot_t *out)
{
 ee_state_t *s;ee_recomp_ee_state_snapshot_t snapshot;
 ee_recomp_bridge_result_t r;
 if(!out)return EE_RECOMP_BRIDGE_INVALID_ARGUMENT;
 r=access_begin(b,&s);if(r!=EE_RECOMP_BRIDGE_OK)return r;
 /* This profile is a read-only architectural snapshot for straight-line
  * integer code. Never normalize or repair an unsupported live state. */
 if(s->halted||s->idle||s->branch_pending||s->exc_in_delay_slot||
    s->vu0_running||(s->cop0[12]&0x6u)||(s->pc&3u)||
    s->next_pc!=s->pc+4u||s->gpr[0].ud0||s->gpr[0].ud1)
  return EE_RECOMP_BRIDGE_UNSUPPORTED_STATE;
 snapshot.abi_version=EE_RECOMP_EE_STATE_ABI_VERSION;
 snapshot.struct_size=(uint32_t)sizeof(snapshot);
 snapshot.profile=EE_RECOMP_EE_STATE_PROFILE_INTEGER_LEAF;
 snapshot.flags=s->branch_pending?EE_RECOMP_EE_STATE_FLAG_BRANCH_PENDING:0u;
 snapshot.pc=s->pc;snapshot.next_pc=s->next_pc;snapshot.sa_reg=s->sa_reg;
 snapshot.reserved=0u;
 memcpy(snapshot.gpr,s->gpr,sizeof(snapshot.gpr));
 snapshot.gpr[0].ud0=0u;snapshot.gpr[0].ud1=0u;
 snapshot.hi=s->hi;snapshot.lo=s->lo;
 *out=snapshot;
 return EE_RECOMP_BRIDGE_OK;
}

/* Batch C: delegate admission and scheduler accounting to the existing
 * Gekko2 native-window gate. This is not an executor: after a nonzero
 * grant the caller must run the matching native body immediately and
 * must not retire those instructions a second time. A zero grant means
 * no guest state was advanced and the precise path must run instead. */
unsigned ee_recomp_bridge_begin_native_window(ee_recomp_bridge_t *b,
 uint32_t pc,const uint32_t *words,unsigned count,unsigned budget,uint32_t needed)
{
 ee_state_t *s;
 if(!words||!count||!budget)return 0u;
 if(access_begin(b,&s)!=EE_RECOMP_BRIDGE_OK)return 0u;
 if(s->pc!=pc||s->next_pc!=pc+4u||s->halted||s->idle||s->branch_pending)return 0u;
 return ee_core_native_window_begin_with_addresses(s,pc,words,count,budget,needed);
}
