#include <stdio.h>
#include <unistd.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "hw/gif.c"
static unsigned fails;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fails++;}}while(0)
static void first(int fraction){
 gif_init();gs_mem_init();apply_ad_write(GS_REG_FRAME_1,2u<<16,0);
 apply_ad_write(GS_REG_SCISSOR_1,127u<<16,63u<<16);apply_ad_write(GS_REG_XYOFFSET_1,32768,32768);
 apply_ad_write(GS_REG_PRIM,6,0);apply_ad_write(GS_REG_RGBAQ,0x80123456,0);
 apply_xyz2_kick(32768+32+fraction,32768+16+fraction,0,1);
}
static void second(void){apply_xyz2_kick(32768+18*16+8,32768+11*16+8,0,1);}
static int legacy(const char*path){
 FILE*f=fopen(path,"rb");if(!f)return -1;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 unsigned char*b=malloc(n);if(!b){fclose(f);return -1;}fread(b,1,n,f);fclose(f);
 long p=8;int found=0;
 while(p+8<=n){unsigned size;memcpy(&size,b+p+4,4);
  if(!memcmp(b+p,"GIF0",4)){if(size!=sizeof(gif_state_t)){free(b);return -1;}
   memmove(b+p+8+size-8,b+p+8+size,n-(p+8+size));size-=8;memcpy(b+p+4,&size,4);n-=8;found=1;break;}
  p+=8+size;
 }
 if(found){f=fopen(path,"wb");fwrite(b,1,n,f);fclose(f);}free(b);return found?0:-1;
}
int main(void){
 bios_image_t bios={0};if(system_init(&bios,&bios))return 2;
 char path[]="/tmp/sprite-ckpt-XXXXXX";int fd=mkstemp(path);close(fd);
 uint32_t expected[64][128];
 for(int old=0;old<2;old++){
  first(old?0:7);uint64_t raw=g_gif.sprite_pos16;
  CHECK(!checkpoint_save(path),"save split fractional sprite");
  second();for(int y=0;y<64;y++)for(int x=0;x<128;x++)expected[y][x]=gs_mem_read_psmct32(0,128,x,y);
  if(old)CHECK(!legacy(path),"construct original GIF0 layout without appended coordinates");
  gif_init();CHECK(!checkpoint_load(path,&bios,&bios,NULL),"load new or original GIF0 layout");
  CHECK(g_gif.sprite_pos16==raw,"restore exact new coordinates or reconstruct legacy integers");
  second();for(int y=0;y<64;y++)for(int x=0;x<128;x++)CHECK(gs_mem_read_psmct32(0,128,x,y)==expected[y][x],"split sprite coverage survives checkpoint");
 }
 unlink(path);printf("Sprite checkpoint: %u failures\n",fails);return !!fails;
}
