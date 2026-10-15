#include <stdio.h>
#include <stdint.h>
#include "core/hw/vif.h"
#include "core/hw/dma.h"
static int failures;
#define CHECK(c,m) do {if(!(c)){puts(m);failures++;}}while(0)
static void wle32(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t enc_vifcode(uint32_t c,uint32_t n,uint32_t i){return c<<24|n<<16|i;}
#define VIF_CMD_STCYCL 1
#define VIF_CMD_UNPACK_S_32 0x60
int main(void){
    { /* Fill lookahead must not consume the following MARK command. */
        vif_init();
        uint8_t buf[16];
        wle32(buf, enc_vifcode(VIF_CMD_STCYCL,0,0x0401));
        wle32(buf+4, enc_vifcode(VIF_CMD_UNPACK_S_32,4,0));
        wle32(buf+8,0x12345678);
        wle32(buf+12,enc_vifcode(0x07,0,0x9876));
        vif1_process_quadwords(DMA_CHANNEL_VIF1,buf,1);
        CHECK(vif1_get_state()->mark==0x9876, "Fill keeps following MARK command");
        CHECK(!vif1_get_state()->unpack_pending, "Fill completes with one consumed source vector");
        uint32_t value=0;
        CHECK(vif_mmio_read32(0x10003c30,&value)&&value==0x9876,"MARK MMIO observes parsed command");
        CHECK(vif_mmio_read32(0x10003c40,&value)&&value==0x0401,"CYCLE MMIO observes CL/WL");
        CHECK(!vif_mmio_read32(0x10003c34,&value),"VIF register padding is not a register alias");
        vif1_get_state()->row[3]=0xabcdef01;
        CHECK(vif_mmio_read32(0x10003d30,&value)&&value==0xabcdef01,"ROW3 MMIO reflects register state");
    }
 if(!failures)puts("PASS VIF fill command boundary and register MMIO");
 return failures?1:0;
}
