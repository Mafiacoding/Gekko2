#ifndef GEKKO2_GS_TRIANGLE_COVERAGE_H
#define GEKKO2_GS_TRIANGLE_COVERAGE_H
#include <stdint.h>
/* Screen Y increases downwards. Normalize winding before choosing
 * inclusive top/left edges; the opposite shared edge is exclusive. */
static inline int gs_triangle_edge_inclusive(int32_t ax,int32_t ay,int32_t bx,int32_t by,int sign)
{
 int32_t dx=(bx-ax)*sign,dy=(by-ay)*sign;
 return dy<0||(dy==0&&dx>0);
}
static inline int gs_triangle_edge_inside(int32_t w,int sign,int inclusive)
{return sign>0?(w>0||(w==0&&inclusive)):(w<0||(w==0&&inclusive));}
#endif
