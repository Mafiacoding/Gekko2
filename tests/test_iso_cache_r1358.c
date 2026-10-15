#define main original_iso_test_main
#include "test_iso_loader.c"
#undef main
#include <assert.h>
int main(void)
{
 const char *path="/tmp/gekko2-r1358-cache.bin";
 const unsigned formats[3][2]={{2048,0},{2352,16},{2352,24}};
 for(unsigned fmt=0;fmt<3;fmt++){
  assert(!make_image(path,formats[fmt][0],formats[fmt][1]));iso_image_t image;
  assert(!iso_open(path,&image)&&image.sector_cache);
  uint8_t expected[2048],got[2048];uint64_t fills=iso_cache_stat(2),hits=iso_cache_stat(1);
  for(unsigned n=0;n<50;n++){
   uint32_t lba=16+n%8;assert(!read_sector_raw(image.fp,lba,formats[fmt][0],formats[fmt][1],expected));
   assert(!iso_read_sector(&image,lba,got)&&!memcmp(got,expected,2048));
  }
  assert(iso_cache_stat(2)==fills+1&&iso_cache_stat(1)==hits+49);
  /* Traversal uses a borrowed view and the same cache without double-free. */
  iso_dirent_t entry;assert(!iso_find_path(&image,"DATA/GAME.ELF",&entry)&&entry.lba==22);
  assert(iso_read_sector(&image,24,got)==-1&&iso_read_sector(&image,UINT32_MAX,got)==-1);
  iso_close(&image);assert(!image.opened&&!image.sector_cache&&!image.fp);
 }
 remove(path);puts("PASS ISO/Mode1/Mode2 cached payload parity, 49/50 hits, tail/OOB, traversal and unmount");
}
