#include "core/ee/ee_core.h"
#include "core/bios_loader.h"
#include <stdlib.h>
#include <stdio.h>
int main(void){
 bios_image_t bios={0};bios.size=BIOS_MAX_SIZE;bios.data=calloc(1,bios.size);bios.loaded=1;
 if(!bios.data||ee_core_init(&bios))return 1;
 ee_state_t *s=ee_core_get_state();
 if(s->vu0_vf[0][0]||s->vu0_vf[0][1]||s->vu0_vf[0][2]||s->vu0_vf[0][3]!=0x3f800000u)return 2;
 puts("PASS VU0 cold reset VF00=(0,0,0,1) before any BIOS instruction");return 0;
}
