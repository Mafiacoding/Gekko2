#include <stdio.h>
#include "core/ee/ee_core.c"
int main(void){bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();unsigned fail=0;
uint32_t a=0xa0100000;ee_mem_write32(s,a+256,0x12345678);iop_sio2_pad_connect();iop_sio2_pad_set_buttons(0x4000);ee_pad_new_write_slots(s,a,0,0);
if(ee_mem_read8(s,a+112)!=6||ee_mem_read8(s,a+113)!=0||ee_mem_read8(s,a+103)!=1||ee_mem_read32(s,a+96)!=32)fail++;
if(ee_mem_read16(s,a+2)!=0xbfff||ee_mem_read16(s,a+130)!=0xbfff)fail++;
if(ee_mem_read32(s,a+256)!=0x12345678)fail++;
ee_pad_new_write_slots(s,a,1,0);if(ee_mem_read8(s,a+112)!=0||ee_mem_read32(s,a+96)!=0)fail++;
ee_pad_new_write_slots(s,a,0,1);if(ee_mem_read8(s,a+112)!=0)fail++;
ee_pad_new_reset();g_ee_pad_new_stat=a;ee_pad_area_refresh_all(s);
if(ee_mem_read32(s,a+4)!=1||ee_mem_read32(s,a+8)!=0||ee_mem_read32(s,a+132)!=1||ee_mem_read32(s,a+136)!=0)fail++;
iop_sio2_pad_disconnect();ee_pad_area_refresh_all(s);
if(ee_mem_read32(s,a+4)!=0||ee_mem_read32(s,a+132)!=0)fail++;
iop_sio2_pad_connect();
printf("New PAD DMA: %u failures (buttons, disconnected ports, two buffers, bounds)\n",fail);return fail?1:0;}
