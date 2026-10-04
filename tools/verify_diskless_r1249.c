#include <stdio.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
int main(int argc,char **argv){
 if(argc<2)return 2; setvbuf(stdout,NULL,_IOLBF,0);
 bios_image_t bios; if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 3;
 ee_state_t *s=ee_core_get_state();
 for(unsigned j=0;j<100 && !s->halted;j++){
 system_run_interleaved(1000000);
 uint64_t a,b,h,re;uint32_t cd,se,hit,w[16];
 ee_core_get_r1190_sif_diag(&a,&b,&h,&re,&cd,&se);
 ee_core_get_r1246_writer(&hit,w);
 printf("BOOT n=%llu pc=%08x sp=%08x ra=%08x tid=%d REND=%llu writer=%u\n",(unsigned long long)s->instructions_executed,s->pc,(unsigned)s->gpr[29].ud0,(unsigned)s->gpr[31].ud0,ee_hle_thread_get_current_thread_id(),(unsigned long long)re,hit);
 uint64_t rc[4];uint32_t rl[6];ee_core_get_r1249_repair(rc,rl);
 printf("REPAIR=%llu CONT=%llu LD=%llu JR=%llu last_slot=%08x target=%08x\n",(unsigned long long)rc[0],(unsigned long long)rc[1],(unsigned long long)rc[2],(unsigned long long)rc[3],rl[2],rl[5]);
 if(hit)printf("WRITE pc=%08x val=%08x:%08x sp=%08x ra=%08x tid=%u\n",w[0],w[4],w[3],w[9],w[10],w[11]);
 }
 return 0;
}
