#include <stdio.h>
#include "core/system.h"
#include "core/ee/ee_core.c"
int main(void){bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();unsigned fail=0;
uint32_t a=0xa0100000;ee_mem_write32(s,a+256,0x12345678);iop_sio2_pad_connect();iop_sio2_pad_set_buttons(0x4000);ee_pad_new_write_slots(s,a,0,0);
if(ee_mem_read8(s,a+112)!=6||ee_mem_read8(s,a+113)!=0||ee_mem_read8(s,a+103)!=1||ee_mem_read32(s,a+96)!=32)fail++;
if(ee_mem_read16(s,a+2)!=0xbfff||ee_mem_read16(s,a+130)!=0xbfff)fail++;
if(ee_mem_read32(s,a+256)!=0x12345678)fail++;
/* ps2sdk padInfoMode's actual admission checks and CURID result. */
if(ee_mem_read8(s,a+114)!=1||ee_mem_read8(s,a+242)!=1||ee_mem_read8(s,a+113)!=0||(ee_mem_read8(s,a+101)>>4)!=4)fail++;
if(ee_mem_read8(s,a+100)!=2||ee_mem_read8(s,a+104)!=2||ee_mem_read16(s,a+80)!=4||ee_mem_read16(s,a+82)!=7)fail++;
uint32_t r=0xa0120000;g_ee_pad_new_bound[0][0]=a;
uint32_t before=ee_mem_read32(s,a+88);g_ee_vblank_events++;ee_pad_new_write_slots(s,a,0,0);if(ee_mem_read32(s,a+88)!=before+1)fail++;
ee_mem_write32(s,r+4,0);ee_mem_write32(s,r+8,0);ee_mem_write32(s,r+12,1);ee_mem_write32(s,r+16,3);
iop_sio2_pad_set_analog_axes(12,34,56,78);ee_pad_new_mode_rpc(s,r);
if(ee_mem_read32(s,r+20)!=1||!iop_sio2_pad_is_analog_mode()||ee_mem_read8(s,a+1)!=0x73||ee_mem_read8(s,a+101)!=0x73||ee_mem_read8(s,a+105)!=1||ee_mem_read8(s,a+4)!=12||ee_mem_read8(s,a+7)!=78||ee_mem_read8(s,a+132)!=12)fail++;
ee_mem_write32(s,r+12,2);ee_pad_new_mode_rpc(s,r);if(ee_mem_read32(s,r+20)!=0||!iop_sio2_pad_is_analog_mode())fail++;
ee_mem_write32(s,r+4,1);ee_mem_write32(s,r+12,0);ee_pad_new_mode_rpc(s,r);if(ee_mem_read32(s,r+20)!=0||!iop_sio2_pad_is_analog_mode())fail++;
ee_mem_write32(s,r+4,0);ee_pad_new_mode_rpc(s,r);if(ee_mem_read32(s,r+20)!=1||iop_sio2_pad_is_analog_mode()||ee_mem_read8(s,a+1)!=0x41||ee_mem_read8(s,a+105)!=0)fail++;
ee_pad_new_write_slots(s,a,1,0);if(ee_mem_read8(s,a+112)!=0||ee_mem_read32(s,a+96)!=0||ee_mem_read8(s,a+114)!=0)fail++;
ee_pad_new_write_slots(s,a,0,1);if(ee_mem_read8(s,a+112)!=0)fail++;
ee_pad_new_reset();g_ee_pad_new_stat=a;ee_pad_area_refresh_all(s);
if(ee_mem_read32(s,a+4)!=1||ee_mem_read32(s,a+8)!=0||ee_mem_read32(s,a+132)!=1||ee_mem_read32(s,a+136)!=0)fail++;
iop_sio2_pad_disconnect();ee_pad_area_refresh_all(s);
if(ee_mem_read32(s,a+4)!=0||ee_mem_read32(s,a+132)!=0)fail++;
iop_sio2_pad_connect();
printf("R1357 XPADMAN DMA / padInfoMode admission: %u failures (modes/RPC/axes, buttons, disconnected ports, two buffers, bounds)\n",fail);return fail?1:0;}
