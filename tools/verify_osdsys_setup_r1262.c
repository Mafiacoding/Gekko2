/* Fresh real-controller test, CROSS press/release only. No guest-state shortcuts. */
#include <stdio.h>
#include <stdlib.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/iop_sio2.h"
static void frame(int step){gs_state_t*g=gs_get_state();uint32_t bp,bw;gs_decode_dispfb((g->pmode&1)?g->dispfb1:g->dispfb2,&bp,&bw);char path[100];snprintf(path,sizeof path,"work/osd-r1262-input-%d.ppm",step);FILE*f=fopen(path,"wb");fprintf(f,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t p=gs_mem_read_psmct32(bp,bw,x,y);unsigned char b[3]={p,p>>8,p>>16};fwrite(b,1,3,f);}fclose(f);}
int main(int ac,char**av){if(ac<4)return 2;setvbuf(stdout,0,_IOLBF,0);bios_image_t b;if(bios_load(av[1],&b)||system_init(&b,&b))return 3;iop_sio2_pad_connect();ee_state_t*s=ee_core_get_state();uint64_t next=100000000; unsigned calls=0;while(s->instructions_executed<1500000000ull&&!s->halted){system_run_interleaved(50000); if(++calls%256==0){printf("HEART EE=%llu count=%u pc=%x idle=%u halt=%u\n",(unsigned long long)s->instructions_executed,s->cop0[9],s->pc,s->idle,s->halted);frame(99);} if(calls>=10000){frame(98);checkpoint_save(av[3]);return 4;} if(s->instructions_executed>=next){next+=100000000;printf("COLD EE=%llu\n",(unsigned long long)s->instructions_executed);}}for(int step=0;step<15;step++){iop_sio2_pad_set_buttons((step%3==0)?IOP_PAD_BTN_CROSS:0);uint64_t end=s->instructions_executed+30000000;unsigned bounded=0;while(s->instructions_executed<end&&!s->halted&&bounded++<3000)system_run_interleaved(50000);if(s->halted||bounded>=3000){checkpoint_save(av[3]);return 5;}frame(step);printf("STEP %d EE=%llu pc=%x buttons=%x padcmd=%u halt=%u\n",step,(unsigned long long)s->instructions_executed,s->pc,iop_sio2_pad_get_buttons(),iop_sio2_get_pad_command_count(),s->halted);}iop_sio2_pad_set_buttons(0);return checkpoint_save(av[3]);}
