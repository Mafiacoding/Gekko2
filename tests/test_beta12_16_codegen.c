#include "core/recompiler/ppc_dynarec.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned tests,failures;
#define CHECK(x) do{tests++;if(!(x)){failures++;fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}}while(0)
static uint32_t ins(unsigned rs,unsigned rt,unsigned rd,unsigned sa)
{return (0x1cu<<26)|(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|8u;}
static uint32_t biased(uint32_t x,unsigned bits){return (x^((uint32_t)1u<<(bits-1u))) & (bits==32u?~0u:((1u<<bits)-1u));}
static uint32_t result(uint32_t a,uint32_t b,unsigned bits,int maximum){
 uint32_t m=biased(a,bits)>biased(b,bits)?~0u:0u;
 return (maximum?((a&m)|(b&~m)):m)&(bits==32u?~0u:((1u<<bits)-1u));
}
static uint32_t reference(uint32_t a,uint32_t b,unsigned bits,int maximum){
 int32_t sa=bits==32u?(int32_t)a:bits==16u?(int16_t)a:(int8_t)a;
 int32_t sb=bits==32u?(int32_t)b:bits==16u?(int16_t)b:(int8_t)b;
 uint32_t mask=bits==32u?~0u:(1u<<bits)-1u;
 return (maximum?(uint32_t)(sa>sb?sa:sb):(sa>sb?~0u:0u))&mask;
}
int main(void){
 static const unsigned kinds[]={2u,3u,6u,7u,10u};
 for(unsigned k=0;k<5;k++){
  unsigned sa=kinds[k],bits=sa<=3u?32u:sa<=7u?16u:8u;
  ppc_codegen_ctx_t c;
  CHECK(ppc_dynarec_init(&c,1u)==0);
  CHECK(ppc_dynarec_translate_one(&c,ins(4,5,4,sa))==0);
  CHECK(c.used_words && c.used_words <=128u); /* no codegen buffer overflow */
  ppc_dynarec_free(&c);
  for(uint32_t i=0;i<100000u;i++){
   uint32_t a=i*747796405u+2891336453u,b=(i^0x81b37u)*1664525u;
   CHECK(result(a,b,bits,sa==3u||sa==7u)==reference(a,b,bits,sa==3u||sa==7u));
  }
 }
 printf("MMI signed PCGT/PMAX codegen + mask/reference: %u checks, %u failures\n",tests,failures);
 return failures!=0;
}
