/* Read-only native BIOS survey. No instruction, PC or ROM patches.
 * Host interpreter evidence is separate from Wii PPC JIT performance. */
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/ipu.h"
#include "core/hw/iop_hle_bios.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static uint32_t hash(const void *data,unsigned size){const unsigned char *p=data;uint32_t v=2166136261u;for(unsigned n=0;n<size;n++)v=(v^p[n])*16777619u;return v;}
int main(int argc,char **argv)
{
 if(argc!=3)return 2;bios_image_t bios;if(bios_load(argv[1],&bios))return 3;
 gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_FASTMEM);
 if(system_init(&bios,&bios))return 4;
 struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);
 unsigned budget=(unsigned)strtoul(argv[2],0,10);system_run_interleaved(budget);clock_gettime(CLOCK_MONOTONIC,&end);
 ee_state_t *e=ee_core_get_state();iop_state_t *i=iop_core_get_state();ipu_profile_t p;ipu_get_profile(&p);
 printf("BIOS_SURVEY EE=%llu PC=%08x IOP=%llu PC=%08x halt=%u/%u EE_regs=%08x IOP_regs=%08x EE_RAM=%08x IOP_RAM=%08x elapsed_ms=%.3f IPU_unimplemented=%llu\n",
 (unsigned long long)e->instructions_executed,(unsigned)e->pc,(unsigned long long)i->instructions_executed,(unsigned)i->pc,e->halted,i->halted,
 hash(e->gpr,sizeof e->gpr),hash(i->gpr,sizeof i->gpr),hash(e->ram,e->ram_size),hash(i->ram,i->ram_size),
 (end.tv_sec-start.tv_sec)*1000.+(end.tv_nsec-start.tv_nsec)/1e6,(unsigned long long)p.unimplemented_commands);
 for(unsigned n=0;n<10;n++)printf("IPU_COMMAND_%u=%llu\n",n,(unsigned long long)p.commands[n]);
 return e->halted||i->halted;
}
