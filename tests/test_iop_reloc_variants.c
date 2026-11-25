#define main previous_elf_test_main
#include "test_iop_elf.c"
#undef main
#include <assert.h>
static void rel(uint8_t *b,unsigned n,uint32_t addr,unsigned type){wle32(b+180+n*8,addr);wle32(b+184+n*8,type);}
int main(void){
 bios_image_t bios={0};assert(!iop_core_init(&bios));iop_state_t*s=iop_core_get_state();
 uint8_t b[512];iop_elf_load_result_t result;const char*err;unsigned checks=0;
 for(unsigned low=0;low<65536;low+=257){
  unsigned size=build_synthetic_module(b),base=0x100004+low;
  base&=~3u;wle32(b+84,0x2408fffcu);wle32(b+88,0x24097fffu);
  rel(b,0,0,1);rel(b,1,4,6);rel(b,2,0xffffffffu,0);rel(b,3,0xffffffffu,0);
  assert(!iop_elf_load(s,b,size,base,&result,&err));
  assert(iop_mem_read32(s,base)==(0x24080000u|((base-4)&0xffffu)));
  assert(iop_mem_read32(s,base+4)==(0x24090000u|((base+0x7fff)&0xffffu)));checks+=3;
 }
 for(unsigned bad=0;bad<6;bad++){
  unsigned size=build_synthetic_module(b),base=0x108004,addend=0x12345;
  wle32(b+84,0x3c080002);wle32(b+92,0x3c09ffff);wle32(b+88,0x3c0a0000);
  rel(b,0,0,250);rel(b,1,addend,bad==3?0:251);rel(b,2,0xffffffffu,0);rel(b,3,0xffffffffu,0);
  if(bad==1)wle32(b+88,0x3c0affff); /* cycle */
  if(bad==2)wle32(b+92,0x3c097fff); /* out of loaded segment */
  if(bad==4)wle32(b+212+40+20,31);
  if(bad==5)wle32(b+212+40+20,8);
  int rc=iop_elf_load(s,b,size,base,&result,&err);
  if(bad){assert(rc<0);assert(iop_mem_read32(s,base)==0x3c080002);}
  else{assert(!rc);unsigned high=((base+addend+0x8000u)>>16)&65535;
   assert(iop_mem_read32(s,base)==(0x3c080000|high));
   assert(iop_mem_read32(s,base+4)==(0x3c0a0000|high));assert(iop_mem_read32(s,base+8)==(0x3c090000|high));}
  checks++;
 }
 printf("PASS %u IOP NONE/16/LO16/SCE-MHI16 checks, signed links and malformed-chain rejection\n",checks);return 0;
}
