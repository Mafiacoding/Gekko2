#include "core/hw/gs_mem.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t before[GS_MEM_SIZE];
static uint32_t rd(unsigned kind,uint32_t bp,uint32_t x,uint32_t y,uint32_t psm) {
 switch(kind) {
 case 0:return gs_mem_read_psmct32(bp,64,x,y);
 case 1:return gs_mem_read_psmct32_swizzled(bp,64,x,y);
 case 2:return gs_mem_read_psmct16(bp,64,x,y);
 case 3:return gs_mem_read_psmct16s(bp,64,x,y);
 default:return gs_mem_read_z(bp,64,x,y,psm);
 }
}
static void wr(unsigned kind,uint32_t bp,uint32_t x,uint32_t y,uint32_t psm,uint32_t v) {
 switch(kind) {
 case 0:gs_mem_write_psmct32(bp,64,x,y,v);break;
 case 1:gs_mem_write_psmct32_swizzled(bp,64,x,y,v);break;
 case 2:gs_mem_write_psmct16(bp,64,x,y,v);break;
 case 3:gs_mem_write_psmct16s(bp,64,x,y,v);break;
 default:gs_mem_write_z(bp,64,x,y,psm,v);break;
 }
}
int main(void) {
 unsigned checks=0;
 for(unsigned kind=0;kind<8;kind++) {
  unsigned psm=kind<4?0:kind==4?0:kind==5?1:kind==6?2:10;
  unsigned bytes=(kind==2||kind==3||psm==2||psm==10)?2:psm==1?3:4;
  uint32_t x=kind==1?63:bytes==2?8:0,y=kind==1?31:0;
  uint32_t bad=kind==1?0x7ffffu:kind<4?0x3fffffffu:0x3ffff9ffu;
  uint32_t good=kind==1?511:kind<4?1048575u:((GS_MEM_SIZE-4u)^6144u)/4u;
  gs_mem_init();memcpy(before,gs_mem_get(),GS_MEM_SIZE);
  for(unsigned delta=0;delta<2;delta++) {
   if(rd(kind,bad-delta,x,y,psm)!=0)return 1;
   wr(kind,bad-delta,x,y,psm,0xaabbccdd);
   if(memcmp(before,gs_mem_get(),GS_MEM_SIZE))return 2;
   checks+=2;
  }
  uint32_t mask=bytes==4?UINT32_MAX:(1u<<(bytes*8))-1u;
  wr(kind,good,x,y,psm,0xaabbccdd);
  if(rd(kind,good,x,y,psm)!=(0xaabbccdd&mask))return 3;
  unsigned off=GS_MEM_SIZE-(bytes==2?2:4);
  for(unsigned n=0;n<bytes;n++)if(gs_mem_get()[off+n]!=(uint8_t)(0xaabbccdd>>(n*8)))return 4;
  checks+=2;
 }
 printf("PASS %u GS bounds cases including all compatibility APIs\n",checks);return 0;
}
