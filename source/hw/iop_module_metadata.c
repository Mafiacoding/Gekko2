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
