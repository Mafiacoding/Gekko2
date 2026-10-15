#define main legacy_gouraud_main
#include "test_gif_gouraud.c"
#undef main
static unsigned seed=314159u;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void){
 gif_init();gs_mem_init();
 for(unsigned n=0;n<2000;n++){
  apply_ad_write(GS_REG_PRIM,((n%3)+3)|((n&1)?8:0),0);
  unsigned x=rnd()%16,y=rnd()%16;
  for(unsigned v=0;v<6;v++){
   g_gif.rgba=rnd();g_gif.cur_u=rnd()%16;g_gif.cur_v=rnd()%16;
   unsigned px=(n&2)?x:rnd()%16,py=(n&2)?y:rnd()%16;
   apply_xyz2_kick(px<<4,py<<4,rnd(),(rnd()&7)!=0);
  }
 }
 uint64_t hash=1469598103934665603ULL;
 const unsigned char *p=(const unsigned char*)&g_gif;
 for(unsigned i=0;i<sizeof g_gif;i++){hash^=p[i];hash*=1099511628211ULL;}
 p=(const unsigned char*)gs_mem_get();
 for(unsigned i=0;i<4*1024*1024;i++){hash^=p[i];hash*=1099511628211ULL;}
 printf("ASSEMBLY_SIGNATURE %016llx\n",(unsigned long long)hash);
 for(unsigned i=0;i<GIF_GOURAUD_STAT_COUNT;i++)printf("%llu ",(unsigned long long)g_gst[i]);puts("");
 return 0;
}
