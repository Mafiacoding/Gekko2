#include "core/checkpoint.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/iop_spu2.h"
#include "core/hw/spu2_mixer.h"
#include "core/hw/audio_output.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void wr(int c,unsigned r,unsigned v){assert(iop_spu2_mmio_write16(iop_spu2_core_reg_addr(c,r),v));}
int main(void){
 static uint8_t rom[4*1024*1024];bios_image_t b={0};b.data=rom;b.size=sizeof rom;assert(!system_init(&b,&b));
 for(unsigned c=0;c<2;c++){
  assert(iop_spu2_mmio_write32(iop_spu2_core_reg_addr(c,SPU2_C_VMIXL),0xffffff));
  assert(iop_spu2_mmio_write32(iop_spu2_core_reg_addr(c,SPU2_C_VMIXR),0xffffff));wr(c,SPU2_C_MMIX,0xc0c);
  assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_MVOLL+c*0x28,0x3fff));
  assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_MVOLR+c*0x28,0x3fff));
 }
 assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_EXTVOLL+0x28,0x7fff));assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_EXTVOLR+0x28,0x7fff));
 uint8_t *ram=spu2_mixer_get_ram();ram[0x18000]=0;ram[0x18001]=7;memset(ram+0x18002,0x77,14);
 wr(0,SPU2_VADDR_BASE+SPU2_VA_SSA_LO,0xc000);wr(0,SPU2_V_VOLL,0x3fff);wr(0,SPU2_V_VOLR,0x3fff);wr(0,SPU2_V_PITCH,0x1000);wr(0,SPU2_C_KON0,1);
 iop_core_get_state()->sched_ticks=1536;iop_spu2_sync(1536);assert(spu2_mixer_get_total_frames_rendered()==2);
 char path[]="/tmp/gekko2-sound-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);assert(!checkpoint_save(path));
 int16_t expected[64*2],actual[64*2];spu2_mixer_render_frames(expected,64);assert(expected[0]>10000);
 iop_spu2_init();assert(!checkpoint_load(path,&b,&b,NULL));assert(ram[0x18001]==7&&ram[0x18002]==0x77);
 assert(spu2_mixer_get_total_frames_rendered()==2&&spu2_mixer_get_total_kon_count()==1);spu2_mixer_render_frames(actual,64);assert(!memcmp(expected,actual,sizeof actual));
 audio_output_info ai;audio_output_get_info(&ai);assert(!ai.queued&&!ai.running);
 /* Corruption must reject before live sound changes. */
 FILE *f=fopen(path,"r+b");assert(f);fseek(f,8,SEEK_SET);char tag[4];uint32_t n;long pos=-1;
 while(fread(tag,1,4,f)==4&&fread(&n,4,1,f)==1){if(!memcmp(tag,"SPUS",4)){pos=ftell(f)-8;break;}assert(!fseek(f,n,SEEK_CUR));}
 assert(pos>=0);uint32_t bad=99;fwrite(&bad,4,1,f);fclose(f);uint64_t frames=spu2_mixer_get_total_frames_rendered();
 assert(checkpoint_load(path,&b,&b,NULL)<0&&spu2_mixer_get_total_frames_rendered()==frames);
 /* A legacy file ending before SPUS/SPUR deliberately resets sound. */
 f=fopen(path,"r+b");assert(f);fseek(f,pos,SEEK_SET);fwrite("END0",1,4,f);n=0;fwrite(&n,4,1,f);assert(!fflush(f));assert(!ftruncate(fileno(f),pos+8));fclose(f);
 assert(!checkpoint_load(path,&b,&b,NULL));assert(!spu2_mixer_get_total_kon_count()&&!ram[0x18001]);unlink(path);
 puts("PASS SPU2 active-voice/RAM checkpoint, exact next PCM, corrupt-state rejection and legacy cold defaults");return 0;
}
