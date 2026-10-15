/* A real checkpoint round trip must preserve PADMAN DMA destinations. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/checkpoint.h"
#include "core/system.h"
#include "core/ee/ee_core.c"
static unsigned failures;
#define CHECK(x,m) do { if (!(x)) { printf("FAIL: %s\n",m); failures++; } } while (0)
int main(void)
{
    bios_image_t bios = {0};
    char path[] = "/tmp/pcsx2-pad-XXXXXX";
    int fd = mkstemp(path); if (fd < 0) return 2; close(fd);
    if (system_init(&bios,&bios)) return 2;
    ee_state_t *st = ee_core_get_state();
    ee_pad_checkpoint_t original = {{0x80101000u,0},{{0x80102000u,0,0,0},{0}},0x80103000u};
    ee_pad_checkpoint_t empty = {{0},{{0}},0}, got;
    ee_irq_checkpoint_t irq_original={3,1},irq_empty={0,0},irq_got;
    ee_core_display_clock_load(0x123456789abcdef0ull);
    ee_core_irq_checkpoint_load(&irq_original);
    ee_core_pad_checkpoint_load(&original);
    CHECK(checkpoint_save(path)==0,"save real state including PAD bindings");
    ee_core_display_clock_load(17);
    ee_core_irq_checkpoint_load(&irq_empty);
    ee_core_pad_checkpoint_load(&empty);
    CHECK(checkpoint_load(path,&bios,&bios,NULL)==0,"load new checkpoint");
    CHECK(ee_core_display_clock_save()==0x123456789abcdef0ull,"independent 64-bit clock survives checkpoint");
    ee_core_pad_checkpoint_save(&got);
    CHECK(memcmp(&got,&original,sizeof(got))==0,"all legacy and XPADMAN bindings survive");
    ee_core_irq_checkpoint_save(&irq_got);
    CHECK(memcmp(&irq_got,&irq_original,sizeof(irq_got))==0,"pending IRQ countdown survives real checkpoint");
    st=ee_core_get_state();
    iop_sio2_pad_connect();iop_sio2_pad_set_buttons(IOP_PAD_BTN_CROSS);
    ee_pad_area_refresh_all(st);
    CHECK(ee_mem_read16(st,original.new_area[0][0]+2)==(uint16_t)~IOP_PAD_BTN_CROSS,"XPAD input after restore");
    CHECK(ee_mem_read16(st,original.legacy_area[0]+10)==(uint16_t)~IOP_PAD_BTN_CROSS,"normalized legacy input after restore");
    CHECK(ee_mem_read32(st,original.new_status+4)==1,"connection after restore");
    unlink(path);printf("R1356 PAD checkpoint: %u failures\n",failures);return failures?1:0;
}
