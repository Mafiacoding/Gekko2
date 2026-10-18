#include <assert.h>
#include <stdio.h>
#include "hw/spu2_mixer.c"
static unsigned cases;
int main(void)
{
 iop_spu2_init();
 const uint16_t levels[]={0,1,0x4000,0x7fff,0x8000,0xffff};
 for(unsigned core=0;core<2;core++)for(unsigned voice=0;voice<24;voice++)for(unsigned i=0;i<6;i++){
  voice_runtime_t *v=&g_voice[core][voice];v->phase=ENV_SUSTAIN;v->active=1;v->adsr_counter=1234;
  assert(iop_spu2_mmio_write16(iop_spu2_voice_reg_addr(core,voice,SPU2_V_ENVX),levels[i]));
  assert(v->level==levels[i]&&v->phase==ENV_SUSTAIN&&v->active&&v->adsr_counter==1234);
  mixer_snapshot snapshot;spu2_mixer_snapshot_save(&snapshot);assert(spu2_mixer_snapshot_valid(&snapshot));
  v->level=0;spu2_mixer_snapshot_load(&snapshot);assert(v->level==levels[i]);cases++;
 }
 /* Gain reaches capture RAM without changing KON or frozen ADSR phase. */
 iop_spu2_init();voice_runtime_t *v=&g_voice[0][1];v->active=1;v->phase=ENV_SUSTAIN;v->block_valid=1;
 for(unsigned i=0;i<28;i++)v->block[i]=16384;
 iop_spu2_mmio_write16(iop_spu2_voice_reg_addr(0,1,SPU2_V_ADSR2),0x1fc0);
 for(unsigned i=0;i<6;i++){
  iop_spu2_mmio_write16(iop_spu2_voice_reg_addr(0,1,SPU2_V_ENVX),levels[i]);
  int16_t out[2];unsigned pos=g_total_frames&511u;spu2_mixer_render_frames(out,1);
  unsigned at=(0x400+pos)*2;int16_t captured=(uint16_t)g_spu2_ram[at]|((uint16_t)g_spu2_ram[at+1]<<8);
  assert(captured==(int32_t)levels[i]/2&&v->level==levels[i]&&!g_total_kon);cases++;
 }
 for(int previous=-65535;previous<=65535;previous+=4369)for(unsigned pitch=0;pitch<=65535;pitch+=257){
  int64_t expected=(int64_t)pitch*(32768+previous);
  if(expected<0)expected=0;expected>>=15;if(expected>0x3fff)expected=0x3fff;
  assert(modulated_pitch((uint16_t)pitch,previous,1)==(unsigned)expected);cases++;
 }
 v->active=0;v->phase=ENV_OFF;
 iop_spu2_mmio_write16(iop_spu2_voice_reg_addr(0,1,SPU2_V_ENVX),1234);
 assert(!v->active&&v->phase==ENV_OFF&&v->level==1234);
 printf("PASS SPU2 writable ENVX, phase/counter and checkpoint preservation, actual capture gain and wide PMON: %u cases\n",cases);return 0;
}
