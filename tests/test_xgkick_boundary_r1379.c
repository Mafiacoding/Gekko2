#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
static void w(uint8_t*p,uint32_t x){for(int i=0;i<4;i++)p[i]=x>>(8*i);}
int main(void){
 uint8_t tag[16]={0},packet[32]={0};
 gif_init();gs_mem_init();
 /* PACKED A+D tag with an incomplete payload and no EOP. */
 w(tag,4);w(tag+4,1u<<28);w(tag+8,0xe);
 gif_process_quadwords(GIF_PATH_3,tag,1);
 assert(g_gif.register_carry_path[GIF_PATH_3].remaining);
 gif_process_xgkick(tag,1);
 assert(!g_gif.register_carry_path[GIF_PATH_1].remaining);
 assert(g_gif.register_carry_path[GIF_PATH_3].remaining);
 w(packet,1u|0x8000u);w(packet+4,1u<<28);w(packet+8,0xe);
 w(packet+16,0x1234);w(packet+24,GS_REG_XYOFFSET_1);
 gif_process_xgkick(packet,2);
 assert(g_gif.ctx1_xyoffset_x==0x1234);
 assert(!g_gif.register_carry_path[GIF_PATH_1].remaining);
 /* IMAGE EOP continuation stops before unrelated following words. */
 memset(tag,0,sizeof tag);w(tag,2u|0x8000u);w(tag+4,2u<<26);
 gif_process_quadwords(GIF_PATH_1,tag,1);
 assert(g_gif.image_carry_path[GIF_PATH_1]==2);
 uint8_t tail[64]={0};memcpy(tail+32,packet,32);w(tail+48,0x9999);
 gif_process_quadwords(GIF_PATH_1,tail,4);
 assert(g_gif.ctx1_xyoffset_x==0x1234);
 assert(!g_gif.image_carry_path[GIF_PATH_1]);
 /* Ending PATH1 never clears another path's current IMAGE bookkeeping. */
 gif_init();
 w(tag,3u);gif_process_quadwords(GIF_PATH_3,tag,1);
 assert(g_gif.image_carry_remaining_qwords==3);
 gif_end_xgkick();
 assert(g_gif.image_carry_path[GIF_PATH_3]==3 && g_gif.image_carry_remaining_qwords==3);
 puts("PASS incomplete bounded XGKICK does not poison next kick or PATH3");
 return 0;
}
