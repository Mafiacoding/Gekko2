/* GPL-3.0-or-later. GX2 presenter for the Wii U test port: uploads an XRGB frame to a texture and draws it as a
 * 4:3 quad on the TV and the GamePad, so scaling is done by the GPU instead of one OSScreenPutPixelEx per pixel.
 * Needs present.gsh (assemble shaders/present.vsh + present.psh with latte-assembler, see ../README.md).
 * It compiles against wut/libwhb; it has NOT been run on Cemu or a Wii U by the author. Do not combine with
 * OSScreen in the same process: both want the scan buffers. */
#ifndef GEKKO2_GX2_PRESENT_H
#define GEKKO2_GX2_PRESENT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* gsh/size: contents of present.gsh (a GFD file holding one vertex and one pixel shader, index 0 each).
 * Returns 1 on success. On failure everything is released and 0 is returned (the caller can fall back to OSScreen
 * only if WHBGfxInit was not reached; check the return code of gx2_present_init_gfx() first). */
int gx2_present_init(const void *gsh, uint32_t size, unsigned max_w, unsigned max_h);
void gx2_present_shutdown(void);
/* xrgb: w*h pixels, 0x00RRGGBB, w <= max_w, h <= max_h. filter: 0 = nearest, 1 = bilinear. */
void gx2_present_frame(const uint32_t *xrgb, unsigned w, unsigned h, int filter);
#ifdef __cplusplus
}
#endif
#endif
