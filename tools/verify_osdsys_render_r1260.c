/* R1260 30M-instruction renderer check, no buttons pressed.
 * Diagnostic continuation of the recorded R1259 BIOS run.
 * The old checkpoint omits host-side PADMAN DMA binding globals.
 * Recover only those bindings from the successful INIT/OPEN RPC trace,
 * after checking the guest's real opened-port structure. No guest PC,
 * registers or RAM are changed to restore the service bindings. */
#include <stdio.h>
#include <stdlib.h>
#include "core/checkpoint.h"
#include "core/ee/ee_core.c"
#include "core/hw/gs_wii_output.h"
static void picture(int k){gs_state_t*g=gs_get_state();uint64_t fb=(g->pmode&1)?g->dispfb1:g->dispfb2;uint32_t bp,bw,sx,sy,sw,sh;gs_decode_dispfb(fb,&bp,&bw);gs_decode_display_region(fb,(g->pmode&1)?g->display1:g->display2,g->smode2,&sx,&sy,&sw,&sh);if(!bw||!sw||!sh)return;char p[100];snprintf(p,sizeof p,"work/osd-r1260-%d.ppm",k);FILE*f=fopen(p,"wb");fprintf(f,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t c=gs_mem_read_psmct32(bp,bw,sx+((x*((sw<<16)/640))>>16),sy+((y*((sh<<16)/448))>>16));unsigned char b[3]={c,c>>8,c>>16};fwrite(b,1,3,f);}fclose(f);}
int main(int ac,char**av){if(ac<4)return 2;setvbuf(stdout,0,_IOLBF,0);bios_image_t b;if(bios_load(av[1],&b)||system_init(&b,&b)||checkpoint_load(av[2],&b,&b,0))return 3;ee_state_t*s=ee_core_get_state();
 if(ee_mem_read32(s,0x40d110)!=1||ee_mem_read32(s,0x40d11c)!=0x2fd000||ee_mem_read32(s,0x40d4c4)!=1)return 4;
 g_ee_pad_new_stat=0x40d4c0;g_ee_pad_new_bound[0][0]=0x2fd000;iop_sio2_pad_connect();
 unsigned keys[]={0};
 for(unsigned k=0;k<sizeof keys/sizeof keys[0];k++){iop_sio2_pad_set_buttons(keys[k]);uint64_t target=s->instructions_executed+30000000;unsigned calls=0;while(s->instructions_executed<target&&!s->halted&&calls++<3000)system_run_interleaved(50000);picture(k);printf("NAV %u EE=%llu pc=%x buttons=%x halt=%u calls=%u\n",k,(unsigned long long)s->instructions_executed,s->pc,keys[k],s->halted,calls);if(s->halted||calls>=3000)return 5;}
 iop_sio2_pad_set_buttons(0);return checkpoint_save(av[3]);}
