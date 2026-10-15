#include <stdio.h>
#include <string.h>
#include "core/system.h"
#include "core/ee/ee_core.c"
static unsigned fail;
#define CHECK(x) do{if(!(x)){printf("FAIL line %u\n",__LINE__);fail++;}}while(0)
int main(void){bios_image_t b={0};CHECK(system_init(&b,&b)==0);ee_state_t *s=ee_core_get_state();uint32_t r=0x80100000,a=0x80110000;
 iop_sio2_pad_connect();iop_sio2_pad_set_buttons(IOP_PAD_BTN_START);
 #define REQUEST(c,p,sl,x,y) do{ee_mem_write32(s,r,c);ee_mem_write32(s,r+4,p);ee_mem_write32(s,r+8,sl);ee_mem_write32(s,r+12,x);ee_mem_write32(s,r+16,y);ee_mem_write32(s,r+20,0xdeadbeef);ee_pad_legacy_rpc(s,r);}while(0)
 REQUEST(0x80000100u,0,0,0,a);CHECK(ee_mem_read32(s,r+12)==1);CHECK(g_ee_pad_area_bound[0]==a);CHECK(ee_mem_read32(s,a+40)==4);CHECK(ee_mem_read16(s,a+10)==(uint16_t)~IOP_PAD_BTN_START);
 for(unsigned b=0;b<16;b++){iop_sio2_pad_set_buttons((uint16_t)(1u<<b));ee_pad_area_refresh_all(s);CHECK(ee_mem_read16(s,a+10)==(uint16_t)~(1u<<b));}
 unsigned char low[128];memcpy(low,s->ram,128);
 REQUEST(0x80000104u,0,0,4,UINT32_MAX);CHECK(ee_mem_read32(s,r+20)==2);CHECK(!memcmp(low,s->ram,128));
 REQUEST(0x80000104u,0,0,4,1);CHECK(ee_mem_read32(s,r+20)==7);CHECK(!memcmp(low,s->ram,128));
 REQUEST(0x80000105u,0,0,1,3);CHECK(ee_mem_read32(s,r+20)==1);CHECK(iop_sio2_pad_is_analog_mode());CHECK(ee_mem_read8(s,a+9)==0x73);CHECK(ee_mem_read32(s,a+40)==8);CHECK(!memcmp(low,s->ram,128));
 REQUEST(0x80000104u,0,0,1,0);CHECK(ee_mem_read32(s,r+20)==7);
 REQUEST(0x80000105u,0,0,0,0);CHECK(!iop_sio2_pad_is_analog_mode());CHECK(ee_mem_read8(s,a+9)==0x41);
 REQUEST(0x80000105u,0,0,2,0);CHECK(ee_mem_read32(s,r+20)==0);CHECK(!iop_sio2_pad_is_analog_mode());
 REQUEST(0x80000104u,1,0,4,UINT32_MAX);CHECK(ee_mem_read32(s,r+20)==0);
 for(unsigned mode=2;mode<12;mode++){if(mode==4||mode==5)continue;REQUEST(0x80000100u+mode,0,0,0,1);CHECK(!memcmp(low,s->ram,128));CHECK(g_ee_pad_area_bound[0]==a);}
 uint32_t frame=ee_mem_read32(s,a);g_ee_vblank_events++;ee_pad_area_refresh_all(s);CHECK(ee_mem_read32(s,a)==frame+1);
 REQUEST(0x80000100u,0,0,0,1);CHECK(ee_mem_read32(s,r+12)==0);CHECK(g_ee_pad_area_bound[0]==a);
 REQUEST(0x80000100u,0,0,0,0x81ffffc0);CHECK(ee_mem_read32(s,r+12)==0);
 REQUEST(0x8000010du,0,0,0,0);CHECK(g_ee_pad_area_bound[0]==0);CHECK(ee_mem_read32(s,r+12)==1);
 REQUEST(0x8000010bu,0,0,0,0);CHECK(ee_mem_read32(s,r+12)==2);
 REQUEST(0x8000010cu,0,0,0,0);CHECK(ee_mem_read32(s,r+12)==1);
 REQUEST(0x80000100u,1,0,0,a);CHECK(ee_mem_read8(s,a+4)==0);CHECK(ee_mem_read32(s,a+40)==0);
 ee_pad_area_refresh_all(s);CHECK(ee_mem_read8(s,a+4)==0);CHECK(ee_mem_read32(s,a+40)==0);
 REQUEST(0x8000010eu,0,0,0,0);CHECK(!g_ee_pad_area_bound[0]&&!g_ee_pad_area_bound[1]);
 printf("Legacy PAD RPC: %u failures; typed results, safe DMA, modes, buttons, frame refresh, close and absent port\n",fail);return fail?1:0;
}
