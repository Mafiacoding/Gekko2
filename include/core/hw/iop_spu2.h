#ifndef PCSX2_WII_IOP_SPU2_H
#define PCSX2_WII_IOP_SPU2_H

#include <stdint.h>

/* Native SPU2 beta: two 24-voice cores, register/FIFO/DMA addressing,
 * guest-clock ADPCM synthesis and optional Wii PCM output.
 * Sources: PCSX2 SPU2/{RegTable,spu2sys,Dma,Mixer,ReadInput}.cpp and
 * ps2sdk common/include/libsd-common.h. Reverb, noise, PMON, sweep,
 * Gaussian interpolation, AC3 and CDDA32 remain unsupported. */
#define IOP_SPU2_BASE 0x1F900000u
#define IOP_SPU2_SIZE 0x0800u

#define SPU2_CORE1_OFFSET 0x400u /* add to every offset below for Core1 */

#define SPU2_VOICE_STRIDE   0x10u
#define SPU2_VOICE_COUNT    24u
#define SPU2_V_VOLL         0x00u /* Voice Volume Left */
#define SPU2_V_VOLR         0x02u /* Voice Volume Right */
#define SPU2_V_PITCH        0x04u /* Pitch */
#define SPU2_V_ADSR1        0x06u /* ADSR envelope spec 1 */
#define SPU2_V_ADSR2        0x08u /* ADSR envelope spec 2 */
#define SPU2_V_ENVX         0x0Au /* current envelope value */
#define SPU2_V_VOLXL        0x0Cu /* current voice volume left */
#define SPU2_V_VOLXR        0x0Eu /* current voice volume right */

/* Per-voice address block (24 voices, stride 0x0C, base = core base +
 * 0x1C0) - real 32-bit addresses split into hi/lo 16-bit halves. */
#define SPU2_VADDR_BASE     0x1C0u
#define SPU2_VADDR_STRIDE   0x0Cu
#define SPU2_VA_SSA_HI      0x00u /* waveform start address, hi */
#define SPU2_VA_SSA_LO      0x02u /* waveform start address, lo */
#define SPU2_VA_LSA_HI      0x04u /* loop-point address, hi */
#define SPU2_VA_LSA_LO      0x06u /* loop-point address, lo */
#define SPU2_VA_NAX_HI      0x08u /* next address to be read, hi */
#define SPU2_VA_NAX_LO      0x0Au /* next address to be read, lo */

/* Core-level control/status registers (base = core base + these
 * offsets). */
#define SPU2_C_FMOD1        0x180u /* pitch modulation spec 1 */
#define SPU2_C_FMOD2        0x182u /* pitch modulation spec 2 */
#define SPU2_C_NON          0x184u /* noise generator enable */
#define SPU2_C_VMIXL        0x188u /* voice mix dry/wet, left */
#define SPU2_C_VMIXR        0x190u /* voice mix dry/wet, right */
#define SPU2_C_MMIX         0x198u /* output spec after voice mix */
#define SPU2_C_CTRL         0x19Au /* core control/attribute register */
#define SPU2_C_IRQA_HI      0x19Cu /* interrupt address, hi */
#define SPU2_C_IRQA_LO      0x19Eu /* interrupt address, lo */
#define SPU2_C_KON0         0x1A0u /* key on, voices 0-15 */
#define SPU2_C_KON1         0x1A2u /* key on, voices 16-23 */
#define SPU2_C_KOFF0        0x1A4u /* key off, voices 0-15 */
#define SPU2_C_KOFF1        0x1A6u /* key off, voices 16-23 */
#define SPU2_C_TSA_HI       0x1A8u /* DMA transfer address, hi */
#define SPU2_C_TSA_LO       0x1AAu /* DMA transfer address, lo */
#define SPU2_C_TDATA        0x1ACu /* transfer data FIFO */
#define SPU2_C_ADMAS        0x1B0u /* AutoDMA status */
#define SPU2_C_ENDX0        0x340u /* end-point-passed flags, voices 0-15 */
#define SPU2_C_ENDX1        0x342u /* end-point-passed flags, voices 16-23 */
#define SPU2_C_STATX        0x344u /* bit10: transfer in progress (ps2sdk) */

/* Output parameters: core0 at 0x760; core1 at 0x788 (+0x28). */
#define SPU2_S_MVOLL        0x760u /* master volume left */
#define SPU2_S_MVOLR        0x762u /* master volume right */
#define SPU2_S_EVOLL        0x764u /* effect volume left */
#define SPU2_S_EVOLR        0x766u /* effect volume right */

#define SPU2_S_EXTVOLL 0x768u
#define SPU2_S_EXTVOLR 0x76au
#define SPU2_S_INPVOLL 0x76cu
#define SPU2_S_INPVOLR 0x76eu
#define SPU2_S_IRQINFO 0x7c2u
/* Internal raw status writes bypass trigger side effects. */
void iop_spu2_set_raw16(uint32_t addr,uint16_t value);
void iop_spu2_sync(uint64_t sched_ticks);
void iop_spu2_sync_now(void);
unsigned iop_spu2_event_distance(unsigned maximum);
void iop_spu2_check_ram_irq(uint32_t word_address,uint32_t words);
int iop_spu2_adma_begin(int core,uint32_t madr,uint32_t bytes);
void iop_spu2_adma_sample(int core,int16_t out[2]);
void iop_spu2_adma_cancel(int core);
uint32_t iop_spu2_audio_stat(unsigned index);
uint32_t iop_spu2_snapshot_size(void);
void iop_spu2_snapshot_save(void *out);
int iop_spu2_snapshot_valid(const void *data,uint64_t ticks,uint32_t ram_size);
void iop_spu2_snapshot_load(const void *data);
void iop_spu2_restore_legacy(uint64_t ticks);
extern uint32_t g_spu2_irq_clock_required,g_spu2_next_sample;
void iop_spu2_init(void);

/* Same convention as every other *_mmio_read16/write16 helper in this
 * project: returns 1 and fills *out if addr falls in the modeled
 * range, 0 otherwise. */
int iop_spu2_mmio_read16(uint32_t addr, uint16_t *out);
int iop_spu2_mmio_write16(uint32_t addr, uint16_t value);

/* 32-bit accessors, since some real IOP code (and this project's own
 * iop_mem_read32/write32 path) may still touch these addresses with
 * LW/SW despite the real hardware being 16-bit-native - handled here
 * as two adjacent 16-bit register slots rather than left unmodeled. */
int iop_spu2_mmio_read32(uint32_t addr, uint32_t *out);
int iop_spu2_mmio_write32(uint32_t addr, uint32_t value);

/* Round 185 convenience helpers: compute the real absolute address
 * for a named per-voice / per-voice-address / core-level register
 * (see the offset table above), then read/write it via the existing
 * byte-addressable register file. core: 0 or 1. voice: 0-23. These
 * are pure address arithmetic + passthrough - no new hardware
 * behavior, matching this file's stated scope. */
uint32_t iop_spu2_voice_reg_addr(int core, int voice, uint32_t voice_reg_offset);
uint32_t iop_spu2_voice_addr_reg_addr(int core, int voice, uint32_t vaddr_reg_offset);
uint32_t iop_spu2_core_reg_addr(int core, uint32_t core_reg_offset);

#endif
