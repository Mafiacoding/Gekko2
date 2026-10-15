/* Exercise the actual PPC translator; do not execute PPC on the host. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "core/recompiler/ppc_dynarec.c"
static int has(const ppc_codegen_ctx_t *c,uint32_t w){for(size_t n=0;n<c->used_words;n++)if(c->code[n]==w)return 1;return 0;}
int main(void){
 uint32_t code[256];ppc_codegen_ctx_t c={code,256,0};
 assert(ppc_dynarec_translate_vu_lower(&c,0x80020875)==0);
 /* 0x7cc63b78 = or r6,r6,r7; 0x7cc63838 = and r6,r6,r7. */
 assert(has(&c,0x7cc63b78u));assert(!has(&c,0x7cc63838u));
 c.used_words=0;assert(ppc_dynarec_translate_vu_lower(&c,0x80020874)==0);
 assert(has(&c,0x7cc63838u));assert(!has(&c,0x7cc63b78u));
 c.used_words=0;assert(ppc_dynarec_translate_vu_lower(&c,0x80020873)==-1);
 c.used_words=0;assert(ppc_dynarec_translate_vu_lower(&c,0x80020835)==0);
 assert(c.used_words==0); /* VI0 destination is a no-op. */
 puts("PASS actual PPC VU IOR/IAND emitter, reserved rejection, VI0");return 0;
}
