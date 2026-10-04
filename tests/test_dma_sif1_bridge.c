/*
 * test_dma_sif1_bridge.c - Round 1106 (task #1033 follow-up, per the
 * user's explicit "check the SIF1 bridge" request): end-to-end
 * regression proof for the Round 1105 SIF1 (EE->IOP) DMA fix.
 *
 * Round 1105's own regression pass (test_dma_core/test_dma_inbound/
 * etc.) verified the two underlying primitives
 * (iop_dma_channel_write_bytes(), iop_dma_signal_channel_done()) in
 * isolation, and verified that the new sink function *compiles* and
 * that ee_core.c's two dma_set_sink(DMA_CHANNEL_SIF1, ...) call sites
 * link - but nothing yet drove a REAL EE-side SIF1 DMA kick through
 * dma.c's generic transfer_quadwords()/g_sinks[] dispatch into the
 * REAL iop_dma_sif1_ee_to_iop_sink() (not a capture stub, unlike
 * test_dma_sif2.c's test_sink()) and confirmed the bytes actually
 * land in g_iop_ram. That's the one link in the chain this test
 * closes.
 *
 * Deliberately does NOT directly include dma.c's source (the way
 * test_dma_sif2.c does) - dma.c and iop_dma.c both declare a file-static `g_dma`
 * struct (of different types: dma_state_t vs iop_dma_state_t), so
 * #including both into one translation unit would be a duplicate-
 * symbol compile error. Instead this file #includes only iop_dma.c
 * (to reach its static internals for the ICR/RAM assertions below)
 * and links against the separately-compiled source/hw/dma.c object
 * for the EE-side public API (dma_init/dma_bind_ee_ram/dma_set_sink/
 * dma_mmio_write32) - run_test.sh's self-include exclusion (see that
 * script's own header comment) handles this automatically since only
 * iop_dma.c is named in an #include here.
 */
#include <stdio.h>
#include <string.h>

#include "hw/iop_dma.c"     /* real iop_dma_sif1_ee_to_iop_sink(), static g_dma (IOP-side ICR) for assertions */
#include "core/hw/dma.h"    /* dma_init/dma_bind_ee_ram/dma_set_sink/dma_mmio_write32 - linked from dma.c */

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static uint8_t g_ee_ram[1024 * 1024];
static uint8_t g_iop_ram_buf[1024 * 1024];

int main(void)
{
    /* EE side (real dma.c, separately compiled) */
    dma_init();
    memset(g_ee_ram, 0, sizeof(g_ee_ram));
    dma_bind_ee_ram(g_ee_ram, sizeof(g_ee_ram));
    dma_set_sink(DMA_CHANNEL_SIF1, iop_dma_sif1_ee_to_iop_sink); /* the exact Round 1105 registration, not a test stub */

    /* IOP side (this file's own iop_dma.c internals) */
    iop_dma_init();
    memset(g_iop_ram_buf, 0, sizeof(g_iop_ram_buf));
    iop_dma_bind_iop_ram(g_iop_ram_buf, sizeof(g_iop_ram_buf));

    CHECK(DMA_CHANNEL_SIF1 == 6, "DMA_CHANNEL_SIF1 constant is 6 (dma.h)");
    CHECK(IOP_DMA_SIF1_CHANNEL == 10, "IOP_DMA_SIF1_CHANNEL constant is 10 (matches iop_dma.c's s_ranges[] SIF1 entry)");

    /* Real EE-side SIF1 base address, confirmed against dma.c's own
     * s_ranges[] table: { 0x1000C400u, 0x0400u, DMA_CHANNEL_SIF1 }. */
    for (int i = 0; i < 16; i++) g_ee_ram[0x9000 + i] = (uint8_t)(0xA0 + i);

    /* Set MADR/QWC through the real public MMIO write path (not by
     * poking dma.c's internal struct directly - that struct isn't
     * visible here anyway, it lives in the separately-compiled
     * dma.c). This IS how a real EE-side SIF1 kick happens. */
    CHECK(dma_mmio_write32(0x1000C410u, 0x9000u) == 1, "SIF1 MADR (+0x10) write accepted");
    CHECK(dma_mmio_write32(0x1000C420u, 1u) == 1, "SIF1 QWC (+0x20) write accepted");

    /* Fix the IOP-side destination MADR for this test's channel-10
     * IOP DMA registers, matching a real BIOS-programmed SIF1 IOP
     * destination */
    g_dma.ch[IOP_DMA_SIF1_CHANNEL].madr = 0x2000;

    /* --- The real kick: EE writes CHCR with STR=1 (NORMAL mode) --- */
    CHECK(dma_mmio_write32(0x1000C400u, 0x00000100u) == 1, "SIF1 CHCR (STR=1, NORMAL mode) kick accepted");

    /* --- Verify the bytes genuinely crossed into g_iop_ram --- */
    CHECK(memcmp(g_iop_ram_buf + 0x2000, g_ee_ram + 0x9000, 16) == 0,
          "SIF1 bridge: EE-side source bytes landed byte-for-byte in IOP RAM at the IOP channel's MADR");
    CHECK(g_dma.ch[IOP_DMA_SIF1_CHANNEL].madr == 0x2010,
          "SIF1 bridge: IOP-side MADR auto-advanced by 16 bytes (1 quadword), same convention as SIF0/SIF2");

    /* --- Verify the real per-channel completion IRQ fired on the
     * IOP side (channel 10's ICR2 flag bit - iop_dma_signal_channel_
     * done()'s own real per-channel bit math, same path SIF0/SIF2
     * already use and test_iop_dma.c already covers in isolation) --- */
    uint32_t icr2_before_clear = g_dma.icr2;
    CHECK(icr2_before_clear != 0, "SIF1 bridge: IOP-side ICR2 shows a real completion flag set after the bridge fired iop_dma_signal_channel_done()");

    /* --- Bytes NEVER silently dropped: qwc=2 (32 bytes) at a fresh
     * MADR proves the fix isn't a single-quadword-only special case --- */
    for (int i = 0; i < 32; i++) g_ee_ram[0xA000 + i] = (uint8_t)(0xC0 + i);
    dma_mmio_write32(0x1000C410u, 0xA000u); /* SIF1 MADR */
    dma_mmio_write32(0x1000C420u, 2u);      /* SIF1 QWC = 2 quadwords */
    g_dma.ch[IOP_DMA_SIF1_CHANNEL].madr = 0x3000;
    dma_mmio_write32(0x1000C400u, 0x00000100u); /* kick */
    CHECK(memcmp(g_iop_ram_buf + 0x3000, g_ee_ram + 0xA000, 32) == 0,
          "SIF1 bridge: 2-quadword (32 byte) transfer also lands correctly, not just qwc=1");

    /* --- Channel isolation: a SIF1 kick must not touch the IOP-side
     * SIF0(9)/SIF2(2) channel registers --- */
    CHECK(g_dma.ch[IOP_DMA_SIF0_CHANNEL].madr == 0, "SIF1 bridge activity did not touch IOP-side SIF0 channel");
    CHECK(g_dma.ch[IOP_DMA_SIF2_CHANNEL].madr == 0, "SIF1 bridge activity did not touch IOP-side SIF2 channel");

    printf("\n%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
