#ifndef PCSX2WII_EE_BLOCK_POLICY_H
#define PCSX2WII_EE_BLOCK_POLICY_H
#include <stdint.h>
/* R1304: direct-RAM memory operations join precise blocks only after the
 * runtime proves the live effective address. A failed proof exits before
 * preparation, so MMIO/TLB/fault semantics remain in the scalar engine. */
static inline unsigned ee_jit_block_memory_width(uint32_t w)
{
 switch(w>>26) {
 case 0x20u:case 0x24u:case 0x28u:return 1u;
 case 0x21u:case 0x25u:case 0x29u:return 2u;
 case 0x23u:case 0x2bu:return 4u;
 default:return 0u;
 }
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
 return (op>=9u&&op<=15u)||op==0x19u||(op==0u&&
 (f==0u||f==2u||f==3u||f==4u||f==6u||f==7u||f==0xau||f==0xbu||
 (f>=0x10u&&f<=0x14u)||(f>=0x16u&&f<=0x1bu)||f==0x21u||f==0x23u||
 (f>=0x24u&&f<=0x27u)||f==0x2au||f==0x2bu||f==0x2du||f==0x2fu||
 f==0x38u||f==0x3au||f==0x3bu||f==0x3cu||f==0x3eu||f==0x3fu)) || ee_jit_block_memory_width(w);
}

#endif
