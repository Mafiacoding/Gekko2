#include "core/hw/iop_module_metadata.h"
#include <string.h>
static uint32_t u32(const uint8_t*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t u16(const uint8_t*p){return p[0]|((uint16_t)p[1]<<8);}
int iop_module_metadata(const uint8_t*b,uint32_t size,char name[64],uint16_t *version){
 if(!b||!name||!version||size<52||memcmp(b,"\177ELF",4)||b[4]!=1||b[5]!=1)return 0;
 uint32_t shoff=u32(b+32);unsigned stride=u16(b+46),count=u16(b+48);
 if(stride<40 || (uint64_t)shoff+(uint64_t)stride*count>size)return 0;
 for(unsigned i=0;i<count;i++){
  const uint8_t*s=b+shoff+i*stride;if(u32(s+4)!=0x70000080u)continue;
  uint32_t off=u32(s+16),n=u32(s+20);
  if(n<27 || (uint64_t)off+n>size)return 0;
  unsigned k=0;while(k<63 && k+26<n && b[off+26+k]){name[k]=(char)b[off+26+k];k++;}
  if(k+26>=n || k==63)return 0;
  name[k]=0;*version=u16(b+off+24);return 1;
 }
 return 0;
}

static const uint8_t *elf_word_at(const uint8_t *b,uint32_t size,uint32_t address)
{
 uint32_t off=u32(b+28);unsigned stride=u16(b+42),count=u16(b+44);
 if(stride<32 || (uint64_t)off+(uint64_t)stride*count>size)return NULL;
 for(unsigned n=0;n<count;n++) {
  const uint8_t *p=b+off+n*stride;
  uint32_t start=u32(p+8),bytes=u32(p+16),file=u32(p+4);
  if(u32(p)!=1 || address<start || bytes<4 || address-start>bytes-4)continue;
  if((uint64_t)file+bytes>size)return NULL;
  return b+file+address-start;
 }
 return NULL;
}
int iop_loadfile_protocol(const uint8_t *b,uint32_t size,uint32_t *version)
{
 if(!b||!version||size<52||memcmp(b,"\177ELF",4)||b[4]!=1||b[5]!=1||u16(b+18)!=8)return 0;
 uint32_t off=u32(b+32),found=0;unsigned stride=u16(b+46),count=u16(b+48);
 if(stride<40 || (uint64_t)off+(uint64_t)stride*count>size)return 0;
 for(unsigned n=0;n<count;n++) {
  const uint8_t *p=b+off+n*stride;
  uint32_t file=u32(p+16),bytes=u32(p+20);
  if(u32(p+4)!=1 || !(u32(p+8)&4))continue;
  if((uint64_t)file+bytes>size)return 0;
  for(uint32_t k=0;k+32u<=bytes;k+=4) {
   const uint8_t *c=b+file+k;
   /* addiu v0,zero,255; bne a0,v0,epilogue; zero v0; then
    * load the response constant and store it into the RPC output buffer.
    * This getter is observational HLE, not module execution. */
   if(u32(c)!=0x240200ffu || u32(c+4)!=0x14820006u || u32(c+8)!=0x00001021u ||
      (u32(c+12)&0xffff0000u)!=0x3c030000u || (u32(c+16)&0xffff0000u)!=0x8c630000u ||
      (u32(c+20)&0xffff0000u)!=0x3c020000u || (u32(c+24)&0xffff0000u)!=0x24420000u ||
      u32(c+28)!=0xac430000u)continue;
   uint32_t address=((u32(c+12)&65535u)<<16)+(int16_t)u32(c+16);
   const uint8_t *value=elf_word_at(b,size,address);if(!value)return 0;
   for(unsigned j=0;j<4;j++)if(value[j]<'0'||value[j]>'9')return 0;
   uint32_t v=u32(value);if(found&&found!=v)return 0;found=v;
  }
 }
 if(!found)return 0;
 *version=found;return 1;
}
