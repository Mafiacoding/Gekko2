#define main prior_protocol_test
#include "test_iop_cdrom_legacy.c"
#undef main
#include "core/system.h"
#include "core/checkpoint.h"
#include <unistd.h>
static uint8_t data_byte(void){uint8_t v;iop_cdrom_legacy_mmio_read8(0x1f801802,&v);return v;}
int main(void){if(prior_protocol_test())return 1;bios_image_t b={0};
 const char *iso="/tmp/gekko2-cdrom-checkpoint-test.iso",*cp="/tmp/gekko2-cdrom-checkpoint-test.ckpt";
 build_synth_iso(iso);CHECK(system_init(&b,&b)==0,"system init");CHECK(iop_cdrom_legacy_mount_iso(iso)==0,"synthetic disc mounted");
 set_intmsk_all();write_param(0);write_param(2);write_param(0x21);write_cmd(2);ack_int();write_cmd(6);
 iop_cdrom_legacy_checkpoint_t saved,got;iop_cdrom_legacy_checkpoint_save(&saved);CHECK(saved.pending!=0,"real pending ReadN second response");
 CHECK(checkpoint_save(cp)==0,"save pending controller");ack_int();CHECK(read_intsts()==1,"normal data-ready response");
 CHECK(checkpoint_load(cp,&b,&b,iso)==0,"restore pending controller");iop_cdrom_legacy_checkpoint_save(&got);CHECK(!memcmp(&got,&saved,sizeof got),"all pending FIFO/register state restored");
 ack_int();CHECK(read_intsts()==1,"second response still delivered after restore");for(unsigned n=0;n<37;n++)CHECK(data_byte()==(uint8_t)n,"real sector prefix");
 iop_cdrom_legacy_checkpoint_save(&saved);CHECK(checkpoint_save(cp)==0,"save partially read sector");for(unsigned n=37;n<100;n++)data_byte();
 CHECK(checkpoint_load(cp,&b,&b,iso)==0,"restore sector cursor");iop_cdrom_legacy_checkpoint_save(&got);CHECK(!memcmp(&got,&saved,sizeof got),"sector data and cursor restored");
 for(unsigned n=37;n<2048;n++)if(data_byte()!=(uint8_t)n){CHECK(0,"remaining sector bytes exact");break;}
 got=saved;got.data_pos=2049;CHECK(iop_cdrom_legacy_checkpoint_load(&got)<0,"reject out-of-range cursor");
 got=saved;got.pending=4;CHECK(!iop_cdrom_legacy_checkpoint_valid(&got),"reject unknown pending phase");
 got=saved;got.result_pos=17;CHECK(!iop_cdrom_legacy_checkpoint_valid(&got),"reject invalid response FIFO position");
 iop_cdrom_legacy_unmount_iso();unlink(cp);unlink(iso);printf("R1357 CDROM checkpoint: %u failures\n",g_fail);return g_fail?1:0;}
