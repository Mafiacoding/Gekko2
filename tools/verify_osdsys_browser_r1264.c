/* R1264 controller-only Browser/Back continuation: PAD bindings load from the checkpoint.
 * Only real controller inputs; no BIOS-specific binding recovery. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/system.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/gs_wii_output.h"
static void picture(int k){gs_state_t*g=gs_get_state();uint64_t fb=(g->pmode&1)?g->dispfb1:g->dispfb2;uint32_t bp,bw,sx,sy,sw,sh;gs_decode_dispfb(fb,&bp,&bw);gs_decode_display_region(fb,(g->pmode&1)?g->display1:g->display2,g->smode2,&sx,&sy,&sw,&sh);if(!bw||!sw||!sh)return;char p[100];snprintf(p,sizeof p,"work/osd-r1264-browser-input-%d.ppm",k);FILE*f=fopen(p,"wb");fprintf(f,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t c=gs_mem_read_psmct32(bp,bw,sx+((x*((sw<<16)/640))>>16),sy+((y*((sh<<16)/448))>>16));unsigned char b[3]={c,c>>8,c>>16};fwrite(b,1,3,f);}fclose(f);}
int main(int ac,char**av){if(ac<4)return 2;setvbuf(stdout,0,_IOLBF,0);bios_image_t b;if(bios_load(av[1],&b)||system_init(&b,&b)||checkpoint_load(av[2],&b,&b,0))return 3;ee_state_t*s=ee_core_get_state();
 ee_pad_checkpoint_t pad;ee_core_pad_checkpoint_save(&pad);
 if(!pad.new_status||!pad.new_area[0][0]){puts("PAD bindings missing from checkpoint");return 4;}
 iop_sio2_pad_connect();
 unsigned keys[]={IOP_PAD_BTN_CROSS,0,IOP_PAD_BTN_CIRCLE,0};
 unsigned budgets[]={15000000,60000000,15000000,30000000};
 unsigned steps=(ac>4 && !strcmp(av[4],"enter-only"))?2u:4u;
 unsigned begin=(ac>4&&!strcmp(av[4],"back-only"))?2u:0u;
 for(unsigned k=begin;k<steps;k++){iop_sio2_pad_set_buttons(keys[k]);uint64_t target=s->instructions_executed+budgets[k];unsigned calls=0;while(s->instructions_executed<target&&!s->halted&&calls++<3000){system_run_interleaved(50000);if(calls%30==0)printf("PROGRESS step=%u call=%u EE=%llu pc=%x\n",k,calls,(unsigned long long)s->instructions_executed,s->pc);}picture(k);printf("NAV %u EE=%llu pc=%x buttons=%x halt=%u calls=%u\n",k,(unsigned long long)s->instructions_executed,s->pc,keys[k],s->halted,calls);if(s->halted||calls>=3000)return 5;}
 iop_sio2_pad_set_buttons(0);return checkpoint_save(av[3]);}
