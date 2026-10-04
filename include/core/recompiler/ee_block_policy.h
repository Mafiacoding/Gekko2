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
 case 0x23u:case 0x2bu:case 0x31u:case 0x39u:return 4u;
 case 0x37u:case 0x3fu:return 8u;
 case 0x1eu:return ((w>>16)&31u)?16u:0u; /* LQ $zero has no access. */
 case 0x1fu:return 16u;
 default:return 0u;
 }
}
static inline int ee_jit_block_memory_store(uint32_t w)
{
 unsigned op=w>>26;
 return op==0x28u||op==0x29u||op==0x2bu||op==0x39u||op==0x3fu||op==0x1fu;
}
/* Control instructions may only terminate a precise block. The delay slot
 * remains in the scalar engine; no next-block linking is implied. */
static inline int ee_jit_block_terminal(uint32_t w)
{
 unsigned op=w>>26,f=w&63u,rt=(w>>16)&31u;
 return op==2u||op==3u||(op>=4u&&op<=7u)||(op>=0x14u&&op<=0x17u)||
        (op==0u&&(f==8u||f==9u))||(op==1u&&rt<=3u);
}
/* Shared opcode policy; memory candidates require an additional live guard. */
static inline int ee_jit_block_candidate(uint32_t w)
{
 unsigned op=w>>26,f=w&63u;
 /* R1297: COP1 bit transfers and sign-bit operations are already native
  * scalar JIT operations; they need no floating-point exception path. */
 if(op==0x11u) {
  unsigned rs=(w>>21)&31u;
  return rs==0u||rs==2u||rs==4u||rs==6u||
      (rs==0x10u&&(f==5u||f==6u||f==7u));
 }
 return (op==0x1eu&&((w>>16)&31u)==0u)||(op>=9u&&op<=15u)||op==0x19u||(op==0u&&
 (f==0u||f==2u||f==3u||f==4u||f==6u||f==7u||f==0xau||f==0xbu||
 (f>=0x10u&&f<=0x14u)||(f>=0x16u&&f<=0x1bu)||f==0x21u||f==0x23u||
 (f>=0x24u&&f<=0x27u)||f==0x2au||f==0x2bu||f==0x2du||f==0x2fu||
 f==0x38u||f==0x3au||f==0x3bu||f==0x3cu||f==0x3eu||f==0x3fu)) || ee_jit_block_memory_width(w);
}

#endif
