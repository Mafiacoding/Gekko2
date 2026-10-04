/* A real checkpoint round trip must preserve PADMAN DMA destinations. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/checkpoint.h"
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
    FILE *clock_file=fopen(path,"r+b");if(!clock_file)return 2;
    fseek(clock_file,-16,SEEK_END);unsigned char bytes[8],expected[8]={0xf0,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12};
    CHECK(fread(bytes,1,8,clock_file)==8 && !memcmp(bytes,expected,8),"ECLK stores fixed little-endian bytes");
    fseek(clock_file,-20,SEEK_END);uint32_t bad_clock_size=7;fwrite(&bad_clock_size,4,1,clock_file);fclose(clock_file);
    CHECK(checkpoint_load(path,&bios,&bios,NULL)<0,"reject malformed clock block size");
    CHECK(ee_core_display_clock_save()==0x123456789abcdef0ull,"rejected clock block leaves live phase intact");
    clock_file=fopen(path,"r+b");if(!clock_file)return 2;
    fseek(clock_file,-20,SEEK_END);bad_clock_size=8;fwrite(&bad_clock_size,4,1,clock_file);
    fseek(clock_file,-8,SEEK_END);long end_offset=ftell(clock_file);uint32_t zero=0;
    fwrite("ECLK",1,4,clock_file);fwrite(&bad_clock_size,4,1,clock_file);fwrite(bytes,1,8,clock_file);
    fwrite("END0",1,4,clock_file);fwrite(&zero,4,1,clock_file);fclose(clock_file);
    CHECK(checkpoint_load(path,&bios,&bios,NULL)<0,"reject duplicate clock block");
    clock_file=fopen(path,"r+b");if(!clock_file)return 2;fseek(clock_file,end_offset,SEEK_SET);
    fwrite("END0",1,4,clock_file);fwrite(&zero,4,1,clock_file);fflush(clock_file);ftruncate(fileno(clock_file),end_offset+8);fclose(clock_file);
    st=ee_core_get_state();
    iop_sio2_pad_connect();iop_sio2_pad_set_buttons(IOP_PAD_BTN_CROSS);
    ee_pad_area_refresh_all(st);
    CHECK(ee_mem_read16(st,original.new_area[0][0]+2)==(uint16_t)~IOP_PAD_BTN_CROSS,"new input reaches restored XPADMAN area");
    CHECK(ee_mem_read8(st,original.legacy_area[0]+11)==((uint16_t)~IOP_PAD_BTN_CROSS&255u),"new input reaches restored legacy area");
    CHECK(ee_mem_read32(st,original.new_status+4)==1,"connection reaches restored status area");
    FILE *f=fopen(path,"r+b");if(!f)return 2;
    fseek(f,-(long)(sizeof(original)+16u+8u+sizeof(ee_irq_checkpoint_t)+16u),SEEK_END);long tail=ftell(f);
    /* A malformed optional binding block must be rejected. */
    fseek(f,tail+4,SEEK_SET);uint32_t wrong=sizeof(original)-4u;fwrite(&wrong,4,1,f);fclose(f);
    CHECK(checkpoint_load(path,&bios,&bios,NULL)<0,"reject malformed binding block size");
    ee_core_pad_checkpoint_save(&got);
    CHECK(memcmp(&got,&original,sizeof(got))==0,"failed load leaves PAD bindings intact");
    /* Remove optional blocks to reproduce the original version-1 format. */
    f=fopen(path,"r+b");if(!f)return 2;
    fseek(f,tail,SEEK_SET);fwrite("END0",1,4,f);wrong=0;fwrite(&wrong,4,1,f);fflush(f);ftruncate(fileno(f),tail+8);fclose(f);
    CHECK(checkpoint_load(path,&bios,&bios,NULL)==0,"old snapshots remain readable");
    CHECK(ee_core_display_clock_save()==0,"legacy snapshots do not inherit unrelated display phase");
    ee_core_pad_checkpoint_save(&got);
    CHECK(memcmp(&got,&empty,sizeof(got))==0,"old snapshots do not inherit unrelated live bindings");
    ee_core_irq_checkpoint_save(&irq_got);
    CHECK(memcmp(&irq_got,&irq_empty,sizeof(irq_got))==0,"legacy snapshots reset missing IRQ state");
    unlink(path);printf("PAD checkpoint: %u failures\n",failures);return failures?1:0;
}
