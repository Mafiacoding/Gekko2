#include <assert.h>
#include <stdio.h>
#include "core/ee/ee_core.c"
int main(void)
{
 bios_image_t bios={0};unsigned char rom[16]={0};bios.data=rom;bios.size=sizeof(rom);
 for(unsigned n=0;n<3;n++) {
  assert(ee_core_init(&bios)==0);ee_state_t *st=ee_core_get_state();
  assert(!g_boot_once.selfloop&&!g_boot_once.fastboot&&!g_boot_once.escalation);
  assert(!g_boot_once.carousel_next&&!g_boot_once.carousel_sequence);
  assert(!ee_sif_sysreg[0]&&!ee_sif_sysreg[1]&&!ee_sif_sysreg[2]);
  st->pc=EE_EELOAD_START_PC;ee_check_eeload_fastboot_patch(st);assert(g_boot_once.fastboot==1);
  g_boot_once.selfloop=g_boot_once.escalation=1;
  g_boot_once.carousel_next=999;g_boot_once.carousel_sequence=7;
  ee_sif_sysreg[0]=0xdeadbeef;ee_sif_sysreg[1]=0x1234;ee_sif_sysreg[2]=1;
  ee_core_shutdown();
 }
 puts("PASS three same-process cold boots reset boot one-shots and SIF system registers");
}
