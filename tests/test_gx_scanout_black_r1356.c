#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/gs_gx.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
int main(void){
 gs_mem_init();uint8_t out[256*4*4];
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<256;x++)gs_mem_write_psmct32(0,256,x,y,x|((255-x)<<8)|(x<<16));
 if(gs_gx_pack_rgba8(out,sizeof out,0,256,0,0,256,4)!=sizeof out)return 1;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<256;x++){
  unsigned t=(x/4)*64+y*8+(x%4)*2;
  if(out[t]!=255||out[t+1]!=x||out[t+32]!=255-x||out[t+33]!=x)return 2;
  if(gs_mem_read_psmct32(0,256,x,y)!=(x|((255-x)<<8)|(x<<16)))return 3;
 }
 /* XFB black stays the studio-range black expected by VI. */
 if(gs_rgb8_pair_to_ycbcr(0,0,0,0,0,0)!=0x10801080u)return 4;
 puts("PASS all 256 RGB levels preserved in GX tiled upload, alpha opaque, VRAM unchanged; software XFB black Y=16");return 0;
}
