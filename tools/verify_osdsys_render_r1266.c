/* Continue an authentic BIOS checkpoint with PAD released; capture active GS display.
 * CLI: BIOS checkpoint-in checkpoint-out [steps<=60], each step 3M EE instructions.
 * Use verify_osdsys_setup_r1265.c for fresh boot; this tool edits no guest PC/RAM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/hw/gs_wii_output.h"
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/iop_sio2.h"
static void frame(int step){gs_state_t*g=gs_get_state();uint32_t bp,bw,sx,sy,sw,sh;uint64_t fb=(g->pmode&1)?g->dispfb1:g->dispfb2;gs_decode_dispfb(fb,&bp,&bw);gs_decode_display_region(fb,(g->pmode&1)?g->display1:g->display2,g->smode2,&sx,&sy,&sw,&sh);if(!bw||!sw||!sh)return;char path[100];snprintf(path,sizeof path,"work/osd-r1266-render-input-%d.ppm",step);FILE*f=fopen(path,"wb");fprintf(f,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t p=gs_mem_read_psmct32(bp,bw,sx+((x*((sw<<16)/640))>>16),sy+((y*((sh<<16)/448))>>16));unsigned char b[3]={p,p>>8,p>>16};fwrite(b,1,3,f);}fclose(f);}
int main(int ac,char**av){if(ac<4)return 2;setvbuf(stdout,0,_IOLBF,0);bios_image_t b;if(bios_load(av[1],&b)||system_init(&b,&b))return 3;if(checkpoint_load(av[2],&b,&b,0))return 6;iop_sio2_pad_connect();ee_state_t*s=ee_core_get_state();unsigned steps=ac>4?strtoul(av[4],0,10):24;if(steps>60)return 7;for(unsigned step=0;step<steps;step++){iop_sio2_pad_set_buttons(0);uint64_t end=s->instructions_executed+3000000;unsigned bounded=0;while(s->instructions_executed<end&&!s->halted&&bounded++<3000)system_run_interleaved(50000);if(s->halted||bounded>=3000){checkpoint_save(av[3]);return 5;}frame(step);printf("STEP %d EE=%llu pc=%x buttons=%x padcmd=%u halt=%u\n",step,(unsigned long long)s->instructions_executed,s->pc,iop_sio2_pad_get_buttons(),iop_sio2_get_pad_command_count(),s->halted);}iop_sio2_pad_set_buttons(0);return checkpoint_save(av[3]);}
