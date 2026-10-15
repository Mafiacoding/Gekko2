#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core/hw/frontend_browser.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
static void le32(unsigned char*p,unsigned v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void be32(unsigned char*p,unsigned v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void both32(unsigned char*p,unsigned v){le32(p,v);be32(p+4,v);}
static void valid_image(const char *path)
{
 FILE *f=fopen(path,"wb");if(!f)exit(2);unsigned char sector[2048];
 for(unsigned i=0;i<18;i++){
  memset(sector,0,sizeof(sector));
  if(i==16){sector[0]=1;memcpy(sector+1,"CD001",5);sector[6]=1;unsigned char*r=sector+156;r[0]=34;both32(r+2,17);both32(r+10,2048);r[25]=2;r[32]=1;r[33]=0;}
  if(i==17){unsigned char*r=sector;r[0]=34;both32(r+2,17);both32(r+10,2048);r[25]=2;r[32]=1;r[33]=0;}
  fwrite(sector,1,sizeof(sector),f);
 }
 fclose(f);
}
static void file(const char *root,const char *name){char p[512];snprintf(p,sizeof(p),"%s/%s",root,name);FILE*f=fopen(p,"wb");if(!f)exit(2);fclose(f);}
int main(void)
{
 char root[]="/tmp/pcsx2-browser-XXXXXX",sub[512],out[512];if(!mkdtemp(root))return 2;
 snprintf(sub,sizeof(sub),"%s/folder",root);if(mkdir(sub,0700))return 2;
 file(root,"Tekken Tag.BIN");file(root,"another.ISO");file(root,"ignore.txt");file(sub,"nested.iso");
 frontend_browser b;
 if(frontend_browser_init(&b,root,root)||b.count!=3||strcmp(b.entries[0].name,"folder")||strcmp(b.entries[1].name,"another.ISO"))return 3;
 if(frontend_browser_activate(&b,out,sizeof(out))!=0||b.count!=2||strcmp(b.entries[0].name,".."))return 4;
 b.selected=1;if(frontend_browser_activate(&b,out,sizeof(out))!=1||!strstr(out,"/folder/nested.iso"))return 5;
 char marker[8]="KEEP";if(frontend_browser_activate(&b,marker,sizeof(marker))!=-1||strcmp(marker,"KEEP"))return 6;
 if(frontend_browser_up(&b)||strcmp(b.path,root)||frontend_browser_up(&b)||strcmp(b.path,root))return 7;
 unsigned count=b.count;if(frontend_browser_scan(&b,"/definitely-not-a-directory")!=-1||b.count!=count||strcmp(b.path,root))return 8;
 if(!frontend_browser_is_image("X.bIn")||frontend_browser_is_image("X.bin.exe"))return 9;
 b.selected=2;if(frontend_browser_activate(&b,out,sizeof(out))!=1||!strstr(out,"/Tekken Tag.BIN"))return 10;
 valid_image(out);
 iop_cdvd_init();iop_cdrom_legacy_init();
 if(iop_cdvd_mount_iso(out)||iop_cdrom_legacy_mount_iso(out))return 12;
 unsigned char sector[2048];if(iop_cdvd_disc_read_sector(16,sector)||memcmp(sector+1,"CD001",5))return 13;
 iop_cdvd_unmount_iso();iop_cdrom_legacy_unmount_iso();
 /* Grow beyond a single visible/list page without losing selectable files. */
 for(int i=0;i<140;i++){char n[32];snprintf(n,sizeof(n),"extra-%03d.iso",i);file(root,n);}
 if(frontend_browser_scan(&b,root)||b.count!=143||b.truncated)return 11;
 /* Remove only files created by this fixture. */
 for(int i=0;i<140;i++){char p[512];snprintf(p,sizeof(p),"%s/extra-%03d.iso",root,i);unlink(p);}
 char p[512];const char*names[]={"Tekken Tag.BIN","another.ISO","ignore.txt"};
 for(unsigned i=0;i<3;i++){snprintf(p,sizeof(p),"%s/%s",root,names[i]);unlink(p);}
 snprintf(p,sizeof(p),"%s/nested.iso",sub);unlink(p);rmdir(sub);rmdir(root);frontend_browser_release(&b);
 puts("PASS SD browser filtering, sorting, navigation, path selection, long-buffer safety and capacity");return 0;
}
