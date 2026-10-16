#ifndef PCSX2WII_GS_WII_OUTPUT_H
#define PCSX2WII_GS_WII_OUTPUT_H

#include <stdint.h>

/* CPU scanout from swizzled GS VRAM to Wii XFB, display-region decoding
 * and bounded source-color probes. GX presentation uses a separate path. */

/* xfb: pointer to a Wii external framebuffer (as set up by
 * SYS_AllocateFramebuffer / VIDEO_SetNextFramebuffer).
 * xfb_width_px: the *display* width in pixels (e.g. rmode->fbWidth) -
 * the XFB is stored as xfb_width_px/2 32-bit YUV words per scanline.
 * dst_x/dst_y: top-left destination position in the XFB, in pixels
 * (dst_x should be even - see note in gs_wii_output.c).
 * gs_bp/gs_bw/src_x/src_y: which region of GS memory to read from
 * (same conventions as gs_mem_read_psmct32).
 * width/height: size of the region to blit, in pixels. */
void gs_blit_psmct32_to_xfb(void *xfb, uint32_t xfb_width_px,
                             uint32_t dst_x, uint32_t dst_y,
                             uint32_t gs_bp, uint32_t gs_bw,
                             uint32_t src_x, uint32_t src_y,
                             uint32_t width, uint32_t height);

/* Exposed for unit testing - converts one pixel pair to a packed
 * Y1CbY2Cr word, ported verbatim (integer-for-integer) from libogc's
 * console.c RGB8_to_YCbCr. */
uint32_t gs_rgb8_pair_to_ycbcr(uint8_t r1, uint8_t g1, uint8_t b1,
                                uint8_t r2, uint8_t g2, uint8_t b2);

/* Round 119 (task #172/#274, GX/Wii output verification): decodes the
 * real PS2 GS hardware DISPFB1/DISPFB2 register field layout (well-
 * established, publicly documented GS register format) - FBP (frame
 * buffer pointer, bits 0-8, units of 2048 words) and FBW (frame
 * buffer width, bits 9-14, units of 64 pixels) - into this project's
 * own gs_mem.h plain word-offset/pixel-count convention.
 *
 * Moved here from main.c (where it originated as part of
 * run_real_boot_flow()'s real production display path, task #126)
 * specifically so it can be unit-tested: main.c #includes <gccore.h>
 * (the real Wii SDK), which isn't available in this project's host-
 * native regression environment, so nothing defined there has ever
 * been directly testable. This function has no Wii-specific
 * dependency at all - it's pure integer bit-field arithmetic - so
 * moving it into this already-host-tested module (gs_wii_output.c/.h)
 * closes a real, previously-undetected test-coverage gap in exactly
 * the code path that decides what real GS memory region gets shown
 * on screen once PMODE/DISPFB1 are finally written for real. main.c
 * now just calls this declaration instead of defining its own static
 * copy - no behavior change, verified identical logic. */
void gs_decode_dispfb(uint64_t dispfb, uint32_t *out_bp_words, uint32_t *out_bw_pixels);

void gs_decode_display_region(uint64_t dispfb, uint64_t display, uint64_t smode2,
                              uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
void gs_blit_scaled_psmct32_to_xfb(void *xfb, uint32_t dst_w, uint32_t dst_h,
                                  uint32_t bp, uint32_t bw,
                                  uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh);
/* Sample the displayed rectangle for actual RGB content. Alpha-only black
 * pixels do not count. This is a presentation probe, never a guest mutation. */
int gs_display_has_rgb(uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t sw,uint32_t sh);
/* 8x8 grid: samples, average R/G/B, maximum approximate luma, nonblack.
 * Reads coherent CPU-visible VRAM; it does not measure TV brightness. */
void gs_scanout_probe_rgb(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t stats[6]);
/* Guest-selected display PSM. 0=CT32, 1=CT24, 2=CT16, 10=CT16S.
 * These APIs never reinterpret CT16 as CT32 or change guest VRAM. */
uint32_t gs_scanout_read_rgb(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm);
void gs_blit_scaled_psm_to_xfb(void *xfb,uint32_t dst_w,uint32_t dst_h,
                               uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,
                               uint32_t sw,uint32_t sh,uint32_t psm);
/* Checked software presentation: 1 copied, 0 invalid input, -1 failed
 * GPU synchronization. Failed calls leave XFB untouched. */
int gs_blit_scaled_psm_to_xfb_checked(void *xfb,uint32_t dst_w,uint32_t dst_h,
                               uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,
                               uint32_t sw,uint32_t sh,uint32_t psm);
int gs_display_has_rgb_psm(uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t sw,uint32_t sh,uint32_t psm);
void gs_scanout_probe_rgb_psm(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                              uint32_t w,uint32_t h,uint32_t psm,uint32_t stats[6]);
/* Bounded 8x8 RGB fingerprint, not a full framebuffer hash. Returns zero
 * on invalid geometry/PSM or failed GPU readback; hash is then zero.
 * Uses the same grid as the color probe. Never hashes failed reads as black. */
int gs_scanout_grid_hash(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                         uint32_t w,uint32_t h,uint32_t psm,uint32_t *hash);
/* Post-presentation 4:3 fit for a Wii configured to a physical 16:9 TV.
 * XFB is coherent and writable; zero return leaves unsupported sizes intact. */
int gs_xfb_fit_4_3(void *xfb,uint32_t width,uint32_t height,int widescreen);
#endif
