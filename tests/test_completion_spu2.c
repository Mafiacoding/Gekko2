/* Synthetic controls and reference waveform; no BIOS or game data.
 * Noise reference truth table: PCSX2 Mixer.cpp 091ab68708a555198d80a7216965481c7ddd0d98,
 * GPL-3.0-or-later. Tests do not establish physical SPU2 timing accuracy. */
#include <stdio.h>
#include <stdlib.h>
#include "hw/spu2_mixer.c"
#include "hw/iop_spu2.c"
static unsigned checks,failures;
#define CHECK(x) do {checks++;if(!(x)){if(failures<10)printf("FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)
static uint16_t readreg(uint32_t a){uint16_t v=0xbeef;CHECK(iop_spu2_mmio_read16(a,&v));return v;}
static void controls(void)
{
 for(unsigned c=0;c<2;c++)for(unsigned v=0;v<24;v++)for(unsigned side=0;side<2;side++){
  uint32_t a=iop_spu2_voice_reg_addr(c,v,side*2),cur=a+12;
  const uint16_t values[]={0,1,0x3fff,0x4000,0x7fff};
  for(unsigned i=0;i<5;i++){
   iop_spu2_mmio_write16(a,values[i]);CHECK(readreg(cur)==(uint16_t)(values[i]*2u));
   iop_spu2_mmio_write16(cur,0x5678);CHECK(readreg(cur)==(uint16_t)(values[i]*2u));
  }
 }
 for(unsigned c=0;c<2;c++)for(unsigned side=0;side<2;side++){
  uint32_t a=IOP_SPU2_BASE+0x760+c*0x28+side*2;
  iop_spu2_mmio_write16(a,0x4321);CHECK(readreg(a+16)==0x8642);
  iop_spu2_mmio_write16(a+16,1234);CHECK(readreg(a+16)==0x8642);
 }
}
static void noise(void)
{
 static const unsigned char lut[64]={
  1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,
  0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1};
 const unsigned fractions[]={0,84,140,180};
 for(unsigned clock=0;clock<64;clock++){
  uint32_t counter=0;uint16_t level=0;g_noise[0]=(noise_state){0};
  for(unsigned f=0;f<65536;f++){
   counter+=65536;counter+=fractions[clock%4];
   if((counter%65536)>=210){counter+=65536;counter-=fractions[clock%4];}
   uint32_t period=(32768u>>(clock/4))*65536u;
   if(counter>=period){do{counter-=period;}while(counter>=period);level=(uint16_t)((level*2u)|lut[(level>>10)&63]);}
   noise_tick(0,clock);CHECK(g_noise[0].counter==counter&&g_noise[0].lfsr==level);
  }
 }
}
static void pitch(void)
{
 const int previous[]={-32768,-32767,-1,0,1,32766,32767};
 const uint16_t pitches[]={0,1,0x1000,0x3fff,0x4000,0xffff};
 for(unsigned a=0;a<7;a++)for(unsigned b=0;b<6;b++)for(int enabled=0;enabled<2;enabled++){
  uint64_t expected=pitches[b];if(enabled)expected=expected*(32768+previous[a])/32768;
  if(expected>16383)expected=16383;
  CHECK(modulated_pitch(pitches[b],previous[a],enabled)==expected);
 }
}
/* Hand-computable sweep boundaries, counter rates and phase directions. */
static void sweeps(void)
{
 uint32_t cur=IOP_SPU2_BASE+12;
 for(unsigned phase=0;phase<2;phase++)for(unsigned dec=0;dec<2;dec++)for(unsigned step=0;step<4;step++){
  unsigned setting=0x8000u|(phase<<12)|(dec<<13)|44u|step;
  int start=phase?-16000:16000;
  iop_spu2_set_raw16(cur,(uint16_t)start);g_volume_counter[0][0][0]=0;
  int delta=dec?-(8-(int)step):7-(int)step;if(phase)delta=-delta;
  CHECK(volume_tick(0,0,0,setting,cur)==start+delta);
  CHECK(readreg(cur)==(uint16_t)(start+delta));
 }
 /* Shift12: one linear step every second sample. */
 iop_spu2_set_raw16(cur,1000);g_volume_counter[0][0][0]=0;
 CHECK(volume_tick(0,0,0,0x8030,cur)==1000);
 CHECK(volume_tick(0,0,0,0x8030,cur)==1007);
 /* Slowest non-infinite rate must eventually progress, all-one rate freezes. */
 iop_spu2_set_raw16(cur,1000);g_volume_counter[0][0][0]=0;
 for(unsigned f=0;f<32768;f++)volume_tick(0,0,0,0x807e,cur);
 CHECK(readreg(cur)==1005);
 for(unsigned f=0;f<65536;f++)volume_tick(0,0,0,0x807f,cur);
 CHECK(readreg(cur)==1005);
 /* Exponential increase above 0x6000 takes four samples; decrease scales. */
 iop_spu2_set_raw16(cur,0x7000);g_volume_counter[0][0][0]=0;
 for(int i=0;i<3;i++)CHECK(volume_tick(0,0,0,0xc02c,cur)==0x7000);
 CHECK(volume_tick(0,0,0,0xc02c,cur)==0x7007);
 iop_spu2_set_raw16(cur,0x4000);CHECK(volume_tick(0,0,0,0xe02c,cur)==0x3ffc);
 /* Mode changes preserve accumulated phase, fixed writes publish immediately. */
 g_volume_counter[0][0][0]=123;iop_spu2_mmio_write16(IOP_SPU2_BASE,0x8000);
 CHECK(g_volume_counter[0][0][0]==123);
 iop_spu2_mmio_write16(IOP_SPU2_BASE,0x1234);CHECK(readreg(cur)==0x2468);
 /* Inactive voice/master sweeps must still run. */
 iop_spu2_init();iop_spu2_mmio_write16(IOP_SPU2_BASE,0x802c);
 iop_spu2_mmio_write16(IOP_SPU2_BASE+0x760,0x802c);
 int16_t out[2];spu2_mixer_render_frames(out,1);
 CHECK(readreg(cur)==7);CHECK(readreg(IOP_SPU2_BASE+0x770)==7);
}
/* Exercise NON/PMON through the complete render loop, including capture RAM.
 * Seed decoded blocks/envelopes so source and post-ADSR values are exact. */
static void render_controls(void)
{
 iop_spu2_init();
 for(unsigned c=0;c<2;c++)for(unsigned v=0;v<4;v++){
  voice_runtime_t *r=&g_voice[c][v];r->active=1;r->phase=ENV_SUSTAIN;
  r->level=16384;r->block_valid=1;
  for(unsigned i=0;i<28;i++)r->block[i]=16384;
  iop_spu2_mmio_write16(iop_spu2_voice_reg_addr(c,v,SPU2_V_ADSR2),0x1fc0);
  iop_spu2_mmio_write16(iop_spu2_voice_reg_addr(c,v,SPU2_V_PITCH),0x1000);
 }
 g_noise[0].lfsr=12000;g_noise[1].lfsr=8000;
 /* Voice0 ignores PMON, voice1 uses previous post-ADSR sample, voice3
  * uses voice2. NON is selected per voice, shared within each core. */
 for(unsigned c=0;c<2;c++){
  iop_spu2_mmio_write16(iop_spu2_core_reg_addr(c,SPU2_C_NON),0x000a);
  iop_spu2_mmio_write16(iop_spu2_core_reg_addr(c,SPU2_C_FMOD1),0x000b);
 }
 int16_t out[2];spu2_mixer_render_frames(out,1);
 for(unsigned c=0;c<2;c++){
  CHECK(g_voice[c][0].pitch_counter==0);CHECK(g_voice[c][0].block_pos==1);
  CHECK(g_voice[c][1].pitch_counter==1024);CHECK(g_voice[c][1].block_pos==1);
  CHECK(g_voice[c][3].pitch_counter==1024);CHECK(g_voice[c][3].block_pos==1);
  unsigned base=c?0xc00:0x400;
  int expected=c?4000:6000;
  for(unsigned offset=0;offset<=0x200;offset+=0x200){
   unsigned a=(base+offset)*2;int16_t value=(uint16_t)g_spu2_ram[a]|((uint16_t)g_spu2_ram[a+1]<<8);
   CHECK(value==expected);
  }
 }
}
static void snapshots(void)
{
 mixer_snapshot a,b;spu2_mixer_snapshot_save(&a);CHECK(spu2_mixer_snapshot_valid(&a));
 for(unsigned i=0;i<127;i++)noise_tick(0,63);
 g_volume_counter[1][23][1]=4567;spu2_mixer_snapshot_save(&a);
 noise_tick(0,63);noise_state expected=g_noise[0];
 spu2_mixer_snapshot_load(&a);noise_tick(0,63);CHECK(!memcmp(&expected,&g_noise[0],sizeof expected));
 CHECK(g_volume_counter[1][23][1]==4567);
 b=a;b.extension=0;CHECK(!spu2_mixer_snapshot_valid(&b));
 b=a;b.volume_counter[0][0][0]=0x8000;CHECK(!spu2_mixer_snapshot_valid(&b));
 b=a;b.noise[1].reserved=1;CHECK(!spu2_mixer_snapshot_valid(&b));
 CHECK(!spu2_mixer_snapshot_valid_sized(&a,sizeof a-1));
 mixer_snapshot_legacy old;memcpy(&old,&a,sizeof old);
 CHECK(spu2_mixer_snapshot_valid_sized(&old,sizeof old));
 spu2_mixer_snapshot_load_sized(&old,sizeof old);
 CHECK(!g_noise[0].lfsr&&!g_noise[0].counter&&!g_volume_counter[1][23][1]);
 spu2_mixer_snapshot_save(&b);CHECK(!memcmp(&old,&b,sizeof old));
}
static void outer_snapshots(void)
{
 iop_spu2_init();
 g_noise[1]=(noise_state){12345,23456,0};g_volume_counter[1][23][1]=6789;
 iop_spu2_mmio_write16(IOP_SPU2_BASE+0x400,0x1234);
 spu_snapshot *saved=malloc(sizeof *saved),*old=malloc(sizeof *old);
 CHECK(saved&&old);if(!saved||!old)exit(1);
 iop_spu2_snapshot_save(saved);
 CHECK(iop_spu2_snapshot_valid(saved,UINT64_MAX,2u*1024u*1024u));
 iop_spu2_init();iop_spu2_snapshot_load(saved);
 CHECK(g_noise[1].counter==12345&&g_noise[1].lfsr==23456);
 CHECK(g_volume_counter[1][23][1]==6789);
 CHECK(readreg(IOP_SPU2_BASE+0x400+12)==0x2468);
 memcpy(old,saved,sizeof *old);old->mixer_size=sizeof(mixer_snapshot_legacy);
 CHECK(iop_spu2_snapshot_valid(old,UINT64_MAX,2u*1024u*1024u));
 iop_spu2_snapshot_load(old);
 CHECK(!g_noise[1].counter&&!g_noise[1].lfsr&&!g_volume_counter[1][23][1]);
 CHECK(readreg(IOP_SPU2_BASE+0x400+12)==0x2468);
 old->mixer_size--;CHECK(!iop_spu2_snapshot_valid(old,UINT64_MAX,2u*1024u*1024u));
 memcpy(old,saved,sizeof *old);
 ((mixer_snapshot *)old->mixer)->volume_counter[0][0][0]=0x8000;
 CHECK(!iop_spu2_snapshot_valid(old,UINT64_MAX,2u*1024u*1024u));
 free(saved);free(old);
}
int main(void)
{
 iop_spu2_init();controls();noise();pitch();sweeps();render_controls();snapshots();outer_snapshots();
 printf("SPU2 datapath checks=%u failures=%u\n",checks,failures);return failures!=0;
}
