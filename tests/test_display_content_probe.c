#include <stdio.h>
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
int main(void){gs_mem_init();unsigned fail=0;
fail+=gs_display_has_rgb(0,64,0,0,64,32)!=0;
gs_mem_write_psmct32(0,64,0,0,0xff000000);
fail+=gs_display_has_rgb(0,64,0,0,64,32)!=0;
gs_mem_write_psmct32(0,64,16,8,0x80000100);
fail+=gs_display_has_rgb(0,64,0,0,64,32)!=1;
fail+=gs_display_has_rgb(0,64,32,16,32,16)!=0;
fail+=gs_display_has_rgb(0,0,0,0,64,32)!=0;
fail+=gs_display_has_rgb(0,64,0,0,0,32)!=0;
gs_mem_init();gs_mem_write_psmct32(0,64,1,1,0x80000100);
fail+=gs_display_has_rgb(0,64,0,0,64,32)!=1;
gs_mem_init();gs_mem_write_psmct32(0,640,639,255,0x80000001);
fail+=gs_display_has_rgb(0,640,0,0,640,256)!=1;
fail+=gs_display_has_rgb(0,640,0,0,639,255)!=0;
printf("%s actual RGB probe: alpha-only black, displayed region, sparse unsampled RGB and last scanout pixel\n",fail?"FAIL":"PASS");return fail?1:0;}
