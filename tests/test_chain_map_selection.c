#include "core/recompiler/ppc_dynarec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int has(const ppc_ee_chain_map *m,unsigned off)
{for(unsigned i=0;i<m->count;i++)if(m->offsets[i]==off)return 1;return 0;}
static void valid(const ppc_ee_chain_map *m)
{
 assert(m->count>0&&m->count<=12);
 for(unsigned i=0;i<m->count;i++){
  unsigned off=m->offsets[i];assert(off>=16&&!(off&3)&&(off<544||off==552));
  for(unsigned j=0;j<i;j++)assert(off!=m->offsets[j]);
  if(off!=552)for(unsigned l=0;l<4;l++)assert(has(m,(off&~15u)+l*4));
 }
}
int main(void)
{
 const uint32_t mmi=(28u<<26)|(20u<<21)|(21u<<16)|(22u<<11)|(18u<<6)|9u;
 uint32_t program[]={mmi,mmi};ppc_ee_chain_map map,again;
 assert(!ppc_dynarec_plan_ee_chain_map(program,2,1024,&map));valid(&map);
 assert(map.count==12&&has(&map,320)&&has(&map,336)&&has(&map,352)&&!has(&map,64));
 assert(!ppc_dynarec_plan_ee_chain_map(program,2,1024,&again));assert(!memcmp(&map,&again,sizeof map));
 const uint32_t hilo[]={(1u<<21)|17u,(2u<<21)|19u,(3u<<11)|16u,(4u<<11)|18u};
 assert(!ppc_dynarec_plan_ee_chain_map(hilo,4,1024,&map));valid(&map);assert(has(&map,512)&&has(&map,528));
 const uint32_t sa[]={(1u<<21)|0x29u,(1u<<11)|0x28u};
 assert(!ppc_dynarec_plan_ee_chain_map(sa,2,1024,&map));valid(&map);assert(has(&map,552));
 const uint32_t memory[]={(0x1eu<<26)|(1u<<21)|(20u<<16),
                          (0x1fu<<26)|(1u<<21)|(20u<<16)|32u};
 assert(!ppc_dynarec_plan_ee_chain_map(memory,2,1024,&map));valid(&map);
 assert(has(&map,320)&&has(&map,332));
 ppc_codegen_ctx_t ctx;assert(!ppc_dynarec_init(&ctx,4));
 size_t before=ctx.used_words;
 assert(ppc_dynarec_translate_ee_chain_block(&ctx,0x200000,memory,2,1,0,0,2,&map)<0);
 assert(ctx.used_words==before); /* Missing RAM-proof callback declines atomically. */
 assert(!ppc_dynarec_translate_ee_chain_block(&ctx,0x200000,memory,2,1,3,0,2,&map));
 const uint32_t branch[]={(3u<<26)|0x80008u,0u};
 assert(!ppc_dynarec_plan_ee_chain_map(branch,2,1024,&map));valid(&map);assert(has(&map,31*16));
 before=ctx.used_words;
 assert(ppc_dynarec_translate_ee_chain_block(&ctx,0x200000,branch,2,1,3,0,2,&map)<0);
 assert(ctx.used_words==before);
 assert(!ppc_dynarec_translate_ee_chain_block(&ctx,0x200000,branch,2,1,3,4,2,&map));
 before=ctx.used_words;
 assert(ppc_dynarec_translate_ee_fused_chain_block(&ctx,0x200000,branch,2,1,3,4,2,0,6,7,&map)<0);
 assert(ctx.used_words==before);
 assert(!ppc_dynarec_translate_ee_fused_chain_block(&ctx,0x200000,branch,2,1,3,4,2,5,6,7,&map));
 ppc_dynarec_free(&ctx);
 unsigned rejected=0;
 const uint32_t bad[]={0,12,(4u<<26)|1,(35u<<26)|(1u<<21)|(2u<<16)};
 for(unsigned i=0;i<4;i++){
  uint32_t words[]={bad[i],bad[i]};memset(&map,0xa5,sizeof map);again=map;
  assert(ppc_dynarec_plan_ee_chain_map(words,2,1024,&map)<0&&!memcmp(&map,&again,sizeof map));rejected++;
 }
 for(unsigned gen=0;gen<=32768;gen+=32768){
  assert(ppc_dynarec_plan_ee_chain_map(program,2,gen,&map)<0);rejected++;
 }
 assert(ppc_dynarec_plan_ee_chain_map(program,2,320,&map)<0);rejected++;
 assert(ppc_dynarec_plan_ee_chain_map(program,1,1024,&map)<0);rejected++;
 puts("PASS demand-selected 128-bit GPR/HI/LO/SA maps, deterministic whole groups and 8 atomic declines");
 assert(rejected==8);return 0;
}
