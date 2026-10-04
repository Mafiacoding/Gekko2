#ifndef GEKKO2_FRONTEND_LOGO_H
#define GEKKO2_FRONTEND_LOGO_H
#include <stdint.h>
/* Embedded launcher artwork. Logical 640x480 coordinates, packed Wii XFB. */
void frontend_logo_draw(void *xfb,uint32_t width,uint32_t height,int x,int y);
#endif
