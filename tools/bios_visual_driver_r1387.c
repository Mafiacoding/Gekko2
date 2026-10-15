/* Deterministic RTC for comparative real-BIOS renderer validation only.
 * Link with --wrap=time; guest code, input and framebuffer remain untouched. */
#include <time.h>
time_t __wrap_time(time_t *out){time_t value=(time_t)1791806400;if(out)*out=value;return value;}
#define main bios_visual_main
#include "game_check_r1386.c"
#undef main
int main(int argc,char **argv){int result=bios_visual_main(argc,argv);
for(unsigned n=0;n<GIF_GOURAUD_STAT_COUNT;n++)printf("GS_TRI_STAT %u=%llu\n",n,(unsigned long long)gif_get_gouraud_stat(n));
for(unsigned n=0;n<16;n++){int32_t xy[6];uint32_t prim;if(gif_get_degenerate_sample(n,xy,&prim))printf("DEGENERATE %u prim=%x xy=%d,%d %d,%d %d,%d\n",n,prim,xy[0],xy[1],xy[2],xy[3],xy[4],xy[5]);}
for(unsigned n=0;n<64;n++){uint32_t v[10];if(gif_get_vertex_source(n,v))printf("VERTEX_SOURCE %u %x %x %x %x %x %x %x %x %x %x\n",n,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9]);}
return result;}
