#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/iso_loader.h"
#include "core/chd_reader.h"
#include "core/hw/frontend_browser.h"
#include "core/hw/iop_cdvd.h"
int main(void){
 char base[1024],path[1200],plain[1200];snprintf(base,sizeof(base),"%s",__FILE__);char *slash=strrchr(base,'/');assert(slash);*slash=0;
 snprintf(plain,sizeof(plain),"%s/fixtures/chd-r1378/synthetic.iso",base);
 const char *names[]={"none","zlib","lzma","cd"};
 for(unsigned n=0;n<4;n++){
  snprintf(path,sizeof(path),"%s/fixtures/chd-r1378/%s.chd",base,names[n]);
  iso_image_t a,b;assert(!iso_open(plain,&a));assert(!iso_open(path,&b)&&b.chd&&b.opened);
  iso_dirent_t entry;assert(!iso_find_path(&b,"cdrom0:/SYSTEM.CNF;1",&entry)&&entry.lba==32&&entry.size==64);
  uint8_t x[2048],y[2048];
  for(unsigned k=0;k<128;k++){unsigned lba=(k*37)%128;assert(!iso_read_sector(&a,lba,x));assert(!iso_read_sector(&b,lba,y));assert(!memcmp(x,y,2048));assert(!iso_read_sector(&b,lba,y)&&!memcmp(x,y,2048));}
  assert(iso_read_sector(&b,128,y)<0);iso_close(&a);iso_close(&b);assert(!b.chd&&!b.opened);
  assert(!iop_cdvd_mount_iso(path));assert(!iop_cdvd_disc_read_sector(16,y)&&!memcmp(y+1,"CD001",5));iop_cdvd_unmount_iso();
  printf("PASS %s CHD: all 128 sectors random/repeated match ISO, directory/file lookup, boundary rejection, live CDVD mount/read/unmount\n",names[n]);
 }
 snprintf(path,sizeof(path),"%s/fixtures/chd-r1378/truncated.chd",base);iso_image_t bad;assert(iso_open(path,&bad)<0&&!bad.opened);
 assert(frontend_browser_is_image("game.CHD"));assert(!frontend_browser_is_image("game.chd.txt"));
 assert(gekko2_chd_stat(1)>0&&gekko2_chd_stat(2)>0);
 puts("PASS truncated CHD rejection, case-insensitive browser, hunk cache exercised");return 0;
}
