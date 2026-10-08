#ifndef PCSX2WII_GS_MEM_H
#define PCSX2WII_GS_MEM_H

#include <stdint.h>

/* GS local memory: one shared 4 MiB buffer, stored in PS2 little-endian
 * byte order. PSMCT32/24 use native 64x32 pages, 8x8 blocks and 8x2
 * columns; PSMCT16/16S use native 64x64 pages, 16x8 blocks and 16x2
 * columns. The 16/32-bit views alias exactly, which is required when
 * OSDSYS uploads one format and samples another. Callers convert BP
 * register units to word offsets; BW is measured in pixels.
 * Dedicated depth and indexed-format addressing remains separate work. */

#define GS_MEM_SIZE (4 * 1024 * 1024)

/* Single-threaded deferred GPU ownership. Resolver must complete GPU work
 * and write PS2 little-endian/swizzled bytes through the supplied raw buffer.
 * It must not call GS accessors or enqueue new rendering work. A final
 * readback copy is allowed when completed before returning. On failure ownership
 * stays pending and CPU access fails closed. No GPU renderer is implicit. */
typedef int (*gs_mem_gpu_resolver)(void *opaque, uint8_t *vram, uint32_t size);
int gs_mem_gpu_bind(gs_mem_gpu_resolver resolver, void *opaque);
int gs_mem_gpu_mark_pending(void);
/* Owner may allow bounded CPU reads/writes outside this conservative physical
 * envelope. Resolver must modify only bytes inside that envelope. Overlapping
 * access and unbounded raw access resolve first. New binding clears the guard. */
int gs_mem_gpu_protect_range(uint32_t lo,uint32_t hi);
/* Bounded read-only access. NULL on invalid input/failed ownership transfer.
 * Do not retain across guest writes or a GPU ownership change. */
const uint8_t *gs_mem_read_range(uint32_t off,uint32_t bytes);
int gs_mem_hash_range(uint32_t off,uint32_t bytes,uint64_t *hash);
int gs_mem_sync(void);
uint32_t gs_mem_gpu_pending(void);
uint32_t gs_mem_sync_failures(void);
void gs_mem_init(void);
uint8_t *gs_mem_get(void); /* Synchronized raw buffer; NULL on failure. Do not retain across GPU work. */

/* bp: base pointer, bw: buffer width in pixels, x/y: pixel
 * coordinates. NOTE: real GS registers (FBP/FBW) encode these in
 * hardware-specific units (blocks-of-32, pixels/64, etc. - see
 * GS/GSRegs.h Block()/FBW fields) - converting real register values
 * into plain word offsets and pixel counts before calling these
 * functions is the caller's job, not handled here. */
uint32_t gs_mem_read_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
/* Exact contiguous PSMCT32 scanout; count outputs, current shared VRAM. */
void gs_mem_read_psmct32_span(uint32_t *out,uint32_t bp,uint32_t bw,
                             uint32_t x,uint32_t y,uint32_t count);
void     gs_mem_write_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba);

/* TEX0/CLUT/BITBLTBUF pointers use 256-byte blocks (64 words). */
static inline uint32_t gs_mem_read_psmct32_blk(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    extern uint32_t gs_mem_read_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
    return gs_mem_read_psmct32(bp * 64u, bw, x, y);
}
static inline void gs_mem_write_psmct32_blk(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba)
{
    extern void gs_mem_write_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba);
    gs_mem_write_psmct32(bp * 64u, bw, x, y, rgba);
}

/* Compatibility API: BP is measured in 8192-byte pages here.
 * It uses the same native addressing as the word-offset API above. */
uint32_t gs_mem_swizzle_addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
uint32_t gs_mem_read_psmct32_swizzled(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
void     gs_mem_write_psmct32_swizzled(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba);

uint16_t gs_mem_read_psmct16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
void gs_mem_write_psmct16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint16_t value);

uint16_t gs_mem_read_psmct16s(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
void gs_mem_write_psmct16s(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint16_t value);

/* Address-only helper for GPU resolver access; returns UINT32_MAX outside VRAM.
 * No ownership change or recursive synchronization. */
uint32_t gs_mem_z_offset(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm);
uint32_t gs_mem_read_z(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t psm);
void gs_mem_write_z(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t psm, uint32_t value);
uint32_t gs_mem_read_index(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm);
void gs_mem_write_index(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm,uint32_t value);
void gs_mem_fill_psmct32_span(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                             uint32_t count,uint32_t rgba);

#endif
