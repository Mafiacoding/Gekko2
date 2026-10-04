#ifndef FRONTEND_TEXT_H
#define FRONTEND_TEXT_H
#include <stdint.h>
/* Native launcher only: coverage font, logical 640x480 layout, packed Wii YCbCr.
 * Scale 1 and 3 have separately rasterized glyphs, never enlarged bitmap cells. */
void frontend_text_draw(void *xfb,uint32_t width,uint32_t height,
                       int x,int y,int scale,const char *text,
                       uint8_t r,uint8_t g,uint8_t b);
#endif
