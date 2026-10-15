/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPU2 ADPCM beta. PS2 addresses use 16-bit words (PCSX2 SPU2/Mixer.cpp,
 * Dma.cpp, RegTable.cpp), not PS1 eight-byte address units.
 * 48 kHz, fixed volume, nearest-sample pitch; reverb/noise/PMON/sweep
 * and Gaussian interpolation remain outside this beta. */
#include "core/hw/spu2_mixer.h"
#include "core/hw/iop_spu2.h"
#include <string.h>

typedef enum {
    ENV_OFF = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} env_phase_t;

typedef struct {
    int active;
    env_phase_t phase;
    int32_t level;             /* current ADSR volume, 0..0x7FFF (real range) */
    uint32_t adsr_counter;     /* real "AdsrCycles"-equivalent accumulator */

    uint32_t cur_addr;         /* byte offset into spu2_ram of the ADPCM
                                 * block currently loaded in block[] */
    uint32_t repeat_addr;      /* real per-voice loop/repeat byte address */
    int16_t hist1;             /* real "old" ADPCM decode history */
    int16_t hist2;             /* real "older" ADPCM decode history */
    int16_t block[28];         /* decoded samples of the current block */
    int block_valid;
    int block_pos;              /* 0..27; >=28 means "needs next block" */
    int manual_loop;
    int pending_loop_end;
    int pending_loop_repeat;
    uint32_t pitch_counter;     /* real pitch Counter; bit12+ = sample advance */
} voice_runtime_t;

static uint8_t g_spu2_ram[SPU2_MIXER_RAM_SIZE];
static voice_runtime_t g_voice[SPU2_MIXER_NUM_CORES][SPU2_MIXER_VOICES_PER_CORE];
static uint64_t g_total_kon = 0;
static uint64_t g_total_frames = 0;

/* ---- register-file glue: read live values via iop_spu2.c's own real
 * addressing API, no duplicated register storage in this file. ---- */

static uint16_t read_voice_reg16(int core, int voice, uint32_t off)
{
    uint16_t v = 0;
    iop_spu2_mmio_read16(iop_spu2_voice_reg_addr(core, voice, off), &v);
    return v;
}

static uint16_t read_vaddr_reg16(int core, int voice, uint32_t off)
{
    uint16_t v = 0;
    iop_spu2_mmio_read16(iop_spu2_voice_addr_reg_addr(core, voice, off), &v);
    return v;
}

static uint16_t read_core_reg16(int core, uint32_t off)
{
    uint16_t v = 0;
    iop_spu2_mmio_read16(iop_spu2_core_reg_addr(core, off), &v);
    return v;
}

static uint16_t read_output_reg16(int core,uint32_t off)
{
    /* Core master/output parameters are 0x28 bytes apart, not 0x400. */
    uint16_t v = 0;
    iop_spu2_mmio_read16(IOP_SPU2_BASE + off + (core?0x28u:0u), &v);
    return v;
}

static void spu2_set_endx_bit(int core, int voice)
{
    uint32_t off = (voice < 16) ? SPU2_C_ENDX0 : SPU2_C_ENDX1;
    int bit = (voice < 16) ? voice : (voice - 16);
    uint16_t cur = read_core_reg16(core, off);
    iop_spu2_set_raw16(iop_spu2_core_reg_addr(core,off), (uint16_t)(cur | (1u << bit)));
}

/* ---- real ADPCM decode, cited formula/predictor table (psx-spx
 * CDROM Format page's decode_28_nibbles, explicitly stated to be the
 * same algorithm SPU-ADPCM uses) - see spu2_mixer.h for the full
 * citation. ---- */

static void decode_adpcm_block(const uint8_t *blk16, int16_t *out28,
                                int16_t *hist1, int16_t *hist2,
                                uint8_t *flags_out)
{
    static const int32_t pos_tbl[5] = { 0, 60, 115, 98, 122 };
    static const int32_t neg_tbl[5] = { 0, 0, -52, -55, -60 };

    unsigned shift_field=blk16[0]&15u;
    if(shift_field>12u)shift_field=9u;
    int shift=12-(int)shift_field;
    unsigned filter=(blk16[0]>>4)&15u;
    if(filter>4u)filter=0u; /* reserved predictor: bounded silent predictor */

    int32_t f0 = pos_tbl[filter];
    int32_t f1 = neg_tbl[filter];

    int16_t old = *hist1;
    int16_t older = *hist2;
    for (int i = 0; i < 28; i++) {
        uint8_t byte = blk16[2 + (i >> 1)];
        uint8_t raw_nibble = (i & 1) ? (uint8_t)(byte >> 4) : (uint8_t)(byte & 0x0F);
        int32_t t = ((int32_t)raw_nibble ^ 8) - 8; /* sign-extend 4-bit */
        int32_t s = t * (1 << shift) + (((int32_t)old * f0 + (int32_t)older * f1 + 32) >> 6);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out28[i] = (int16_t)s;
        older = old;
        old = (int16_t)s;
    }
    *hist1 = old;
    *hist2 = older;
    if (flags_out) *flags_out = blk16[1];
}

/* ---- real ADSR envelope stepping, faithfully transcribed from
 * psx-spx's "Envelope Operation depending on Shift/Step/Mode/
 * Direction" pseudocode. all_ones models the real "Step field all-
 * ones => never steps, never saturates" quirk (psx-spx: "Using a step
 * value of all-ones causes the volume to never step... i.e. 0x7f, or
 * 0x1f for decay/release"); phase_negative is always 0 in this engine
 * (see spu2_mixer.h's documented simplification). ---- */

static int32_t envelope_step(int mode, int decreasing, int shift_value,
                              int step_value, int32_t level,
                              uint32_t *counter, int all_ones)
{
    int32_t adsr_step = 7 - step_value;
    if (decreasing) {
        adsr_step = ~adsr_step; /* +7,+6,+5,+4 => -8,-7,-6,-5 (real, cited) */
    }
    {
        int shl = 11 - shift_value;
        if (shl < 0) shl = 0;
        adsr_step = adsr_step * (1 << shl); /* avoid UB of signed <<, same result */
    }

    int32_t counter_increment;
    {
        int shr = shift_value - 11;
        if (shr < 0) shr = 0;
        counter_increment = (int32_t)(0x8000 >> shr);
    }

    if (mode == 1 && !decreasing && level > 0x6000) {
        if (shift_value < 10) {
            adsr_step /= 4;
        } else if (shift_value >= 11) {
            counter_increment /= 4;
        } else {
            adsr_step /= 2;
            counter_increment /= 2;
        }
    } else if (mode == 1 && decreasing) {
        adsr_step = (int32_t)((adsr_step * level) >> 15);
    }

    if (!all_ones && counter_increment < 1) counter_increment = 1;

    *counter += (uint32_t)counter_increment;
    if ((*counter & 0x8000u) == 0) {
        return level; /* no step this 44.1kHz tick */
    }
    *counter &= 0x7FFFu;

    level += adsr_step;
    if (!decreasing) {
        if (level > 0x7FFF) level = 0x7FFF;
        if (level < -0x8000) level = -0x8000;
    } else {
        /* PhaseNegative branch (real hardware CLAMP(-8000h..0h)) is
         * provably unreachable in this engine - see header note. */
        if (level < 0) level = 0;
    }
    return level;
}

static void voice_tick_envelope(voice_runtime_t *v, uint16_t adsr1, uint16_t adsr2)
{
    if (v->phase == ENV_OFF) return;

    int attack_mode  = (adsr1 >> 15) & 1;
    int attack_shift = (adsr1 >> 10) & 0x1F;
    int attack_step  = (adsr1 >> 8) & 0x3;
    int decay_shift  = (adsr1 >> 4) & 0xF;
    int sustain_level_field = adsr1 & 0xF;
    int32_t sustain_level = (int32_t)(sustain_level_field + 1) * 0x800; /* real, cited */

    int sustain_mode     = (adsr2 >> 15) & 1;
    int sustain_dir_dec  = (adsr2 >> 14) & 1;
    int sustain_shift    = (adsr2 >> 8) & 0x1F;
    int sustain_step     = (adsr2 >> 6) & 0x3;
    int release_mode     = (adsr2 >> 5) & 1;
    int release_shift    = adsr2 & 0x1F;

    switch (v->phase) {
    case ENV_ATTACK: {
        int all_ones = (attack_shift == 0x1F) && (attack_step == 3);
        v->level = envelope_step(attack_mode, 0, attack_shift, attack_step,
                                  v->level, &v->adsr_counter, all_ones);
        if (v->level >= 0x7FFF) {
            v->level = 0x7FFF;
            v->phase = ENV_DECAY;
            v->adsr_counter = 0;
        }
        break;
    }
    case ENV_DECAY: {
        /* Decay: real hardware fixes Mode=Exponential, Direction=Decrease,
         * Step=-8 (StepValue=0 fed into the shared formula, cited in
         * spu2_mixer.h, reduces to exactly that fixed step). */
        int all_ones = (decay_shift == 0xF); /* decay's real shift field is
                                               * only 4 bits (max 0xF) -
                                               * documented approximation of
                                               * psx-spx's generic "0x1f for
                                               * decay/release" all-ones note,
                                               * scaled to this field's real
                                               * width. */
        v->level = envelope_step(1, 1, decay_shift, 0, v->level, &v->adsr_counter, all_ones);
        if (v->level <= sustain_level) {
            v->level = (sustain_level < 0) ? 0 : sustain_level;
            v->phase = ENV_SUSTAIN;
            v->adsr_counter = 0;
        }
        break;
    }
    case ENV_SUSTAIN: {
        int all_ones = (sustain_shift == 0x1F) && (sustain_step == 3);
        v->level = envelope_step(sustain_mode, sustain_dir_dec, sustain_shift,
                                  sustain_step, v->level, &v->adsr_counter, all_ones);
        /* Real hardware: Sustain never auto-exits on level; only a real
         * Key OFF (spu2_mixer_on_koff()) drives it to Release. */
        break;
    }
    case ENV_RELEASE: {
        int all_ones = (release_shift == 0x1F);
        v->level = envelope_step(release_mode, 1, release_shift, 0,
                                  v->level, &v->adsr_counter, all_ones);
        if (v->level <= 0) {
            v->level = 0;
            v->phase = ENV_OFF;
            v->active = 0;
        }
        break;
    }
    default:
        break;
    }
}

/* ---- ADPCM playback: block fetch/decode + real loop-flag handling
 * (psx-spx "Possible combinations": Normal / End+Mute / End+Repeat). ---- */

static void voice_load_next_block(int core, int voice, voice_runtime_t *v)
{
    if (!v->active) return;
    if (v->cur_addr + 16 > SPU2_MIXER_RAM_SIZE) {
        v->active = 0;
        v->phase = ENV_OFF;
        return;
    }
    uint8_t flags = 0;
    decode_adpcm_block(&g_spu2_ram[v->cur_addr], v->block, &v->hist1, &v->hist2, &flags);
    v->block_pos = 0;
    v->block_valid = 1;
    if ((flags & 0x04) && !v->manual_loop) { /* LoopStart */
        v->repeat_addr = v->cur_addr;
    }
    v->pending_loop_end = (flags & 0x01) ? 1 : 0;
    v->pending_loop_repeat = (flags & 0x02) ? 1 : 0;
    iop_spu2_check_ram_irq(v->cur_addr/2u,8u);
    if((flags&4u)&&!v->manual_loop){iop_spu2_set_raw16(iop_spu2_voice_addr_reg_addr(core,voice,SPU2_VA_LSA_HI),(uint16_t)(v->repeat_addr>>17));
      iop_spu2_set_raw16(iop_spu2_voice_addr_reg_addr(core,voice,SPU2_VA_LSA_LO),(uint16_t)(v->repeat_addr/2u));}
}

static int16_t voice_produce_sample(int core, int voice, voice_runtime_t *v, uint16_t pitch_reg)
{
    if (!v->active) return 0;
    if (!v->block_valid || v->block_pos >= 28) {
        voice_load_next_block(core, voice, v);
        if (!v->active) return 0;
    }

    int16_t sample = v->block[v->block_pos];

    uint32_t step = pitch_reg;
    if (step > 0x4000) step = 0x4000; /* real, cited clamp */
    v->pitch_counter += step;

    while (v->pitch_counter >= 0x1000u && v->active) {
        v->pitch_counter -= 0x1000u;
        v->block_pos++;
        if (v->block_pos >= 28) {
            if (v->pending_loop_end) {
                spu2_set_endx_bit(core, voice); /* real: ENDX marks loop-end
                                                  * passed regardless of
                                                  * mute-vs-repeat outcome */
                if (v->pending_loop_repeat) {
                    v->cur_addr = v->repeat_addr;
                    voice_load_next_block(core, voice, v);
                } else {
                    v->active = 0;
                    v->phase = ENV_OFF;
                    v->level = 0;
                    break;
                }
            } else {
                v->cur_addr = (v->cur_addr+16u)&(SPU2_MIXER_RAM_SIZE-1u);
                voice_load_next_block(core, voice, v);
            }
        }
    }
    return sample;
}

/* ---- volume ---- */

static int32_t fixed_volume(uint16_t reg)
{
    /* Real Fixed Volume Mode (bit15=0): bits0-14 = "Voice volume/2".
     * Sweep Mode (bit15=1) is read as Fixed anyway - documented
     * simplification, see spu2_mixer.h. */
    int32_t raw = (int32_t)(reg & 0x7FFF);
    if (raw & 0x4000) raw -= 0x8000; /* sign-extend 15-bit field */
    return raw * 2;
}

/* ---- public API ---- */

void spu2_mixer_init(void)
{
    memset(g_spu2_ram, 0, sizeof(g_spu2_ram));
    memset(g_voice, 0, sizeof(g_voice));
    g_total_kon = 0;
    g_total_frames = 0;
}

uint8_t *spu2_mixer_get_ram(void)
{
    return g_spu2_ram;
}

void spu2_mixer_on_kon(int core, int is_upper_half, uint16_t bits)
{
    if (core < 0 || core >= SPU2_MIXER_NUM_CORES) return;
    int base_voice = is_upper_half ? 16 : 0;
    for (int b = 0; b < 16; b++) {
        if (!(bits & (1u << b))) continue;
        int voice = base_voice + b;
        if (voice >= SPU2_MIXER_VOICES_PER_CORE) continue;

        uint16_t ssa_hi = read_vaddr_reg16(core, voice, SPU2_VA_SSA_HI);
        uint16_t ssa_lo = read_vaddr_reg16(core, voice, SPU2_VA_SSA_LO);
        uint32_t ssa_units = ((uint32_t)ssa_hi << 16) | ssa_lo;
        uint32_t ssa_addr = (ssa_units & 0xffff8u) * 2u;
        if (SPU2_MIXER_RAM_SIZE > 0) {
            ssa_addr %= SPU2_MIXER_RAM_SIZE;
        }
        ssa_addr &= ~0xFu; /* block-align (16-byte ADPCM blocks) */

        voice_runtime_t *v = &g_voice[core][voice];
        memset(v, 0, sizeof(*v));
        v->active = 1;
        v->phase = ENV_ATTACK;
        v->level = 0; /* real: KON "automatically initializes ADSR Volume to zero" */
        v->cur_addr = ssa_addr;
        uint32_t lsa=((uint32_t)read_vaddr_reg16(core,voice,SPU2_VA_LSA_HI)<<16)|read_vaddr_reg16(core,voice,SPU2_VA_LSA_LO);
        v->repeat_addr = (lsa&0xffff8u)*2u;
        v->block_pos = 28; /* force decode on first produce_sample() call */

        g_total_kon++;
    }
}

void spu2_mixer_on_koff(int core, int is_upper_half, uint16_t bits)
{
    if (core < 0 || core >= SPU2_MIXER_NUM_CORES) return;
    int base_voice = is_upper_half ? 16 : 0;
    for (int b = 0; b < 16; b++) {
        if (!(bits & (1u << b))) continue;
        int voice = base_voice + b;
        if (voice >= SPU2_MIXER_VOICES_PER_CORE) continue;
        voice_runtime_t *v = &g_voice[core][voice];
        if (v->active && v->phase != ENV_OFF && v->phase != ENV_RELEASE) {
            v->phase = ENV_RELEASE; /* real: KOFF "Starts Release", any phase */
            v->adsr_counter = 0;
        }
    }
}

static int32_t clamp16(int32_t x){return x>32767?32767:x< -32768?-32768:x;}
/* Mixer/Capture RAM writes are guest-visible and can request IRQ9 even
 * for silent output. PCSX2 SPU2/Mixer.cpp's spu2M_WriteFast contract. */
static void output_ram_word(uint32_t word,int32_t value)
{
 int16_t sample=(int16_t)clamp16(value);g_spu2_ram[word*2u]=(uint8_t)sample;g_spu2_ram[word*2u+1u]=(uint8_t)((uint16_t)sample>>8);
 iop_spu2_check_ram_irq(word,1u);
}
void spu2_mixer_render_frames(int16_t *out_lr, int n_frames)
{
 if(!out_lr||n_frames<=0)return;
 for(int f=0;f<n_frames;f++){
  unsigned output_pos=(unsigned)g_total_frames&511u;
  int32_t ext_l=0,ext_r=0;
  for(int core=0;core<2;core++){
   int32_t mix_l=0,mix_r=0,wet_l=0,wet_r=0,capture[2]={0,0};
   uint32_t vlmask=read_core_reg16(core,SPU2_C_VMIXL)|((uint32_t)read_core_reg16(core,SPU2_C_VMIXL+2u)<<16);
   uint32_t vrmask=read_core_reg16(core,SPU2_C_VMIXR)|((uint32_t)read_core_reg16(core,SPU2_C_VMIXR+2u)<<16);
   uint32_t welmask=read_core_reg16(core,SPU2_C_VMIXL+4u)|((uint32_t)read_core_reg16(core,SPU2_C_VMIXL+6u)<<16);
   uint32_t wermask=read_core_reg16(core,SPU2_C_VMIXR+4u)|((uint32_t)read_core_reg16(core,SPU2_C_VMIXR+6u)<<16);
   for(int voice=0;voice<24;voice++){
    voice_runtime_t *v=&g_voice[core][voice];if(!v->active)continue;
    voice_tick_envelope(v,read_voice_reg16(core,voice,SPU2_V_ADSR1),read_voice_reg16(core,voice,SPU2_V_ADSR2));
    iop_spu2_set_raw16(iop_spu2_voice_reg_addr(core,voice,SPU2_V_ENVX),(uint16_t)v->level);
    if(!v->active)continue;
    int32_t raw=voice_produce_sample(core,voice,v,read_voice_reg16(core,voice,SPU2_V_PITCH));
    iop_spu2_set_raw16(iop_spu2_voice_reg_addr(core,voice,SPU2_V_ENVX),(uint16_t)v->level);
    int32_t sample=(raw*v->level)>>15;
    if(voice==1)capture[0]=sample;else if(voice==3)capture[1]=sample;
    int32_t vl=fixed_volume(read_voice_reg16(core,voice,SPU2_V_VOLL));
    int32_t vr=fixed_volume(read_voice_reg16(core,voice,SPU2_V_VOLR));
    iop_spu2_set_raw16(iop_spu2_voice_reg_addr(core,voice,SPU2_V_VOLXL),(uint16_t)vl);
    iop_spu2_set_raw16(iop_spu2_voice_reg_addr(core,voice,SPU2_V_VOLXR),(uint16_t)vr);
    uint32_t na=(v->cur_addr/2u+1u+(unsigned)(v->block_pos<28?v->block_pos:27)/4u)&0xfffffu;
    iop_spu2_set_raw16(iop_spu2_voice_addr_reg_addr(core,voice,SPU2_VA_NAX_HI),(uint16_t)(na>>16));
    iop_spu2_set_raw16(iop_spu2_voice_addr_reg_addr(core,voice,SPU2_VA_NAX_LO),(uint16_t)na);
    if(vlmask&(1u<<voice))mix_l+=(sample*vl)>>15;
    if(vrmask&(1u<<voice))mix_r+=(sample*vr)>>15;
    if(welmask&(1u<<voice))wet_l+=(sample*vl)>>15;
    if(wermask&(1u<<voice))wet_r+=(sample*vr)>>15;
   }
   uint32_t dry=core?0x1800u:0x1000u;
   output_ram_word((core?0xc00u:0x400u)+output_pos,capture[0]);
   output_ram_word((core?0xe00u:0x600u)+output_pos,capture[1]);
   output_ram_word(dry+output_pos,mix_l);output_ram_word(dry+0x200u+output_pos,mix_r);
   output_ram_word(dry+0x400u+output_pos,wet_l);output_ram_word(dry+0x600u+output_pos,wet_r);
   uint16_t mmix=read_core_reg16(core,SPU2_C_MMIX);
   if(!(mmix&0x800u))mix_l=0;else mix_l=clamp16(mix_l);
   if(!(mmix&0x400u))mix_r=0;else mix_r=clamp16(mix_r);
   if(core){
    if(mmix&8u)mix_l+=(ext_l*(int16_t)read_output_reg16(core,SPU2_S_EXTVOLL))>>15;
    if(mmix&4u)mix_r+=(ext_r*(int16_t)read_output_reg16(core,SPU2_S_EXTVOLR))>>15;
   }
   int16_t in[2];iop_spu2_adma_sample(core,in);
   if(mmix&0x80u)mix_l+=((int32_t)in[0]*(int16_t)read_output_reg16(core,SPU2_S_INPVOLL))>>15;
   if(mmix&0x40u)mix_r+=((int32_t)in[1]*(int16_t)read_output_reg16(core,SPU2_S_INPVOLR))>>15;
   int32_t ml=fixed_volume(read_output_reg16(core,SPU2_S_MVOLL)),mr=fixed_volume(read_output_reg16(core,SPU2_S_MVOLR));
   iop_spu2_set_raw16(IOP_SPU2_BASE+0x770u+(core?0x28u:0u),(uint16_t)ml);
   iop_spu2_set_raw16(IOP_SPU2_BASE+0x772u+(core?0x28u:0u),(uint16_t)mr);
   ext_l=(clamp16(mix_l)*ml)>>15;ext_r=(clamp16(mix_r)*mr)>>15;
   if(!core){output_ram_word(0x800u+output_pos,ext_l);output_ram_word(0xa00u+output_pos,ext_r);}
  }
  out_lr[f*2]=clamp16(ext_l);out_lr[f*2+1]=clamp16(ext_r);g_total_frames++;
 }
}

uint64_t spu2_mixer_get_total_kon_count(void)
{
    return g_total_kon;
}

uint64_t spu2_mixer_get_total_frames_rendered(void)
{
    return g_total_frames;
}

typedef struct {voice_runtime_t voice[2][24];uint64_t kon,frames;} mixer_snapshot;
uint32_t spu2_mixer_snapshot_size(void){return sizeof(mixer_snapshot);}
void spu2_mixer_snapshot_save(void *out)
{
 mixer_snapshot *s=out;memset(s,0,sizeof *s);memcpy(s->voice,g_voice,sizeof g_voice);s->kon=g_total_kon;s->frames=g_total_frames;
}
int spu2_mixer_snapshot_valid(const void *data)
{
 const mixer_snapshot *s=data;
 for(int c=0;c<2;c++)for(int i=0;i<24;i++){
  const voice_runtime_t *v=&s->voice[c][i];
  if(v->active<0||v->active>1||v->phase>ENV_RELEASE||v->phase<ENV_OFF||v->level<0||v->level>32767||
     v->cur_addr>=SPU2_MIXER_RAM_SIZE||(v->cur_addr&15u)||v->repeat_addr>=SPU2_MIXER_RAM_SIZE||(v->repeat_addr&15u)||
     v->block_pos<0||v->block_pos>28||v->block_valid<0||v->block_valid>1||
     v->manual_loop<0||v->manual_loop>1||v->pending_loop_end<0||v->pending_loop_end>1||v->pending_loop_repeat<0||v->pending_loop_repeat>1||v->pitch_counter>=4096u)return 0;
 }
 return 1;
}
void spu2_mixer_snapshot_load(const void *data)
{const mixer_snapshot *s=data;memcpy(g_voice,s->voice,sizeof g_voice);g_total_kon=s->kon;g_total_frames=s->frames;}

void spu2_mixer_on_loop_address_write(int core,int voice)
{
 if(core<0||core>1||voice<0||voice>=24)return;
 voice_runtime_t *v=&g_voice[core][voice];
 uint32_t word=((uint32_t)read_vaddr_reg16(core,voice,SPU2_VA_LSA_HI)<<16)|read_vaddr_reg16(core,voice,SPU2_VA_LSA_LO);
 v->repeat_addr=(word&0xffff8u)*2u;v->manual_loop=1;
}
