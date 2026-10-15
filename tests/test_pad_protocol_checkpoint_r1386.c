#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/ee/ee_core.h"
#include "core/hw/iop_sio2.h"
int main(void)
{
    bios_image_t bios={0};char path[]="/tmp/gekko2-pad-protocol-XXXXXX";
    int fd=mkstemp(path);assert(fd>=0);close(fd);
    assert(system_init(&bios,&bios)==0);
    iop_sio2_pad_set_analog_mode(1);iop_sio2_pad_set_analog_axes(3,7,11,19);
    uint8_t original[IOP_PAD_PROTOCOL_BYTES],got[IOP_PAD_PROTOCOL_BYTES];
    iop_sio2_pad_protocol_save(original);assert(checkpoint_save(path)==0);
    iop_sio2_init();assert(!iop_sio2_pad_is_analog_mode());
    iop_sio2_pad_set_buttons(IOP_PAD_BTN_CROSS);
    assert(checkpoint_load(path,&bios,&bios,NULL)==0);
    iop_sio2_pad_protocol_save(got);assert(!memcmp(original,got,sizeof got));
    assert(iop_sio2_pad_get_buttons()==IOP_PAD_BTN_CROSS);
    got[3]=2;assert(!iop_sio2_pad_protocol_valid(got));iop_sio2_pad_protocol_load(got);
    iop_sio2_pad_protocol_save(got);assert(!memcmp(original,got,sizeof got));
    ee_state_t *st=ee_core_get_state();ee_pad_checkpoint_t old={{0},{{0}},0};
    old.legacy_area[0]=0x200000;st->ram[0x200009]=0x73;
    iop_sio2_pad_set_analog_mode(0);ee_core_pad_checkpoint_load(&old);
    assert(iop_sio2_pad_is_analog_mode());
    old.new_area[0][0]=0x201000;st->ram[0x201001]=0x41;
    ee_core_pad_checkpoint_load(&old);assert(!iop_sio2_pad_is_analog_mode());
    unlink(path);puts("PAD protocol checkpoint and legacy recovery: PASS");return 0;
}
