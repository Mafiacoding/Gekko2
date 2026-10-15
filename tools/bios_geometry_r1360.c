#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/vu.h"
#include "core/hw/gif.h"
#include "core/hw/gs.h"
#include "core/checkpoint.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 if(argc!=4)return 2;setvbuf(stdout,NULL,_IOLBF,0);
 bios_image_t bios;if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 3;
 uint64_t limit=strtoull(argv[2],NULL,0),next=10000000;ee_state_t *e=ee_core_get_state();
 while(e->instructions_executed<limit&&!e->halted){
  system_run_interleaved(100000);
  if(e->instructions_executed>=next){
   vu1_state_t *v=vu1_get_state();gif_state_t *g=gif_get_state();
   printf("EE=%llu PC=%08x IOP=%llu VU1=%llu missing=%llu triangles=%llu degenerate=%llu PMODE=%llx\n",
    (unsigned long long)e->instructions_executed,e->pc,(unsigned long long)iop_core_get_state()->instructions_executed,
    (unsigned long long)v->instructions_executed,(unsigned long long)v->unimplemented_opcodes_seen,
    (unsigned long long)g->triangles_drawn,(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEGENERATE),
    (unsigned long long)gs_get_state()->pmode);next+=10000000;
  }
 }
 return checkpoint_save(argv[3]);
}
