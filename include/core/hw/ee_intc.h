#ifndef PCSX2WII_EE_INTC_H
#define PCSX2WII_EE_INTC_H

#include <stdint.h>

/*
 * ee_intc.h - EE interrupt controller (INTC_STAT/INTC_MASK)
 *
 * Real EE hardware memory map (PCSX2's pcsx2/Hw.h EERegisterAddresses):
 *   INTC_STAT = 0x1000F000
 *   INTC_MASK = 0x1000F010
 *
 * Task #176 (splash-screen blocker investigation): this project had
 * NO external-interrupt-source model for the EE at all before this -
 * only Cause.IP7 (the internal COP0 Timer/Compare interrupt, see
 * ee_check_timer_interrupt() in ee_core.c) was ever raised. Real
 * hardware routes ten external sources (GS, SBUS, VBLANK start/end,
 * VIF0/1, VU0/1, IPU, Timers 0-3, SFIFO, VU0 watchdog) through this
 * single INTC_STAT/MASK pair into Cause.IP2 - this file only models
 * the register pair itself; ee_core.c's ee_check_intc_interrupt()
 * does the Cause.IP2 raising.
 *
 * Semantics ported from real PCSX2 source (pcsx2/HwWrite.cpp's
 * mcase(INTC_STAT)/mcase(INTC_MASK) in _hwWrite32(), pcsx2/Hw.cpp's
 * intcInterrupt()), not reinvented:
 *
 *   INTC_STAT: read is a plain value. WRITE clears the bits that are
 *              SET in the written value - `psHu32(INTC_STAT) &= ~value`
 *              (write-1-to-clear/acknowledge, matching the polarity
 *              already used by GS_CSR/SIF SMFLAG elsewhere in this
 *              project - NOT the IOP's I_STAT "write-0-to-clear"
 *              polarity, which is the opposite and easy to confuse).
 *   INTC_MASK: read is a plain value. WRITE TOGGLES (XORs) the bits
 *              that are set in the (16-bit-truncated) written value -
 *              `psHu32(INTC_MASK) ^= (u16)value` - this is a real
 *              hardware quirk (also used by DMAC_STAT's upper/enable
 *              half, see dma.h), not "plain assignment" like most
 *              other mask registers in this project.
 *
 * An interrupt is pending (should raise Cause.IP2) whenever
 * (INTC_STAT & INTC_MASK) != 0 - see intcInterrupt() in Hw.cpp.
 */

typedef struct {
    uint32_t stat;
    uint32_t mask;
} ee_intc_state_t;

void ee_intc_init(void);
ee_intc_state_t *ee_intc_get_state(void);

/* Returns 1 and fills *out if addr is INTC_STAT/INTC_MASK, 0
 * otherwise - same convention as dma_mmio_read32/sif_mmio_read32. */
int ee_intc_mmio_read32(uint32_t addr, uint32_t *out);
int ee_intc_mmio_write32(uint32_t addr, uint32_t value);

/* Sets bit `irq` (0-31) in INTC_STAT, as a real EE peripheral would
 * when it has a pending interrupt to report.
 *
 * Round 1107 correction (task #1033 follow-up): the comment that used
 * to sit here ("Not yet called by anything ... no peripheral in this
 * project raises a real IP2 source yet") is STALE and was found to be
 * actively misleading during this round's fresh investigation - it
 * was written at task #176 (this file's introduction) and never
 * updated after task #179 (Round 178+, see ee_core.c's own citation
 * at its EE_CYCLES_PER_FRAME_NTSC block) wired up the real VBLANK
 * source. As of this round, grep confirms real callers already exist
 * across the tree:
 *   - ee_core.c: ee_check_vblank() raises EE_INTC_IRQ_VBLANK_START/END
 *     every real NTSC frame (cop0[9]/Count-driven, task #179); GS
 *     completion raises EE_INTC_IRQ_GS; SBUS raises EE_INTC_IRQ_SBUS
 *     on the IOP->EE mailbox path (task #344).
 *   - hw/dma.c, hw/sif.c, hw/iop_icfg.c: further EE_INTC_IRQ_SBUS
 *     raise sites (DMAC completion / SIF / IOP config paths).
 *   - hw/ee_timers.c: per-timer IRQ bits (Timers 0-3).
 * i.e. GS, SBUS, VBLANK_START, VBLANK_END and the four EE timer
 * sources are ALL genuinely raised today - only VIF0/1, VU0/1, IPU,
 * SFIFO and the VU0 watchdog remain unraised (this project does not
 * yet model those peripherals at all, which is a separate, much
 * larger scope than a doc fix). Left un-reworded further than this
 * so a future grep for "Not yet called by anything" won't still find
 * a false hit here. */
void ee_intc_raise(int irq);
void ee_intc_raise_sbus_event(void);
uint32_t ee_intc_get_sbus_event_pending(void);

/* Returns 1 if (stat & mask) != 0 - i.e. a real IP2 interrupt is
 * currently pending and unmasked. ee_core.c's ee_check_intc_interrupt()
 * calls this every step, mirroring ee_check_timer_interrupt()'s
 * Cause.IP7 pattern for this new external line. */
int ee_intc_pending(void);

/* Round 716 (task #696-699): real per-cause IRQ-raise hit counter,
 * mirroring ee_timers_get_irq_count() (Round 715). Purely diagnostic -
 * does not affect emulated behavior. */
uint32_t ee_intc_get_raise_count(int irq);
uint32_t ee_intc_get_sbus_ack_count(void);

#endif
