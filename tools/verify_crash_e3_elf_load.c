/* Optional, private Crash E3 ELF-load regression. No disc bytes in source.
 * Link with the host core (tools/build_host_core.py) and set
 * GEKKO2_CRASH_E3_BIN=/private/path/to/your/raw.bin.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/ee/ee_core.c"
#include "core/system.h"
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void){
 const char *path=getenv("GEKKO2_CRASH_E3_BIN");
 if(!path||!*path)return puts("SKIP: set GEKKO2_CRASH_E3_BIN for private image"),0;
 bios_image_t bios={0};
 bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;
 if(!bios.data||system_init(&bios,&bios))return puts("FAIL host core initialization"),1;
 if(iop_cdvd_mount_iso(path))return puts("FAIL mount image"),1;
 iop_cdvd_set_disc_present(IOP_CDVD_TYPE_PS2CD);
 uint32_t epc=0,gp=0;
 if(!sif_loadfile_elf_load_disc(ee_core_get_state(),"cdrom0:\\SCEU_000.01;1",&epc,&gp))
  return puts("FAIL LOADFILE disc ELF path"),1;
 if(epc!=0x00100008u||gp!=0u)return puts("FAIL LOADFILE real entry"),1;
 uint32_t lba=0,size=0;uint8_t head[2048];
 if(!iop_cdvd_disc_find_file("SCEU_000.01;1",&lba,&size)||iop_cdvd_disc_read_sector(lba,head))return 1;
 uint32_t phoff=rd32(head+28u);uint32_t phsize=rd32(head+phoff+16u);
 uint32_t vaddr=rd32(head+phoff+8u);uint32_t off=rd32(head+phoff+4u);
 uint32_t memsz=rd32(head+phoff+20u);
 ee_state_t *st=ee_core_get_state();uint32_t base=sif_loadfile_translate_base(st,vaddr);
 if(!phsize||phsize>memsz||base+memsz>st->ram_size)return puts("FAIL unexpected private ELF layout"),1;
 /* Full byte-for-byte readback, not just a signature check. */
 for(uint32_t k=0;k<phsize;){
  uint32_t a=off+k, n=2048u-(a%2048u);
  if(n>phsize-k)n=phsize-k;
  uint8_t buf[2048];
  if(iop_cdvd_disc_read_sector(lba+a/2048u,buf))return puts("FAIL sector read"),1;
  if(memcmp(st->ram+base+k,buf+(a%2048u),n))
   return printf("FAIL ELF mismatch at file byte %u\n",k),1;
  k+=n;
 }
 for(uint32_t k=phsize;k<memsz;k++)
  if(st->ram[base+k])return printf("FAIL BSS byte at %u\n",k),1;
 printf("PASS private Crash E3 LOADFILE ELF=%u B, loaded=%u B, BSS=%u B, entry=%08x; all loaded bytes and BSS verified\n",size,phsize,memsz-phsize,epc);
 iop_cdvd_unmount_iso();free(bios.data);return 0;
}
