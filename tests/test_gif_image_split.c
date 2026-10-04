#include <stdio.h>
#include <stdlib.h>
#include "hw/gif.c"
static void le32(unsigned char*p,unsigned v){for(int j=0;j<4;j++)p[j]=v>>(8*j);}
static void ad(unsigned char*p,unsigned reg,unsigned value){le32(p,0x8001);le32(p+4,1u<<28);le32(p+8,0xe);le32(p+16,value);le32(p+24,reg);}
int main(void){
 gs_mem_init();gif_init();
 apply_ad_write(GS_REG_BITBLTBUF,0,1u<<16);
 apply_ad_write(GS_REG_TRXREG,64,313);apply_ad_write(GS_REG_TRXDIR,0,0);
 unsigned char first[48]={0};le32(first,5000|0x8000);le32(first+4,2u<<26);
 gif_process_quadwords(GIF_PATH_3,first,3);
 if(g_gif.image_carry_path[GIF_PATH_3]!=4998)return puts("FAIL legal IMAGE above 64 KiB discarded"),1;
 unsigned char other[32]={0};ad(other,GS_REG_PRIM,5);gif_process_quadwords(GIF_PATH_2,other,2);
 if(g_gif.prim!=5||g_gif.image_carry_path[GIF_PATH_3]!=4998)return puts("FAIL IMAGE carry leaked to other GIF path"),1;
 unsigned char *tail=calloc(4998,16);le32(tail+4998*16-4,0x12345678);
 gif_process_quadwords(GIF_PATH_3,tail,4998);free(tail);
 if(g_gif.image_carry_path[GIF_PATH_3]||gs_mem_read_psmct32(0,64,31,312)!=0x12345678)return puts("FAIL split IMAGE final texel"),1;
 gif_init();apply_ad_write(GS_REG_BITBLTBUF,0,1u<<16);apply_ad_write(GS_REG_TRXREG,1,1);apply_ad_write(GS_REG_TRXDIR,0,0);
 le32(first,3|0x8000);gif_process_quadwords(GIF_PATH_3,first,2);
 if(g_gif.trx_active||g_gif.image_carry_path[GIF_PATH_3]!=2)return puts("FAIL IMAGE padding dropped when rectangle filled"),1;
 unsigned char rest[64]={0};ad(rest,GS_REG_PRIM,1);ad(rest+32,GS_REG_PRIM,6);
 gif_process_quadwords(GIF_PATH_3,rest,4);
 if(g_gif.prim!=6||g_gif.image_carry_path[GIF_PATH_3])return puts("FAIL IMAGE padding interpreted as GIFtag"),1;
 puts("PASS full-size split IMAGE, path isolation and rectangle padding");return 0;
}
