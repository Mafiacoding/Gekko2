/* Read-only check of the supplied SCPH-50004 OSDSYS's early libmc state.
 * CLI: BIOS early-checkpoint. No firmware bytes or guest state edits. */
#include <stdio.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
static uint32_t rd(ee_state_t*s,uint32_t a){return s->ram[a]|((uint32_t)s->ram[a+1]<<8)|((uint32_t)s->ram[a+2]<<16)|((uint32_t)s->ram[a+3]<<24);}
int main(int ac,char**av){
 if(ac!=3)return 2;bios_image_t b;if(bios_load(av[1],&b)||system_init(&b,&b)||checkpoint_load(av[2],&b,&b,0))return 3;
 ee_state_t*s=ee_core_get_state();if(s->ram_size<0x413000)return 4;
 uint32_t server=rd(s,0x411a24),result=rd(s,0x412fc0),serv=rd(s,0x412fc4),man=rd(s,0x412fc8);
 printf("EE=%llu halt=%u tracked=%x/%x libmc.server=%x reply=%d version=%x/%x\n",(unsigned long long)s->instructions_executed,s->halted,s->mcserv_module_version,s->mcman_module_version,server,(int32_t)result,serv,man);
 unsigned failed=0;failed+=s->mcserv_module_version!=0x208;failed+=s->mcman_module_version!=0x209;failed+=!server;failed+=serv!=0x208||man!=0x209;failed+=(int32_t)result!=-11;
 printf("SCPH-50004 early libmc: %u failures across provider metadata, live connection, retained INIT versions and actual no-card reply\n",failed);return failed?1:0;
}
