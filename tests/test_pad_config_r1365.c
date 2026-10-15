#include "core/hw/iop_sio2.h"
#include <assert.h>
#include <stdio.h>
static unsigned commands;
static void xfer(unsigned cmd,const uint8_t payload[6],unsigned n,uint8_t out[32])
{
 uint8_t in[32]={1,cmd,0};
 for(unsigned i=0;i<6;i++)in[i+3]=payload?payload[i]:0;
 iop_sio2_mmio_write32(IOP_SIO2_BASE+0x68,4);
 for(unsigned i=0;i<n;i++)iop_sio2_mmio_write8(IOP_SIO2_BASE+0x60,in[i]);
 iop_sio2_mmio_write32(IOP_SIO2_BASE+0x68,1);
 for(unsigned i=0;i<32;i++)iop_sio2_mmio_read8(IOP_SIO2_BASE+0x64,out+i);
 commands++;
}
int main(void)
{
 uint8_t out[32],enter[]={1,0,0,0,0,0},leave[]={0,0,0,0,0,0};
 iop_sio2_init();
 xfer(0x44,enter,9,out);assert(!iop_sio2_pad_is_analog_mode()); /* config gate */
 xfer(0x43,enter,3,out);xfer(0x45,0,9,out);assert(out[1]==255); /* partial enter */
 xfer(0x43,enter,9,out);assert(out[1]==0x41);
 xfer(0x45,0,9,out);assert(out[1]==0xf3&&out[2]==0x5a&&out[3]==3&&out[4]==2&&out[5]==0&&out[6]==2&&out[7]==1);
 uint8_t analog[]={1,3,0,0,0,0};xfer(0x44,analog,9,out);assert(iop_sio2_pad_is_analog_mode());
 xfer(0x45,0,9,out);assert(out[5]==1);
 xfer(0x46,leave,9,out);assert(out[5]==1&&out[6]==2&&out[7]==0&&out[8]==10);
 xfer(0x46,enter,9,out);assert(out[5]==1&&out[6]==1&&out[7]==1&&out[8]==20);
 xfer(0x47,0,9,out);assert(out[5]==2&&out[7]==1);
 xfer(0x4c,leave,9,out);assert(out[6]==4);
 xfer(0x4c,enter,9,out);assert(out[6]==7);
 uint8_t motor[]={0,1,255,255,255,255};xfer(0x4d,motor,9,out);for(unsigned i=3;i<9;i++)assert(out[i]==255);
 xfer(0x4d,motor,9,out);assert(out[3]==0&&out[4]==1&&out[5]==255);
 xfer(0x41,0,9,out);assert(out[3]==255&&out[4]==255&&out[5]==3&&out[8]==0x5a);
 xfer(0x40,0,9,out);assert(out[5]==2&&out[8]==0x5a);
 uint8_t native[]={255,255,3,0,0,0};xfer(0x4f,native,9,out);assert(out[8]==0x5a);
 xfer(0x43,leave,9,out);assert(out[1]==0xf3);
 iop_sio2_pad_set_buttons(IOP_PAD_BTN_CROSS|IOP_PAD_BTN_LEFT|IOP_PAD_BTN_R2);
 iop_sio2_pad_set_analog_axes(12,34,56,78);xfer(0x42,0,21,out);
 assert(out[1]==0x79&&out[2]==0x5a&&out[5]==12&&out[6]==34&&out[7]==56&&out[8]==78);
 for(unsigned i=0;i<12;i++)assert(out[9+i]==((i==1||i==6||i==11)?255:0));
 assert(out[21]==255); /* stop at pressure packet end */
 xfer(0x43,enter,21,out);uint8_t analog_mask[]={0x3f,0,0,0,0,0};xfer(0x4f,analog_mask,9,out);
 xfer(0x43,leave,9,out);xfer(0x42,0,21,out);assert(out[1]==0x73&&out[9]==255);
 xfer(0x43,enter,9,out);xfer(0x4f,leave,9,out);xfer(0x43,leave,9,out);
 xfer(0x42,0,21,out);assert(out[1]==0x41&&out[5]==255);
 iop_sio2_pad_disconnect();xfer(0x43,enter,9,out);for(unsigned i=0;i<9;i++)assert(out[i]==255);
 iop_sio2_init();xfer(0x45,0,9,out);assert(out[1]==255);
 printf("R1365 DualShock2: %u command batches passed; modes/config/partial transfers/metadata/pressure/motor map/reset\n",commands);
 return 0;
}
