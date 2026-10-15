#include <assert.h>
#include <stdio.h>
#include "../source/hw/vif.c"
int main(void) {
 vif_state_t v={0};
 for(unsigned cl=0;cl<256;cl++) for(unsigned wl=1;wl<=256;wl++)
 for(unsigned num=1;num<=256;num++) {
  v.cycle_cl=cl; v.cycle_wl=wl==256?0:wl;
  unsigned n=cl>=wl?num:cl*(num/wl)+((num%wl)<cl?(num%wl):cl);
  unsigned expected=(n*12+3)&~3u;
  assert(vif_unpack_needed_bytes(&v,0x68,num)==expected);
 }
 puts("VIF payload: 16777216 fill/skip combinations passed");
 return 0;
}
