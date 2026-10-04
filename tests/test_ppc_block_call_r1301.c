#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "core/recompiler/ppc_dynarec.c"
int main(void)
{
 uint32_t code[16]={0};ppc_codegen_ctx_t c={0};c.code=code;c.capacity_words=16;
 uint32_t site=(uint32_t)(uintptr_t)code;
 int64_t offsets[]={-33554432LL,-4,0,4,33554428LL,-33554436LL,33554432LL};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
  int64_t target=(int64_t)site+offsets[i];if(target<0||target>UINT32_MAX)continue;
  c.used_words=0;ee_block_call(&c,(uint32_t)target);
  int near=offsets[i]>=-33554432LL&&offsets[i]<=33554428LL;
  if(near){assert(c.used_words==1);assert(code[0]==(enc_b((int32_t)offsets[i])|1u));}
  else{assert(c.used_words>1);assert(code[c.used_words-1]==enc_bctrl());}
 }
 c.used_words=0;ee_block_call(&c,site+1u);
 assert(c.used_words>1&&code[c.used_words-1]==enc_bctrl());
 puts("PASS direct PPC callback reach endpoints, signed displacements and far/unaligned CTR fallback");return 0;
}
