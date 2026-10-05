#ifndef PCSX2WII_EE_BLOCK_POLICY_H
#define PCSX2WII_EE_BLOCK_POLICY_H
#include <stdint.h>
/* R1306: RAM memory operations join precise blocks only after the
 * runtime proves the live effective address. A failed proof exits before
 * preparation, so unsupported MMIO/mapping/fault semantics remain in the scalar engine. */
static inline unsigned ee_jit_block_memory_width(uint32_t w)
{
 switch(w>>26) {
 case 0x20u:case 0x24u:case 0x28u:return 1u;
 case 0x21u:case 0x25u:case 0x29u:return 2u;
 case 0x22u:case 0x26u:case 0x2au:case 0x2eu:case 0x23u:case 0x27u:case 0x2bu:case 0x31u:case 0x39u:return 4u;
 case 0x1au:case 0x1bu:case 0x2cu:case 0x2du:case 0x37u:case 0x3fu:return 8u;
 case 0x1eu:return ((w>>16)&31u)?16u:0u; /* LQ $zero has no access. */
 case 0x1fu:case 0x3eu:return 16u;
 case 0x36u:return ((w>>16)&31u)?16u:0u;
 default:return 0u;
 }
}
static inline int ee_jit_block_memory_merge(uint32_t w)
{
 unsigned op=w>>26;
 return op==0x22u||op==0x26u||op==0x2au||op==0x2eu||
        op==0x1au||op==0x1bu||op==0x2cu||op==0x2du;
}
static inline int ee_jit_block_memory_store(uint32_t w)
{
 unsigned op=w>>26;
 return op==0x2au||op==0x2eu||op==0x2cu||op==0x2du||op==0x28u||op==0x29u||op==0x2bu||op==0x39u||op==0x3fu||op==0x1fu||op==0x3eu;
}
/* A control instruction ends formation after at most one legal delay slot.
 * The delay preparation callback checks pending/annulled state before it
 * advances PC; unsupported or unsafe slots return to the scalar frontend. */
static inline int ee_jit_block_terminal(uint32_t w)
{
 unsigned op=w>>26,f=w&63u,rt=(w>>16)&31u;
 return op==2u||op==3u||(op>=4u&&op<=7u)||(op>=0x14u&&op<=0x17u)||
        (op==0u&&(f==8u||f==9u))||(op==1u&&(rt<=3u||(rt>=0x10u&&rt<=0x13u)))||
        (op==0x11u&&((w>>21)&31u)==8u&&rt<=3u);
}
/* R1319: exact MMI admission for precise resident blocks.  The single-op
 * frontend intentionally has a blanket MMI gate because the PPC backend
 * exact-decodes it.  Block formation must be stricter: admitting an unknown
 * op=0x1c encoding would turn a scalar fallback into a block-translation
 * failure.  Keep the legal primary/subgroup selectors explicit here. */
static inline int ee_jit_block_mmi(uint32_t w)
{
 if((w>>26)!=0x1cu)return 0;
 unsigned f=w&63u,sa=(w>>6)&31u;
 switch(f) {
 case 0x00u:case 0x01u:case 0x04u:
 case 0x10u:case 0x11u:case 0x12u:case 0x13u:
 case 0x18u:case 0x19u:case 0x1au:case 0x1bu:
 case 0x20u:case 0x21u:case 0x34u:case 0x36u:case 0x37u:
 case 0x3cu:case 0x3eu:case 0x3fu:
  return 1;
 case 0x30u: /* PMFHL: LW/UW/SLW/LH/SH modes only. */
  return sa<=4u;
 case 0x31u: /* PMTHL.LW: all other sa encodings are reserved. */
  return sa==0u;
 case 0x08u: /* MMI0 */
  switch(sa) {
  case 0u:case 1u:case 2u:case 3u:case 4u:case 5u:case 6u:case 7u:
  case 8u:case 9u:case 10u:case 16u:case 17u:case 18u:case 19u:
  case 20u:case 21u:case 22u:case 23u:case 24u:case 25u:case 26u:
  case 27u:case 30u:case 31u:return 1;
  default:return 0;
  }
 case 0x28u: /* MMI1 */
  switch(sa) {
  case 1u:case 2u:case 3u:case 4u:case 5u:case 6u:case 7u:case 10u:
  case 16u:case 17u:case 18u:case 20u:case 21u:case 22u:
  case 24u:case 25u:case 26u:case 27u:return 1;
  default:return 0;
  }
 case 0x09u: /* MMI2 */
  switch(sa) {
  case 0u:case 2u:case 3u:case 4u:case 8u:case 9u:case 10u:
  case 12u:case 13u:case 14u:case 16u:case 17u:case 18u:case 19u:
  case 20u:case 21u:case 26u:case 27u:case 28u:case 29u:case 30u:
  case 31u:return 1;
  default:return 0;
  }
 case 0x29u: /* MMI3 */
  switch(sa) {
  case 0u:case 3u:case 8u:case 9u:case 10u:case 12u:case 13u:case 14u:
  case 18u:case 19u:case 26u:case 27u:case 30u:return 1;
  default:return 0;
  }
 default:return 0;
 }
}
/* Shared opcode policy; memory candidates require an additional live guard. */
static inline int ee_jit_block_candidate(uint32_t w)
{
 unsigned op=w>>26,f=w&63u;
 if(ee_jit_block_mmi(w))return 1;
 if(op==8u||op==0x18u||(op==0u&&(f==0x20u||f==0x22u||f==0x2cu||f==0x2eu)))return 1;
 if(op==0u&&(f==0x30u||f==0x31u||f==0x32u||f==0x33u||f==0x34u||f==0x36u))return 1;
 if(op==1u){unsigned rt=(w>>16)&31u;if(rt==8u||rt==9u||rt==10u||rt==11u||rt==12u||rt==14u)return 1;}
 /* Supported scalar COP1 emitters can join precise blocks. Their current
  * floating-point semantics are checked against the interpreter; this does
  * not add PS2 exception flags absent from that baseline. */
 if(op==0x11u) {
  unsigned rs=(w>>21)&31u;
  return rs==0u||rs==2u||rs==4u||rs==6u||
      (rs==0x14u&&f==0x20u)||
      (rs==0x10u&&(f<=7u||f==0x16u||(f>=0x18u&&f<=0x1fu&&f!=0x1bu)||f==0x24u||
        f==0x28u||f==0x29u||f==0x32u||f==0x34u||f==0x36u));
 }
 return ((op==0x1eu||op==0x36u)&&((w>>16)&31u)==0u)||(op>=9u&&op<=15u)||op==0x19u||(op==0u&&
 (f==0u||f==2u||f==3u||f==4u||f==6u||f==7u||f==0xau||f==0xbu||
 (f>=0x10u&&f<=0x14u)||(f>=0x16u&&f<=0x1bu)||f==0x21u||f==0x23u||
 (f>=0x24u&&f<=0x27u)||f==0x2au||f==0x2bu||f==0x2du||f==0x2fu||
 f==0x38u||f==0x3au||f==0x3bu||f==0x3cu||f==0x3eu||f==0x3fu)) || ee_jit_block_memory_width(w);
}

#endif