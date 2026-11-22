/* GPL-3.0-or-later. Tiny RGB text/blit helpers for the GX2 path of the Wii U test port (GX2 has no font). */
#ifndef GEKKO2_WIIU_TEXT_H
#define GEKKO2_WIIU_TEXT_H
#include <stddef.h>
#include <stdint.h>
/* Draws ASCII text with the antialiased 8x14 launcher font into a 0x00RRGGBB buffer. Clips to the buffer. */
void wiiu_text_draw(uint32_t *fb, unsigned w, unsigned h, int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);
/* Darkens a horizontal strip (rows y0..y1-1) so text stays readable over a picture. */
void wiiu_text_dim_rows(uint32_t *fb, unsigned w, unsigned y0, unsigned y1);
/* Nearest-neighbour scale of src (sw x sh) into dst (dw x dh). */
void wiiu_scale_nearest(uint32_t *dst, unsigned dw, unsigned dh, const uint32_t *src, unsigned sw, unsigned sh);
#endif
