/* Private-image native diagnostic. Uses the launcher's actual cold disc boot.
 * Never patches guest code or fabricates PC/register/video progress. */
#include "core/checkpoint.h"
#include "core/ee/ee_hle_thread.h"
#include "core/hw/sif.h"
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/gs.h"
#include "core/hw/gif.h"
#include "core/hw/vu.h"
#include "core/hw/ipu.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
 if(argc<4)return 2;setvbuf(stdout,0,_IOLBF,0);
 bios_image_t bios;if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 3;
 if(argv[2][0]!='-'){
  int a=iop_cdvd_mount_iso(argv[2]),b=iop_cdrom_legacy_mount_iso(argv[2]);
  if(a&&b)return 4;if(!a)iop_cdvd_set_disc_present(0x12);
 }
 unsigned slices=strtoul(argv[3],0,0);
 for(unsigned n=0;n<slices;n++){
  system_run_interleaved(1000000);
  ee_state_t *e=ee_core_get_state();iop_state_t *i=iop_core_get_state();gs_state_t *g=gs_get_state();ipu_profile_t p;ipu_get_profile(&p);
  iop_cdvd_checkpoint_t cd;iop_cdvd_checkpoint_save(&cd);
  printf("SURVEY slice=%u EE=%llu PC=%08x IOP=%llu PC=%08x halted=%u/%u PMODE=%llx GIF=%llu VU=%llu CDVD_N=%llu CDVD_S=%llu IPU_input=%llu busy=%u last=%08x unimplemented=%llu LOADFILE_version=%08x\n",n,
  (unsigned long long)e->instructions_executed,e->pc,(unsigned long long)i->instructions_executed,i->pc,e->halted,i->halted,
  (unsigned long long)g->pmode,(unsigned long long)gif_get_state()->quadwords_seen,(unsigned long long)vu1_get_state()->instructions_executed,
  (unsigned long long)cd.ncmd_call_count,(unsigned long long)cd.scmd_call_count,(unsigned long long)p.accepted_qwc,p.busy,p.last_command,(unsigned long long)p.unimplemented_commands,e->loadfile_rpc_version);
  if((n==3||e->halted||i->halted) && argc>4){
   printf("CHECKPOINT rc=%d path=%s\n",checkpoint_save(argv[4]),argv[4]);
   printf("SIF_INIT=%u BINDS=%u current_thread=%d\n",sif_cmd_iop_get_init_cmd_count(),sif_cmd_iop_get_rpc_bind_count(),ee_hle_thread_get_current_thread_id());
   uint32_t cd[64],sid[64];unsigned count=sif_cmd_iop_dump_bind_table(cd,sid,64);
   for(unsigned j=0;j<count;j++)printf("BIND cd=%08x sid=%08x\n",cd[j],sid[j]);
  }
  if(e->halted||i->halted){printf("HALT %s | %s\n",e->halt_reason,i->halt_reason);break;}
 }
 ipu_profile_t p;ipu_get_profile(&p);for(unsigned n=0;n<10;n++)printf("IPU_COMMAND %u=%llu\n",n,(unsigned long long)p.commands[n]);
 return 0;
}
