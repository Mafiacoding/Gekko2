#include <stdio.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
int main(int argc,char**argv){bios_image_t b;if(argc<2||bios_load(argv[1],&b))return 2;
for(int i=0;i<3;i++){if(system_init(&b,&b))return 3;ee_state_t*s=ee_core_get_state();if(!s->ram||s->instructions_executed)return 4;system_run_interleaved(1000);if(s->halted)return 5;iop_cdvd_unmount_iso();iop_cdrom_legacy_unmount_iso();ee_core_shutdown();iop_core_shutdown();}puts("PASS three BIOS core allocation / boot / shutdown / cold reset cycles");return 0;}
