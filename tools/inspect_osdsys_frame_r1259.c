/* Read-only checkpoint inspection. No guest execution or memory edits. */
#include <stdio.h>
#include <stdint.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
#include "core/hw/gif.h"
#include "core/hw/vu.h"
int main(int ac,char **av){
 if(ac<4)return 2;bios_image_t b;
 if(bios_load(av[1],&b)||system_init(&b,&b)||checkpoint_load(av[2],&b,&b,0))return 3;
 ee_state_t *e=ee_core_get_state();gs_state_t*g=gs_get_state();gif_state_t*f=gif_get_state();vu1_state_t*v=vu1_get_state();
 uint64_t fb=(g->pmode&1)?g->dispfb1:g->dispfb2,display=(g->pmode&1)?g->display1:g->display2;
 uint32_t bp,bw,sx,sy,sw,sh;gs_decode_dispfb(fb,&bp,&bw);gs_decode_display_region(fb,display,g->smode2,&sx,&sy,&sw,&sh);
 printf("EE=%llu PC=%08x HALT=%u Count=%u\n",(unsigned long long)e->instructions_executed,e->pc,e->halted,e->cop0[9]);
 printf("GS PMODE=%llx SMODE2=%llx FB=%llx DISPLAY=%llx BP=%u BW=%u region=%u,%u %ux%u\n",(unsigned long long)g->pmode,(unsigned long long)g->smode2,(unsigned long long)fb,(unsigned long long)display,bp,bw,sx,sy,sw,sh);
 printf("VU1 instructions=%llu unimplemented=%llu PATH1=%llu sprites=%llu triangles=%llu lines=%llu\n",(unsigned long long)v->instructions_executed,(unsigned long long)v->unimplemented_opcodes_seen,(unsigned long long)f->gif_path1_transfers,(unsigned long long)f->sprites_drawn,(unsigned long long)f->triangles_drawn,(unsigned long long)f->lines_drawn);
 if(!bw||!sw||!sh)return 4;
 FILE*out=fopen(av[3],"wb");if(!out)return 5;fprintf(out,"P6\n640 448\n255\n");
 uint32_t dx=(sw<<16)/640,dy=(sh<<16)/448;
 for(uint32_t y=0;y<448;y++)for(uint32_t x=0;x<640;x++){
  uint32_t p=gs_mem_read_psmct32(bp,bw,sx+((x*dx)>>16),sy+((y*dy)>>16));
  unsigned char rgb[3]={p,p>>8,p>>16};fwrite(rgb,1,3,out);
 }
 fclose(out);return 0;
}
