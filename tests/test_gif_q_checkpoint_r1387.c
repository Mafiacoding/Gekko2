#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/hw/gif.h"
int main(void)
{
 bios_image_t bios={0};char path[]="/tmp/gekko2-q-XXXXXX",legacy[]="/tmp/gekko2-q-old-XXXXXX";
 int fd=mkstemp(path);assert(fd>=0);close(fd);fd=mkstemp(legacy);assert(fd>=0);close(fd);
 assert(system_init(&bios,&bios)==0);
 gif_get_state()->gif_q_bits=0x40000000;gif_get_state()->cur_q=3.0f;
 assert(checkpoint_save(path)==0);gif_get_state()->gif_q_bits=0x40800000;
 assert(checkpoint_load(path,&bios,&bios,NULL)==0);
 assert(gif_get_state()->gif_q_bits==0x40000000 && gif_get_state()->cur_q==3.0f);
 FILE *in=fopen(path,"rb"),*out=fopen(legacy,"wb");assert(in&&out);
 unsigned char magic[8];assert(fread(magic,1,8,in)==8);assert(fwrite(magic,1,8,out)==8);
 for(;;){char tag[4];uint32_t n;assert(fread(tag,1,4,in)==4);assert(fread(&n,4,1,in)==1);
  unsigned char *blob=malloc(n?n:1);assert(blob);assert(fread(blob,1,n,in)==n);
  uint32_t kept=n;if(!memcmp(tag,"GIF0",4))kept=offsetof(gif_state_t,gif_q_bits);
  assert(fwrite(tag,1,4,out)==4);assert(fwrite(&kept,4,1,out)==1);assert(fwrite(blob,1,kept,out)==kept);free(blob);
  if(!memcmp(tag,"END0",4))break;
 }
 fclose(in);fclose(out);gif_get_state()->gif_q_bits=0x40800000;
 assert(checkpoint_load(legacy,&bios,&bios,NULL)==0);
 assert(gif_get_state()->gif_q_bits==0x3f800000 && gif_get_state()->cur_q==3.0f);
 unlink(path);unlink(legacy);puts("PASS independent GIF/GS Q checkpoint roundtrip and legacy prefix recovery");return 0;
}
