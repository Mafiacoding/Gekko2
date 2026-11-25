#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/gs_wii_output.h"
static uint32_t memory[2+512*3],before[2+512*3];
int main(void){unsigned checks=0;
 for(unsigned w=8;w<=1024;w+=8)for(unsigned wide=0;wide<2;wide++){
  unsigned pairs=w/2,active=pairs*3/4,left=(pairs-active)/2;
  for(unsigned i=0;i<2+pairs*3;i++)memory[i]=0x12340000+i;
  memcpy(before,memory,sizeof memory);assert(gs_xfb_fit_4_3(memory+1,w,3,wide));
  assert(memory[0]==before[0]&&memory[1+pairs*3]==before[1+pairs*3]);
  for(unsigned y=0;y<3;y++)for(unsigned x=0;x<pairs;x++){
   uint32_t expected=before[1+y*pairs+x];
   if(wide)expected=x<left||x>=left+active?0x10801080u:before[1+y*pairs+((2*(x-left)+1)*pairs)/(2*active)];
   assert(memory[1+y*pairs+x]==expected);
  }checks++;
 }
 memcpy(before,memory,sizeof memory);assert(!gs_xfb_fit_4_3(memory,641,1,1));assert(!memcmp(before,memory,sizeof memory));
 printf("PASS %u XFB aspect fits: 4:3 identity, 16:9 pillarbox, YUV pair alignment and bounds\n",checks);
}
