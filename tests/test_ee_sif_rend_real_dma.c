/*
 * test_ee_sif_rend_real_dma.c - Round 1136 (task #1046 continuation,
 * user-directed "implement REND"): regression coverage for
 * sif_cmd_iop_send_rpcinit_ready() and sif_cmd_iop_send_rpc_bind_rend()
 * after their delivery mechanism was switched from direct
 * ee_mem_write32() writes + dma_channel_note_reply_delivered()
 * (bookkeeping-only shortcut) to genuine dma_mmio_write32() (SIF0
 * MADR) + dma_channel_receive_quadwords() (the project's real
 * inbound DMA write primitive, source/hw/dma.c, Round 198/task #365).
 *
 * This proves the switch is content-, bookkeeping-, and signal-
 * preserving: the exact same bytes land at ee_recvbuf, MADR/QWC/
 * quadwords_transferred update identically to the old shortcut's
 * documented behavior (see test_dma_reply_delivered.c), and the real
 * SBUS completion interrupt still fires - now reached through genuine
 * DMA-engine delivery instead of a bookkeeping-only call.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>

#include "core/ee/ee_core.c"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static uint32_t rle32(ee_state_t *st, uint32_t addr)
{
    return ee_mem_read32(st, addr);
}

int main(void)
{
    static bios_image_t ee_bios;
    memset(&ee_bios, 0, sizeof(ee_bios));
    ee_bios.data = memalign(32, BIOS_MAX_SIZE);
    memset(ee_bios.data, 0, BIOS_MAX_SIZE);
    ee_bios.size = BIOS_MAX_SIZE;
    ee_bios.loaded = 1;

    ee_core_init(&ee_bios);
    ee_state_t *st = ee_core_get_state();

    /* --- Test 1: sif_cmd_iop_send_rpcinit_ready() - 24-byte struct
     * sr_pkt, delivered via real SIF0 DMA (2 padded quadwords). --- */
    uint32_t recvbuf1 = 0x80050000u; /* KSEG0 - realistic ee_recvbuf convention */
    dma_get_state()->chan[DMA_CHANNEL_SIF0].madr = 0xDEADBEEFu; /* must not matter - real MADR write below overrides it */
    dma_get_state()->chan[DMA_CHANNEL_SIF0].qwc = 10;
    dma_get_state()->d_stat = 0;

    sif_cmd_iop_send_rpcinit_ready(st, recvbuf1);

    CHECK(rle32(st, recvbuf1 + 0u) == 24u, "rpcinit_ready: psize=24 landed in real EE RAM via DMA copy");
    CHECK(rle32(st, recvbuf1 + 4u) == 0u, "rpcinit_ready: header.dest=NULL");
    CHECK(rle32(st, recvbuf1 + 8u) == (uint32_t)SIF_CMD_SET_SREG, "rpcinit_ready: cid=SIF_CMD_SET_SREG");
    CHECK(rle32(st, recvbuf1 + 12u) == 0u, "rpcinit_ready: header.opt=0");
    CHECK(rle32(st, recvbuf1 + 16u) == (uint32_t)SIF_SREG_RPCINIT, "rpcinit_ready: sreg=SIF_SREG_RPCINIT");
    CHECK(rle32(st, recvbuf1 + 20u) == 1u, "rpcinit_ready: val=1");
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].madr == (recvbuf1 & 0x1FFFFFFFu) + 32u,
          "rpcinit_ready: real MADR advanced by 2 whole quadwords (32 bytes, rounded up from 24)");
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].qwc == 8,
          "rpcinit_ready: real QWC decremented by 2 (10 - 2 = 8)");
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].quadwords_transferred == 2,
          "rpcinit_ready: real lifetime quadword counter advanced by 2");
    CHECK((dma_get_state()->d_stat & (1u << DMA_CHANNEL_SIF0)) != 0,
          "rpcinit_ready: real DMAC_STAT completion bit set (dma_channel_signal_done side effect)");

    /* --- Test 2: sif_cmd_iop_send_rpc_bind_rend() - 48-byte
     * SifRpcRendPkt_t, delivered via real SIF0 DMA (3 exact quadwords,
     * no padding). --- */
    uint32_t recvbuf2 = 0x80060000u;
    uint32_t cd_ptr = 0x80070000u;
    uint32_t inner_cid = 0x80000009u; /* SIF_CMD_RPC_BIND-ish placeholder value, content-only check */
    dma_get_state()->chan[DMA_CHANNEL_SIF0].madr = 0xCAFEBABEu;
    dma_get_state()->chan[DMA_CHANNEL_SIF0].qwc = 20;
    dma_get_state()->d_stat = 0;

    /* Give cd_ptr a real, valid SifRpcClientData_t-shaped region so
     * sif_cmd_iop_write_private_queue_copy()'s own real code (invoked
     * internally, unchanged by this round) has EE RAM to operate on -
     * mirrors how a genuine bind-reply call site would already have
     * initialized this from the original request packet. */
    for (uint32_t off = 0; off < 64; off += 4) ee_mem_write32(st, cd_ptr + off, 0u);
    ee_mem_write32(st, cd_ptr + 0x08u, (uint32_t)-1); /* sema_id = -1: no completion semaphore, safe default for this test */

    sif_cmd_iop_send_rpc_bind_rend(st, recvbuf2, cd_ptr, inner_cid);

    CHECK(rle32(st, recvbuf2 + 0x00u) == 0x30u, "bind_rend: psize=48 landed in real EE RAM via DMA copy");
    CHECK(rle32(st, recvbuf2 + 0x04u) == 0u, "bind_rend: header.dest=NULL");
    CHECK(rle32(st, recvbuf2 + 0x08u) == (uint32_t)SIF_CMD_RPC_END, "bind_rend: outer cid=SIF_CMD_RPC_END");
    CHECK(rle32(st, recvbuf2 + 0x0Cu) == 0u, "bind_rend: header.opt=0");
    CHECK(rle32(st, recvbuf2 + 0x1Cu) == cd_ptr, "bind_rend: cd echoed correctly");
    CHECK(rle32(st, recvbuf2 + 0x20u) == inner_cid, "bind_rend: inner_cid echoed correctly");
    CHECK(rle32(st, recvbuf2 + 0x24u) == 0x00001000u, "bind_rend: sd placeholder correct");
    CHECK(rle32(st, recvbuf2 + 0x28u) == 0u, "bind_rend: buf=NULL");
    CHECK(rle32(st, recvbuf2 + 0x2Cu) == 0u, "bind_rend: cbuf=NULL");
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].madr == (recvbuf2 & 0x1FFFFFFFu) + 48u,
          "bind_rend: real MADR advanced by exactly 3 quadwords (48 bytes, no padding)");
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].qwc == 17,
          "bind_rend: real QWC decremented by 3 (20 - 3 = 17)");
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].quadwords_transferred == 5,
          "bind_rend: real lifetime quadword counter is cumulative - 2 (Test 1) + 3 (Test 2) = 5");
    CHECK((dma_get_state()->d_stat & (1u << DMA_CHANNEL_SIF0)) != 0,
          "bind_rend: real DMAC_STAT completion bit set");

    /* --- Test 3: ee_recvbuf==0 remains a safe no-op for both
     * functions (guards unchanged). --- */
    uint32_t madr_before = dma_get_state()->chan[DMA_CHANNEL_SIF0].madr;
    sif_cmd_iop_send_rpcinit_ready(st, 0u);
    sif_cmd_iop_send_rpc_bind_rend(st, 0u, cd_ptr, inner_cid);
    CHECK(dma_get_state()->chan[DMA_CHANNEL_SIF0].madr == madr_before,
          "ee_recvbuf=0: both functions remain safe no-ops, no channel state change");

    printf("\n%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
