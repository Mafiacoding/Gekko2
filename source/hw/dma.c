/*
 * dma.c - EE DMA controller register skeleton
 *
 * See include/core/hw/dma.h for scope notes. Register layout matches
 * real PS2 hardware (cross-checked against PCSX2's pcsx2/Hw.h), but
 * this file only stores/returns register values - it does not
 * execute any actual DMA transfers yet.
 */

#include "core/hw/dma.h"
#include "core/hw/ee_intc.h"
#include "core/hw/ipu.h"
extern uint32_t ipu_input_write(const uint8_t *,uint32_t) __attribute__((weak));
extern uint32_t ipu_output_read(uint8_t *,uint32_t) __attribute__((weak));
extern void ipu_service(void) __attribute__((weak));
#include <string.h>

/* Local alias, matching the existing per-file-local-constant convention
 * used by source/hw/sif.c and source/hw/iop_icfg.c for this same value. */
#define EE_INTC_IRQ_SBUS 1
#ifdef R933_DMA_KICK_TRACE
#include <stdio.h>
#endif
#ifdef R1132_WRITE_WATCH
#include <stdio.h>
#endif

static dma_state_t g_dma;
static uint8_t *g_ee_ram = NULL;
static uint32_t g_ee_ram_size = 0;
/* R1316: optional JIT source-generation observer. Kept as a callback so
 * standalone DMA tests retain zero dependency on the EE core/recompiler. */
static void (*g_ee_write_notify)(uint32_t phys_addr, uint32_t len) = NULL;
/* Round 572: bound the same way g_ee_ram is (see dma_bind_ee_ram() /
 * dma_bind_scratchpad() below) rather than calling ee_core_get_state()
 * directly - dma.c must stay linkable standalone (several test source
 * files #include this file directly without linking ee_core.o at all;
 * a hard call-time dependency on ee_core_get_state() broke every one
 * of them with an undefined-reference link error the first time this
 * was tried). ee_core_init() wires this in exactly like it already
 * does for g_ee_ram. */
static uint8_t *g_ee_scratch = NULL;
static uint32_t g_ee_scratch_size = 0;
static dma_sink_fn g_sinks[DMA_CHANNEL_COUNT];
static dma_sink_fn g_tag_sinks[DMA_CHANNEL_COUNT];

void dma_set_tag_sink(int channel, dma_sink_fn fn)
{
    if (channel >= 0 && channel < DMA_CHANNEL_COUNT) g_tag_sinks[channel] = fn;
}

void dma_init(void)
{
    memset(&g_dma, 0, sizeof(g_dma));
    memset(g_sinks, 0, sizeof(g_sinks));
    memset(g_tag_sinks, 0, sizeof(g_tag_sinks));
    /* Deliberately not clearing g_ee_ram/g_ee_ram_size here - dma_init()
     * resets register state on emulator (re)start, but the RAM binding
     * is a one-time wiring done by ee_core_init() at a different point
     * in the boot sequence. */

    /* Round 539: real hardware/PCSX2 hwReset() sets DMAC_ENABLER and
     * DMAC_ENABLEW to 0x1201 at boot/reset (see dma.h's doc comment on
     * d_enable_state for the full citation) - NOT zero, unlike every
     * other register memset above. */
    g_dma.d_enable_state = 0x1201u;
}

dma_state_t *dma_get_state(void) { return &g_dma; }

void dma_bind_ee_ram(uint8_t *ram, uint32_t ram_size)
{
    g_ee_ram = ram;
    g_ee_ram_size = ram_size;
}

/* Round 572 (task #536/#547): binds the EE's 16KB on-chip scratchpad
 * (the SAME buffer ee_core.c's ee_mem_ptr() exposes to CPU loads/
 * stores at KUSEG 0x70000000-0x70003FFF, ee_state_t.scratch) so
 * dma_resolve_ptr() can route SPR-flagged DMA addresses (real
 * hardware's tDMAC_ADDR bit 31 - see dma_resolve_ptr()'s doc comment)
 * to it instead of misreading them as wild main-RAM offsets. Mirrors
 * dma_bind_ee_ram() exactly, including the "may never be called"
 * safety: dma_resolve_ptr() checks g_ee_scratch for NULL before use,
 * same as every other binding in this file. */
void dma_bind_scratchpad(uint8_t *scratch, uint32_t scratch_size)
{
    g_ee_scratch = scratch;
    g_ee_scratch_size = scratch_size;
}

void dma_set_ee_write_notify(void (*fn)(uint32_t phys_addr, uint32_t len))
{
    g_ee_write_notify = fn;
}

void dma_set_sink(int channel, dma_sink_fn fn)
{
    if (channel >= 0 && channel < DMA_CHANNEL_COUNT)
        g_sinks[channel] = fn;
}

#define EE_SCRATCH_SIZE (16u * 1024u)

/* Resolves a raw 32-bit DMA address register value (MADR/TADR, or a
 * chain tag's own ADDR word) to a host pointer, per real hardware's
 * tDMAC_ADDR/tDMA_TAG bitfield layout (PCSX2's Dmac.h, vendored at
 * docs/reference/pcsx2/pcsx2/Dmac.h): bits 0-30 are the real address,
 * bit 31 is the SPR (scratchpad) selector - "Memory/SPR Address (only
 * effective for MADR and TADR of non-SPR DMAs)". When SPR is set the
 * low bits address the EE's 16KB on-chip scratchpad - the SAME buffer
 * ee_core.c's ee_mem_ptr() already exposes to CPU loads/stores at
 * KUSEG 0x70000000-0x70003FFF (ee_state_t.scratch) - which is a
 * completely different memory than main RAM, not an offset within it.
 *
 * Round 572 (task #536/#547): found via a live diskless-boot trace
 * that real BIOS code writes VIF1's TADR as an address with bit 31
 * set (observed: 0x80002290) mid-chain. Before this fix, every DMA
 * address (MADR/TADR/tag-ADDR) was used as a flat, unmasked 32-bit
 * offset straight into g_ee_ram - bit 31 alone is ~2GB, always
 * failing the g_ee_ram_size (32MB) bound check. The chain-walk loop's
 * only response to that failure is a silent `break` that leaves the
 * channel's STR bit set and never signals completion (see
 * dma_channel_kick()) - so the channel looks "still busy" forever,
 * and every later MMIO-triggered re-kick just re-fails at the exact
 * same stuck TADR, doing nothing. This was the actual, precise reason
 * VIF1's real DMA traffic (tens of thousands of real kicks) never
 * delivered enough real data to ever reach an MSCAL/MSCNT VIFcode. */
static int dma_resolve_ptr(uint32_t raw_addr, uint32_t len, uint8_t **out)
{
    uint32_t addr = raw_addr & 0x7FFFFFFFu;
    if (raw_addr & 0x80000000u) {
        if (!g_ee_scratch)
            return 0;
        uint32_t off = addr & (EE_SCRATCH_SIZE - 1u);
        if (off > g_ee_scratch_size || len > g_ee_scratch_size - off || len > EE_SCRATCH_SIZE - off)
            return 0;
        *out = g_ee_scratch + off;
        return 1;
    }
    if (!g_ee_ram || addr > g_ee_ram_size || len > g_ee_ram_size - addr)
        return 0;
    *out = g_ee_ram + addr;
    return 1;
}

/* Little-endian-explicit RAM access - same reasoning as ee_core.c and
 * iop_core.c: PS2 memory is little-endian, our Wii/PowerPC build
 * target is big-endian, so this can't be a raw memcpy. */
static int ram_read32(uint32_t raw_addr, uint32_t *out)
{
    uint8_t *p;
    if (!dma_resolve_ptr(raw_addr, 4, &p))
        return 0;
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return 1;
}

static int ram_ptr(uint32_t raw_addr, uint32_t len, const uint8_t **out)
{
    uint8_t *p;
    if (!dma_resolve_ptr(raw_addr, len, &p))
        return 0;
    *out = p;
    return 1;
}

/* D_CTRL.MFD selects VIF1 (2) or GIF (3), per Dmac.h. A drain must
 * not interpret unwritten ring bytes as a REFE completion. */
static int mfifo_channel(void)
{
    unsigned mfd=(g_dma.d_ctrl>>2)&3u;
    return mfd==2u?DMA_CHANNEL_VIF1:mfd==3u?DMA_CHANNEL_GIF:-1;
}
static int mfifo_valid(void)
{
    uint64_t size=(uint64_t)g_dma.d_rbsr+16u;
    return mfifo_channel()>=0 && g_dma.d_rbsr && !(g_dma.d_rbor&15u) &&
           !(g_dma.d_rbsr&15u) && !(size&(size-1u)) &&
           (uint64_t)g_dma.d_rbor+size<=g_ee_ram_size;
}
static uint32_t mfifo_wrap(uint32_t addr)
{
    return g_dma.d_rbor+((addr-g_dma.d_rbor)&g_dma.d_rbsr);
}
static int mfifo_contains(uint32_t addr)
{
    return addr>=g_dma.d_rbor && (uint64_t)addr<(uint64_t)g_dma.d_rbor+g_dma.d_rbsr+16u;
}
static uint32_t mfifo_available(uint32_t addr)
{
    return ((mfifo_wrap(g_dma.chan[DMA_CHANNEL_FROMSPR].madr)-mfifo_wrap(addr))&g_dma.d_rbsr)/16u;
}

/* Transfers 'qwc' quadwords (16 bytes each) starting at physical
 * address 'addr' to the channel's registered sink (if any). Returns 1
 * on success, 0 if the range falls outside bound RAM. */
static int transfer_quadwords(int channel, uint32_t addr, uint32_t qwc)
{
    /* SPR channels copy between EE RAM and the 16 KiB scratchpad,
     * rather than sending bytes to a peripheral sink. SADR wraps. */
    if (channel == DMA_CHANNEL_TOSPR || channel == DMA_CHANNEL_FROMSPR) {
        dma_channel_t *ch = &g_dma.chan[channel];
        uint32_t phys = addr & 0x1fffffffu;
        uint64_t bytes = (uint64_t)qwc * 16u;
        int ring=channel==DMA_CHANNEL_FROMSPR && mfifo_valid();
        if (!g_ee_ram || !g_ee_scratch || g_ee_scratch_size < EE_SCRATCH_SIZE ||
            (!ring && (uint64_t)phys + bytes > g_ee_ram_size)) return 0;
        uint32_t spr = ch->sadr & (EE_SCRATCH_SIZE - 16u);
        for (uint32_t i=0; i<qwc; i++) {
            if (channel == DMA_CHANNEL_TOSPR) {
                memcpy(g_ee_scratch + spr, g_ee_ram + phys + i*16u, 16u);
            } else {
                uint32_t dst = ring ? mfifo_wrap(phys + i*16u) : phys + i*16u;
                memcpy(g_ee_ram + dst, g_ee_scratch + spr, 16u);
                if (g_ee_write_notify) g_ee_write_notify(dst, 16u);
            }
            spr = (spr + 16u) & (EE_SCRATCH_SIZE - 1u);
        }
        ch->sadr = spr;
        ch->quadwords_transferred += qwc;
        return 1;
    }
    if (qwc == 0)
        return 1;
    if(channel==mfifo_channel() && mfifo_valid() && mfifo_contains(addr)) {
        for(uint32_t i=0;i<qwc;i++) {
            const uint8_t *q;
            if(!ram_ptr(mfifo_wrap(addr+i*16u),16u,&q))return 0;
            if(g_sinks[channel])g_sinks[channel](channel,q,1u);
        }
        g_dma.chan[channel].quadwords_transferred+=qwc;
        return 1;
    }
    const uint8_t *p;
    if (!ram_ptr(addr, qwc * 16u, &p))
        return 0;
    if (g_sinks[channel])
        g_sinks[channel](channel, p, qwc);
    g_dma.chan[channel].quadwords_transferred += qwc;
    return 1;
}

/* Reads one 64-bit DMA chain tag (2 words: control word with QWC/ID/
 * IRQ, then the ADDR word) from physical address 'tag_addr'. Matches
 * PCSX2's tDMA_TAG bitfield layout (Dmac.h): QWC in bits 0-15, PCE in
 * 26-27, ID in 28-30, IRQ in bit 31 of the control word; the ADDR word
 * is itself a tDMAC_ADDR (ADDR:31, SPR:1 - see dma_resolve_ptr()'s doc
 * comment above). Round 572: *addr now preserves the raw word
 * (including bit 31/SPR) instead of masking it away here - a
 * NEXT-type tag can legitimately continue the chain into scratchpad,
 * and the caller stores this value straight into ch->tadr, which
 * dma_resolve_ptr() (via ram_read32()/ram_ptr()) already knows how to
 * interpret correctly on its own next use. */
static int read_chain_tag(uint32_t tag_addr, uint32_t *qwc, uint32_t *id,
                           uint32_t *addr, uint32_t *irq)
{
    uint32_t ctrl, addr_word;
    if (!ram_read32(tag_addr, &ctrl)) return 0;
    if (!ram_read32(tag_addr + 4, &addr_word)) return 0;

    *qwc  = ctrl & 0xFFFFu;
    *id   = (ctrl >> 28) & 0x7u;
    *irq  = (ctrl >> 31) & 0x1u;
    *addr = addr_word;
    return 1;
}

/* R1335: independent bounded FIFO transfers, invoked by the IPU service.
 * Do not use the old void sink: it cannot return partial consumption. */
int dma_ipu_service(void)
{
 if(!ipu_input_write||!ipu_output_read)return 0;
 int progress=0;
 for(int channel=DMA_CHANNEL_FROMIPU;channel<=DMA_CHANNEL_TOIPU;channel++) {
  dma_channel_t *ch=&g_dma.chan[channel];
  if(!(ch->chcr&0x100u))continue;
  unsigned mod=(ch->chcr>>2)&3;
  if(mod>1||(channel==DMA_CHANNEL_FROMIPU&&mod)) {
   ch->last_error=DMA_ERR_UNSUPPORTED_TAG;ch->chcr&=~0x100u;continue;
  }
  if(channel==DMA_CHANNEL_TOIPU&&mod==1&&!g_dma.ipu_tag_pending) {
   uint32_t qwc,id,addr,irq;
   if(!read_chain_tag(ch->tadr,&qwc,&id,&addr,&irq))goto bad_address;
   uint32_t next=ch->tadr+16, payload=next;
   g_dma.ipu_tag_end=(id==DMA_TAG_REFE||id==DMA_TAG_END||((ch->chcr&0x80u)&&irq));
   if(id==DMA_TAG_REFE||id==DMA_TAG_REF||id==DMA_TAG_REFS)payload=addr;
   else if(id==DMA_TAG_CNT||id==DMA_TAG_END)next+=qwc*16;
   else if(id==DMA_TAG_NEXT)next=addr;
   else if(id==DMA_TAG_CALL){
    unsigned asp=(ch->chcr>>4)&3;
    if(asp>=2){ch->last_error=DMA_ERR_UNSUPPORTED_TAG;ch->chcr&=~0x100u;continue;}
    if(asp==0)ch->asr0=payload+qwc*16;else ch->asr1=payload+qwc*16;
    ch->chcr=(ch->chcr&~0x30u)|((asp+1)<<4);next=addr;
   }else if(id==DMA_TAG_RET){
    unsigned asp=(ch->chcr>>4)&3;
    if(!asp||asp>2){g_dma.ipu_tag_end=1;next=payload+qwc*16;}
    else{next=asp==2?ch->asr1:ch->asr0;ch->chcr=(ch->chcr&~0x30u)|((asp-1)<<4);}
   }
   ch->chcr=(ch->chcr&0xffffu)|((id<<28)|(irq<<31)) ;
   ch->madr=payload;ch->qwc=qwc;ch->tadr=next;g_dma.ipu_tag_pending=1;progress=1;
  }
  if(ch->qwc) {
   uint32_t wanted=ch->qwc>8?8:ch->qwc,done;uint8_t *p;
   if(!dma_resolve_ptr(ch->madr,wanted*16,&p))goto bad_address;
   if(channel==DMA_CHANNEL_TOIPU)done=ipu_input_write(p,wanted);
   else {
    done=ipu_output_read(p,wanted);
    if(done&&g_ee_write_notify&&!(ch->madr&0x80000000u))g_ee_write_notify(ch->madr,done*16);
   }
   ch->madr+=done*16;ch->qwc-=done;ch->quadwords_transferred+=done;
   progress|=done!=0;
  }
  if(!ch->qwc) {
   if(channel==DMA_CHANNEL_TOIPU&&mod==1){g_dma.ipu_tag_pending=0;if(!g_dma.ipu_tag_end)continue;}
   ch->chcr&=~0x100u;dma_channel_signal_done(channel);progress=1;
  }
  continue;
 bad_address:
  ch->last_error=DMA_ERR_OUT_OF_BOUNDS;ch->chcr&=~0x100u;
  if(channel==DMA_CHANNEL_TOIPU)g_dma.ipu_tag_pending=0;
 }
 return progress;
}

void dma_channel_kick(int channel)
{
    if (channel < 0 || channel >= DMA_CHANNEL_COUNT)
        return;

    dma_channel_t *ch = &g_dma.chan[channel];
    ch->last_error = DMA_ERR_NONE;

    if (!g_ee_ram) {
        ch->last_error = DMA_ERR_NO_RAM_BOUND;
        ch->chcr &= ~0x100u; /* clear STR - can't run */
        return;
    }

    if ((channel==DMA_CHANNEL_TOIPU||channel==DMA_CHANNEL_FROMIPU)&&ipu_service) {ipu_service();return;}
    uint32_t mod = (ch->chcr >> 2) & 0x3u;
#ifdef R933_DMA_KICK_TRACE
    fprintf(stderr, "[R933DMA] kick channel=%d chcr=0x%08x madr=0x%08x qwc=%u tadr=0x%08x mod=%u sink=%p\n",
            channel, ch->chcr, ch->madr, ch->qwc, ch->tadr, mod, (void*)g_sinks[channel]);
#endif

    if (mod == 0) {
        /* NORMAL mode: one shot, QWC quadwords straight from MADR. */
        if (!transfer_quadwords(channel, ch->madr, ch->qwc)) {
            ch->last_error = DMA_ERR_OUT_OF_BOUNDS;
        } else {
            ch->madr += ch->qwc * 16u;
            if(channel==DMA_CHANNEL_FROMSPR && mfifo_valid())ch->madr=mfifo_wrap(ch->madr);
            ch->qwc = 0;
        }
        ch->chcr &= ~0x100u; /* transfer complete - clear STR */
        dma_channel_signal_done(channel); /* task #176: real hwDmacIrq(n) equivalent */
        /* Publishing actual producer bytes can restart a waiting drain. */
        int drain=mfifo_channel();
        if(channel==DMA_CHANNEL_FROMSPR && ch->last_error==DMA_ERR_NONE && mfifo_valid() && drain>=0 && (g_dma.chan[drain].chcr&0x100u))dma_channel_kick(drain);
        return;
    }

    if (mod == 1) {
        /* CHAIN mode: walk tags starting at TADR. Implements the four
         * most common tag IDs (REFE/CNT/NEXT/END); REF/REFS/CALL/RET
         * are flagged as unsupported rather than silently mishandled -
         * see docs/ROADMAP.md. */
        const int MAX_TAGS_PER_KICK = 4096; /* guards against a corrupt/cyclic chain hanging us forever */
        for (int guard = 0; guard < MAX_TAGS_PER_KICK; guard++) {
            uint32_t qwc, id, addr, irq;
            int ring=channel==mfifo_channel() && mfifo_valid() && mfifo_contains(ch->tadr);
            uint32_t available=ring?mfifo_available(ch->tadr):0;
            if(ring && !available)return; /* STR stays set, no completion IRQ. */
            (void)irq;
            if (!read_chain_tag(ch->tadr, &qwc, &id, &addr, &irq)) {
                ch->last_error = DMA_ERR_OUT_OF_BOUNDS;
                break;
            }

            if(ring && (id==DMA_TAG_CNT || id==DMA_TAG_NEXT || id==DMA_TAG_END || id==5u || id==6u) && available<qwc+1u)return;

            /* CHCR.TTE sends the tag's upper two words to VIF before
             * the associated data, including tags with QWC=0. */
            if ((ch->chcr & 0x40u) && g_tag_sinks[channel]) {
                const uint8_t *tag_data;
                if (!ram_ptr(ch->tadr + 8u, 8u, &tag_data)) {
                    ch->last_error = DMA_ERR_OUT_OF_BOUNDS;
                    ch->chcr &= ~0x100u;
                    return;
                }
                g_tag_sinks[channel](channel, tag_data, 2u);
            }

            switch (id) {
            case DMA_TAG_REFE: /* 0: data at ADDR, then stop */
                if (!transfer_quadwords(channel, addr, qwc)) { ch->last_error = DMA_ERR_OUT_OF_BOUNDS; }
                ch->tadr += 16u;
                if(ring)ch->tadr=mfifo_wrap(ch->tadr);
                ch->chcr &= ~0x100u;
                dma_channel_signal_done(channel); /* task #176 */
                return;

            case DMA_TAG_CNT: /* 1: data follows the tag itself, keep going */
                if (!transfer_quadwords(channel, ring?mfifo_wrap(ch->tadr + 16u):ch->tadr + 16u, qwc)) { ch->last_error = DMA_ERR_OUT_OF_BOUNDS; return; }
                ch->tadr = ch->tadr + 16u + qwc * 16u;
                if(ring)ch->tadr=mfifo_wrap(ch->tadr);
                continue;

            case DMA_TAG_NEXT: /* 2: data follows the tag, next tag is at ADDR */
                if (!transfer_quadwords(channel, ring?mfifo_wrap(ch->tadr + 16u):ch->tadr + 16u, qwc)) { ch->last_error = DMA_ERR_OUT_OF_BOUNDS; return; }
                ch->tadr = ring?mfifo_wrap(addr):addr;
                continue;

            case DMA_TAG_END: /* 7: data follows the tag, then stop */
                if (!transfer_quadwords(channel, ring?mfifo_wrap(ch->tadr + 16u):ch->tadr + 16u, qwc)) { ch->last_error = DMA_ERR_OUT_OF_BOUNDS; }
                ch->tadr += 16u + qwc * 16u;
                if(ring)ch->tadr=mfifo_wrap(ch->tadr);
                ch->chcr &= ~0x100u;
                dma_channel_signal_done(channel); /* task #176 */
                return;

            case DMA_TAG_REF: /* 3 */
            case DMA_TAG_REFS: /* 4 - task #447/#521 (Round 553): real PCSX2 source
                 * (docs/reference/pcsx2/pcsx2/Hw.cpp's hwDmacSrcChainWithStack(),
                 * cross-checked against Dmac.h's own tag_id comments) treats REF
                 * and REFS identically for basic chain-walk purposes: "Transfer
                 * QWC from ADDR field" (i.e. the SAME transfer_quadwords(addr,qwc)
                 * shape this file already uses for REFE, just without stopping),
                 * then "Set TADR to next tag" (tadr += 16) and continue the walk
                 * (hwDmacSrcChainWithStack returns false = not-done for both,
                 * unlike REFE's true = done). REFS's real difference from REF is
                 * STADR-based "stall control" (an unrelated flow-control feature,
                 * not modeled here, same honest scope as this file's existing
                 * "no chain-mode stall control" gap) - the basic data-transfer and
                 * chain-advancement behavior this fix needs is identical for both,
                 * so both are handled together rather than leaving REFS on the
                 * unsupported path this fix specifically closes for REF.
                 *
                 * Found via a real, reproducible host-native trace (task #447's
                 * Round 552 fast-boot-patch scratch driver): EELOAD's own real,
                 * unmodified code - reached organically for the first time via
                 * Round 552's "rom0:OSDSYS"-string patch, no register hijacking
                 * involved - calls a real SIF0 chain-mode DMA send (via a function
                 * at 0x00083fd0, called right after a real CreateSema) whose real
                 * tag chain includes a REF tag (id=3) this project had never
                 * implemented; the previous default: path aborted the transfer
                 * (DMA_ERR_UNSUPPORTED_TAG, STR cleared, no completion signal),
                 * which is the most likely reason the real completion interrupt
                 * that should eventually let EELOAD's own code SignalSema() the
                 * semaphore it is WaitSema()-blocked on never fires. */
                if (!transfer_quadwords(channel, addr, qwc)) { ch->last_error = DMA_ERR_OUT_OF_BOUNDS; return; }
                ch->tadr += 16u;
                if(ring)ch->tadr=mfifo_wrap(ch->tadr);
                continue;

            default: /* CALL/RET - not implemented (no evidence yet this project's
                 * traces ever need the address-stack mechanism these two tag
                 * types require; left honestly unsupported per task #447/#521's
                 * evidence-only-implement standard, unlike REF/REFS above which
                 * were directly observed and confirmed against real PCSX2 source). */
                ch->last_error = DMA_ERR_UNSUPPORTED_TAG;
                ch->chcr &= ~0x100u;
                return;
            }
        }
        /* Fell out of the loop via the guard counter - treat as an error
         * rather than leaving STR set forever. */
        ch->last_error = DMA_ERR_UNSUPPORTED_TAG;
        ch->chcr &= ~0x100u;
        return;
    }

    /* INTERLEAVE mode (SPR only) - not implemented. */
    ch->last_error = DMA_ERR_UNSUPPORTED_TAG;
    ch->chcr &= ~0x100u;
}

/*
 * Task #176: sets DMAC_STAT's low (status) bit for `channel` - see
 * the doc comment on this function in dma.h and PCSX2's hwDmacIrq(n)
 * in Hw.cpp (`psHu32(DMAC_STAT) |= 1<<n`). d_stat's layout: bits 0-9
 * are per-channel status (this function only ever sets one of
 * those), bits 16-25 are the per-channel enable mask written via
 * dma_mmio_write32's special-cased DMAC_STAT toggle-on-write-1 path.
 */
void dma_channel_signal_done(int channel)
{
    if (channel < 0 || channel >= DMA_CHANNEL_COUNT)
        return;
    g_dma.d_stat |= (1u << channel);
}

/*
 * Round 198 (task #365): the missing inbound (device -> EE RAM)
 * write capability - see the doc comment on this function's
 * declaration in dma.h for the full citation trail and honest scope
 * note. Mirrors transfer_quadwords()'s contract exactly, just in the
 * reverse direction: writes to the channel's own MADR instead of
 * reading from it, using a plain byte-for-byte copy (endian-safe
 * regardless of host CPU, same reasoning as ram_ptr()'s existing raw
 * uint8_t* contract for the outbound side - no multi-byte integer
 * interpretation happens here, so the PPC-host/little-endian-PS2-
 * data mismatch that requires explicit byte assembly in ram_read32()
 * simply doesn't apply to a raw copy).
 */
int dma_channel_receive_quadwords(int channel, const uint8_t *data, uint32_t qwc)
{
    if (channel < 0 || channel >= DMA_CHANNEL_COUNT)
        return 0;
    if (!g_ee_ram)
        return 0;
    if (qwc == 0)
        return 1;

    dma_channel_t *ch = &g_dma.chan[channel];
    uint32_t len = qwc * 16u;
    if ((uint64_t)ch->madr + (uint64_t)len > (uint64_t)g_ee_ram_size)
        return 0;

#ifdef R1132_WRITE_WATCH
    {
        uint32_t lo = 0x00023FC8u, hi = 0x00023FCCu + 4u;
        if (ch->madr < hi && (ch->madr + len) > lo) {
            fprintf(stderr, "[R1132_WRITE_WATCH] who=dma_channel_receive_quadwords channel=%d madr=0x%08x len=%u qwc=%u\n",
                    channel, ch->madr, len, qwc);
        }
    }
#endif
    uint32_t written_madr = ch->madr;
    memcpy(g_ee_ram + written_madr, data, len);
    if (g_ee_write_notify) g_ee_write_notify(written_madr, len);
    ch->madr += len;
    ch->qwc = (ch->qwc > qwc) ? (ch->qwc - qwc) : 0u;
    ch->quadwords_transferred += qwc;

    /* R1200: an inbound DMA completion is still a DMA completion.
     * The normal/chain outbound engine clears CHCR.STR (bit 8) before
     * raising DMAC_STAT, but the inbound helper historically left STR
     * set forever. R1199 caught SIF0 at CHCR=0x184 even after the 3-QWC
     * REND had completed (MADR advanced 0x935c0 -> 0x935f0, QWC=0).
     * That exposes the channel as permanently busy to the real BIOS.
     * Clear STR before signaling completion, matching dma_channel_kick(). */
    ch->chcr &= ~0x100u;
    dma_channel_signal_done(channel); /* real DMAC_STAT completion status */

    /* R1203: inbound SIF0 completion must also assert the EE SBUS INTC
     * source.  dma_channel_signal_done() only sets DMAC_STAT; despite
     * older comments claiming otherwise it does not call ee_intc_raise().
     * The bookkeeping-only dma_channel_note_reply_delivered() path below
     * already performs this exact SIF0 -> SBUS coupling, but the real
     * receive-DMA helper introduced later never inherited it.  R1202
     * proved three byte-correct REND DMAs completed with STR clear while
     * none correlated with the BIOS SBUS handler.  Raise the hardware
     * completion source here, generically for every real inbound SIF0 DMA. */
    if (channel == DMA_CHANNEL_SIF0)
        ee_intc_raise_sbus_event();
    return 1;
}

/*
 * Round 225 (task #366/#172, 265th finding): see the doc comment in
 * dma.h above this function's declaration for full grounding. Mirrors
 * dma_channel_receive_quadwords()'s real bookkeeping exactly, minus
 * the byte copy (the caller already performed it directly via
 * ee_mem_write32, matching this project's existing SIF-RPC reply
 * pattern) and targeting the caller-supplied dest_addr instead of the
 * channel's pre-existing MADR.
 */
void dma_channel_note_reply_delivered(int channel, uint32_t dest_addr, uint32_t nbytes)
{
    if (channel < 0 || channel >= DMA_CHANNEL_COUNT)
        return;

    dma_channel_t *ch = &g_dma.chan[channel];
    uint32_t qwc = (nbytes + 15u) / 16u; /* round up to whole quadwords - real DMA transfer granularity */

    ch->madr = dest_addr + qwc * 16u;
    ch->qwc = (ch->qwc > qwc) ? (ch->qwc - qwc) : 0u;
    ch->quadwords_transferred += qwc;

    dma_channel_signal_done(channel);

    /* Round 1096 (task #447/#536, GT3 disc-boot track): every genuine
     * SIF0 completion is a real EE-side SBUS event on real hardware
     * (see PCSX2 Sif.cpp / ps2sdk sifdma.c - the EE's SBUS IRQ is what
     * lets BIOS/game code waiting via SIF_SMFLAG-poll or the EE INTC
     * SBUS bit actually observe "a reply arrived"). Rounds 263/317/
     * 1021/1076 each hard-coded this at one specific poll PC
     * (0x8000CFCC, 0x8000FD74, 0x8000FE2C) as separate one-off
     * shortcuts, because at the time only those exact addresses were
     * known to matter. That doesn't scale - GT3 (Round 1075/1076)
     * proved a single poll site can be visited more than once, and a
     * fourth/fifth pinned address is not a real fix. This raises
     * SBUS generically, from the one real place completions are
     * actually delivered, exactly like real hardware does it.
     * Verified (Round 1096, scratch tree only until this commit):
     *  - fires correctly for all real SIF0 deliveries across a fresh
     *    GT3 cold boot (sif0_replies=4, matching the real bind-reply
     *    traffic already characterized in Rounds 1071-1075, with the
     *    last one at ee_pc=0x80000358 - the exact SignalSema(5) site
     *    Round 1075 already proved is the real CDVD_INIT completion);
     *  - a 240M-instruction non-regression window shows byte-identical
     *    final resting state to the unpatched tree (ee_pc=0x8000fe24) -
     *    honestly, this alone does not push GT3 further, because no
     *    5th SIF0 completion is ever produced in this window (the
     *    remaining gap is upstream: nothing issues another outbound
     *    SIF-RPC request), not because this raise is wrong or unused.
     * Shipped anyway because it is a strictly more correct/general
     * replacement for the 3 existing address-pinned shortcuts, with
     * no observed regression. */
    if (channel == DMA_CHANNEL_SIF0)
        ee_intc_raise_sbus_event();
}

void dma_channel_set_irq_enable(int channel, int enabled)
{
    if (channel < 0 || channel >= DMA_CHANNEL_COUNT)
        return;
    uint32_t bit = 1u << (16 + channel);
    if (enabled)
        g_dma.d_stat |= bit;
    else
        g_dma.d_stat &= ~bit;
}

int dma_dmac_interrupt_pending(void)
{
    /* Real hardware/PCSX2 dmacInterrupt() (Hw.cpp): pending if
     * (status_low & enable_high) != 0, or status_low bit 15 (BEIS,
     * bus-error/stall-detect - never set by this project, no bus
     * errors modeled, but checked here for completeness/fidelity)
     * is set - AND the DMAC is actually enabled (DMAC_CTRL.DMAE,
     * bit 0 of d_ctrl) AND not suspended (Round 539: real hardware's
     * DMAC_ENABLER "suspended" byte, `psHu8(DMAC_ENABLER+2) == 1` -
     * previously left out as a documented, deliberately-not-fabricated
     * gap; now modeled for real via d_enable_state, see dma.h). */
    uint32_t status_low = g_dma.d_stat & 0xFFFFu;
    uint32_t enable_high = (g_dma.d_stat >> 16) & 0xFFFFu;
    if (!(g_dma.d_ctrl & 0x1u))
        return 0; /* DMAC_CTRL.DMAE == 0: master DMA enable is off */
    if (((g_dma.d_enable_state >> 16) & 0xFFu) == 0x01u)
        return 0; /* DMAC_ENABLER byte+2 == 1: DMAC suspended */
    if ((status_low & enable_high) != 0u)
        return 1;
    if (status_low & 0x8000u)
        return 1;
    return 0;
}

#define DMAC_BASE       0x10008000u
#define DMAC_CHAN_END   0x1000E000u
#define DMAC_CTRL_BASE  0x1000E000u
#define DMAC_CTRL_END   0x1000F000u
#define CHAN_BLOCK_SIZE 0x400u

/* Explicit (base, size, channel) table - VIF0/VIF1/GIF each get a
 * full 0x1000-byte block, but fromIPU/toIPU and the SIFx/SPR channels
 * pack two channels into the same 0x1000 region (0x400 bytes each),
 * so this can't be resolved by masking to one fixed block size -
 * must check explicit ranges, smallest/most-specific first. */
typedef struct { uint32_t base, size; int channel; } dma_range_t;

static const dma_range_t s_ranges[] = {
    { 0x10008000u, 0x1000u, DMA_CHANNEL_VIF0 },
    { 0x10009000u, 0x1000u, DMA_CHANNEL_VIF1 },
    { 0x1000A000u, 0x1000u, DMA_CHANNEL_GIF },
    { 0x1000B000u, 0x0400u, DMA_CHANNEL_FROMIPU },
    { 0x1000B400u, 0x0400u, DMA_CHANNEL_TOIPU },
    { 0x1000C000u, 0x0400u, DMA_CHANNEL_SIF0 },
    { 0x1000C400u, 0x0400u, DMA_CHANNEL_SIF1 },
    { 0x1000C800u, 0x0400u, DMA_CHANNEL_SIF2 },
    { 0x1000D000u, 0x0400u, DMA_CHANNEL_FROMSPR },
    { 0x1000D400u, 0x0400u, DMA_CHANNEL_TOSPR },
};

static int decode_channel(uint32_t addr, uint32_t *reg_off)
{
    for (size_t i = 0; i < sizeof(s_ranges) / sizeof(s_ranges[0]); i++) {
        if (addr >= s_ranges[i].base && addr < s_ranges[i].base + s_ranges[i].size) {
            *reg_off = addr - s_ranges[i].base;
            return s_ranges[i].channel;
        }
    }
    return -1;
}

static uint32_t *channel_reg_ptr(dma_channel_t *c, uint32_t off)
{
    switch (off) {
    case 0x00: return &c->chcr;
    case 0x10: return &c->madr;
    case 0x20: return &c->qwc;
    case 0x30: return &c->tadr;
    case 0x40: return &c->asr0;
    case 0x50: return &c->asr1;
    case 0x80: return &c->sadr;
    default: return NULL;
    }
}

static uint32_t *ctrl_reg_ptr(uint32_t addr)
{
    switch (addr) {
    case 0x1000E000u: return &g_dma.d_ctrl;
    case 0x1000E010u: return &g_dma.d_stat;
    case 0x1000E020u: return &g_dma.d_pcr;
    case 0x1000E030u: return &g_dma.d_sqwc;
    case 0x1000E040u: return &g_dma.d_rbsr;
    case 0x1000E050u: return &g_dma.d_rbor;
    case 0x1000E060u: return &g_dma.d_stadr;
    default: return NULL;
    }
}

int dma_mmio_read32(uint32_t addr, uint32_t *out_val)
{
    if (addr >= DMAC_BASE && addr < DMAC_CHAN_END) {
        uint32_t off;
        int ch = decode_channel(addr, &off);
        if (ch < 0) { *out_val = 0; return 1; }
        uint32_t *reg = channel_reg_ptr(&g_dma.chan[ch], off);
        *out_val = reg ? *reg : 0;
        return 1;
    }
    if (addr >= DMAC_CTRL_BASE && addr < DMAC_CTRL_END) {
        uint32_t *reg = ctrl_reg_ptr(addr);
        *out_val = reg ? *reg : 0;
        return 1;
    }
    if (addr == 0x1000F520u || addr == 0x1000F590u) {
        /* Round 539: DMAC_ENABLER/DMAC_ENABLEW - see dma.h's
         * d_enable_state doc comment. Not adjacent to either range
         * above (they live in the same 0x1000F000-0x1000FFFF page as
         * INTC/SIF/MCH, at offset 0x520/0x590), so handled explicitly. */
        *out_val = g_dma.d_enable_state;
        return 1;
    }
    return 0;
}

int dma_mmio_write32(uint32_t addr, uint32_t val)
{
    if (addr >= DMAC_BASE && addr < DMAC_CHAN_END) {
        uint32_t off;
        int ch = decode_channel(addr, &off);
        if (ch < 0) return 1; /* consumed, ignored - unknown sub-register */
        uint32_t *reg = channel_reg_ptr(&g_dma.chan[ch], off);
        if (reg) {
            if(ch==DMA_CHANNEL_TOIPU&&off==0&&!(val&0x100u))g_dma.ipu_tag_pending=g_dma.ipu_tag_end=0;
            *reg = val;
            /* Writing CHCR (offset 0x00) with STR (bit 8) set kicks off
             * a real transfer - see dma_channel_kick(). */
            if (off == 0x00 && (val & 0x100u))
                dma_channel_kick(ch);
        }
        return 1;
    }
    if (addr == 0x1000E010u) {
        /* Task #176: DMAC_STAT real write semantics (PCSX2 HwWrite.cpp
         * dmacWrite32<>, case DMAC_STAT) - NOT a plain overwrite:
         * lower 16 bits (per-channel status) clear on write-1, upper
         * 16 bits (per-channel enable mask) toggle on write-1. See the
         * doc comment on d_stat's layout in dma.h. */
        uint32_t status_low = g_dma.d_stat & 0xFFFFu;
        uint32_t enable_high = (g_dma.d_stat >> 16) & 0xFFFFu;
        status_low &= ~(val & 0xFFFFu);
        enable_high ^= (val >> 16) & 0xFFFFu;
        g_dma.d_stat = status_low | (enable_high << 16);
        return 1;
    }
    if (addr >= DMAC_CTRL_BASE && addr < DMAC_CTRL_END) {
        uint32_t *reg = ctrl_reg_ptr(addr);
        if (reg) *reg = val;
        return 1;
    }
    if (addr == 0x1000F520u || addr == 0x1000F590u) {
        /* Round 539: DMAC_ENABLER/DMAC_ENABLEW - see dma.h's
         * d_enable_state doc comment. PCSX2 doesn't special-case a
         * toggle/mask here (unlike DMAC_STAT above) - it's a plain
         * store, matching real games' documented use (write 0 or
         * 0xFFFFFFFF wholesale, not per-bit twiddling). */
        g_dma.d_enable_state = val;
        return 1;
    }
    return 0;
}
