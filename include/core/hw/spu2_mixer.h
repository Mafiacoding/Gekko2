#ifndef PCSX2_WII_SPU2_MIXER_H
#define PCSX2_WII_SPU2_MIXER_H

#include <stdint.h>

/* SPU2-native beta: 16-bit-word addresses, 48 kHz. PCM generation,
 * guest-clock scheduling, DMA delivery and optional Wii output are wired.
 * Noise, pitch modulation and voice/master volume sweeps are modeled.
 * Reverb/Gaussian interpolation and full silent-voice timing remain open.
 * Sources: PCSX2 pcsx2/SPU2/{Mixer,Dma,RegTable,spu2sys}.cpp and
 * ps2sdk common/include/libsd-common.h. */
#define SPU2_MIXER_RAM_SIZE (2u * 1024u * 1024u)
#define SPU2_MIXER_NUM_CORES 2
#define SPU2_MIXER_VOICES_PER_CORE 24
#define SPU2_MIXER_SAMPLE_RATE_HZ 48000

void spu2_mixer_init(void);

/* Shared 2 MiB SPU2 RAM, accessed by DMA4/7, FIFO and decoder. */
uint8_t *spu2_mixer_get_ram(void);

/* Real KON/KOFF hooks - call these from iop_spu2.c's existing
 * KON0/KON1/KOFF0/KOFF1 register-write handling (same call site
 * Round 524 already added the real "KON clears ENDX" logic to),
 * passing the real per-half 16-bit value written (voices 0-15 for
 * the *0 register, 16-23 for the *1 register - bit N = voice N or
 * N+16). core is 0 or 1. */
void spu2_mixer_on_kon(int core, int is_upper_half, uint16_t bits);
void spu2_mixer_on_loop_address_write(int core,int voice);
void spu2_mixer_on_volume_write(uint32_t addr,uint16_t value);
void spu2_mixer_on_koff(int core, int is_upper_half, uint16_t bits);

/* Renders n_frames real stereo S16 PCM frames (L,R interleaved) at
 * SPU2_MIXER_SAMPLE_RATE_HZ into out, advancing every active voice's
 * real ADPCM decode/pitch/ADSR state by exactly that many 48000Hz
 * ticks. Reads live voice register values (VOLL/VOLR/PITCH/ADSR1/
 * ADSR2/SSA/LSA) directly from iop_spu2.c's existing register file
 * via its own iop_spu2_mmio_read16()/iop_spu2_voice_reg_addr() API -
 * no duplicated register storage. Silent (all-zero) output is a real,
 * correct result when no voice is currently keyed on - not a
 * failure. */
void spu2_mixer_render_frames(int16_t *out_lr, int n_frames);

/* Diagnostic counters, non-hardware, for host-native survey drivers -
 * same established convention as gif_state_t's Round 577
 * gif_path1_transfers counter. */
uint64_t spu2_mixer_get_total_kon_count(void);
uint64_t spu2_mixer_get_total_frames_rendered(void);

uint32_t spu2_mixer_snapshot_size(void);
void spu2_mixer_snapshot_save(void *out);
int spu2_mixer_snapshot_valid(const void *data);
void spu2_mixer_snapshot_load(const void *data);
int spu2_mixer_snapshot_valid_sized(const void *data,uint32_t bytes);
void spu2_mixer_snapshot_load_sized(const void *data,uint32_t bytes);
#endif
