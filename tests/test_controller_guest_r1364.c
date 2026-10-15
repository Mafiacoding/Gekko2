#include "core/hw/controller_input.h"
#include "core/hw/iop_sio2.h"
#include <assert.h>
#include <stdio.h>
static void read_pad(uint8_t out[9])
{
 uint8_t command[9]={1,0x42};
 iop_sio2_mmio_write32(IOP_SIO2_BASE+0x68,4);
 for(unsigned i=0;i<9;i++)iop_sio2_mmio_write8(IOP_SIO2_BASE+0x60,command[i]);
 iop_sio2_mmio_write32(IOP_SIO2_BASE+0x68,1);
 for(unsigned i=0;i<9;i++)iop_sio2_mmio_read8(IOP_SIO2_BASE+0x64,out+i);
}
int main(void)
{
 controller_pad p;uint8_t out[9];
 iop_sio2_init();
 controller_map_drc(&p,0x80c8,0xc0,-75,75,75,-75);
 iop_sio2_pad_set_buttons(p.guest);
 iop_sio2_pad_set_analog_axes(p.rx,p.ry,p.lx,p.ly);
 read_pad(out);
 assert(out[1]==0x41);
 assert((uint16_t)(out[3]|out[4]<<8)==(uint16_t)~p.guest);
 /* Host axes do not force a guest analog-mode switch. */
 assert(!iop_sio2_pad_is_analog_mode());
 iop_sio2_pad_set_analog_mode(1);read_pad(out);
 assert(out[1]==0x73&&out[2]==0x5a);
 assert(out[5]==255&&out[6]==255&&out[7]==0&&out[8]==0);
 controller_neutral(&p);
 iop_sio2_pad_set_buttons(p.guest);
 iop_sio2_pad_set_analog_axes(p.rx,p.ry,p.lx,p.ly);
 read_pad(out);
 assert(out[3]==255&&out[4]==255);
 for(unsigned i=5;i<9;i++)assert(out[i]==128);
 printf("R1364 GamePad -> SIO2 digital/analog replies, inverted buttons and disconnect neutral: passed\n");
 return 0;
}
