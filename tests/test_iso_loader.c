#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "core/iso_loader.c"
static int fail=0;
#define CHECK(x,m) do{if(x)printf("ok:   %s\n",m);else{printf("FAIL: %s\n",m);fail=1;}}while(0)
static void le32(uint8_t*p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void be32(uint8_t*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void both(uint8_t*p,uint32_t v){le32(p,v);be32(p+4,v);}
static uint32_t rec(uint8_t*d,uint32_t o,uint32_t lba,uint32_t size,uint8_t flags,const char*n){uint8_t nl=(uint8_t)strlen(n),rl=(uint8_t)(33+nl);d[o]=rl;both(d+o+2,lba);both(d+o+10,size);d[o+25]=flags;d[o+32]=nl;memcpy(d+o+33,n,nl);return o+rl;}
static int make_image(const char*path,uint32_t stride,uint32_t dataoff)
{
 FILE*f=fopen(path,"wb");if(!f)return -1;uint8_t phys[2352],sec[2048];
 #define WRITESEC(payload) do{memset(phys,0,sizeof(phys));memcpy(phys+dataoff,payload,2048);fwrite(phys,1,stride,f);}while(0)
 memset(sec,0,2048);for(int i=0;i<16;i++)WRITESEC(sec);
 memset(sec,0,2048);sec[0]=1;memcpy(sec+1,"CD001",5);sec[6]=1;uint8_t*r=sec+156;r[0]=34;both(r+2,20);both(r+10,2048);r[25]=2;r[32]=1;r[33]=0;WRITESEC(sec);
 memset(sec,0,2048);sec[0]=255;memcpy(sec+1,"CD001",5);sec[6]=1;WRITESEC(sec);memset(sec,0,2048);WRITESEC(sec);WRITESEC(sec);
 memset(sec,0,2048);uint32_t o=0;sec[o]=34;both(sec+o+2,20);both(sec+o+10,2048);sec[o+25]=2;sec[o+32]=1;o+=34;sec[o]=34;both(sec+o+2,20);both(sec+o+10,2048);sec[o+25]=2;sec[o+32]=1;sec[o+33]=1;o+=34;o=rec(sec,o,21,2048,2,"DATA");o=rec(sec,o,23,34,0,"SYSTEM.CNF;1");WRITESEC(sec);
 memset(sec,0,2048);o=0;sec[o]=34;both(sec+o+2,21);both(sec+o+10,2048);sec[o+25]=2;sec[o+32]=1;o+=34;sec[o]=34;both(sec+o+2,20);both(sec+o+10,2048);sec[o+25]=2;sec[o+32]=1;sec[o+33]=1;o+=34;o=rec(sec,o,22,32,0,"GAME.ELF;1");WRITESEC(sec);
 memset(sec,0,2048);memcpy(sec,"ELF-DATA",8);WRITESEC(sec);
 memset(sec,0,2048);memcpy(sec,"BOOT2 = cdrom0:\\DATA\\GAME.ELF;1\r\n",39);WRITESEC(sec);
 fclose(f);return 0;
 #undef WRITESEC
}
int main(void)
{
 const char*p="/tmp/r1330.iso";CHECK(make_image(p,2048,0)==0,"create plain ISO");iso_image_t img;CHECK(iso_open(p,&img)==0,"open plain 2048 image");iso_dirent_t e;CHECK(iso_find_path(&img,"cdrom0:\\SYSTEM.CNF",&e)==0&&e.lba==23,"SYSTEM.CNF ;1 fallback");CHECK(iso_find_path(&img,"cdrom0:\\DATA\\GAME.ELF",&e)==0&&e.lba==22,"nested game ELF path traversal");uint8_t b[2048];CHECK(iso_read_sector(&img,999,b)!=0,"out-of-image LBA rejected");iso_close(&img);remove(p);
 const char*r1="/tmp/r1330m1.bin";CHECK(make_image(r1,2352,16)==0,"create raw Mode1 image");CHECK(iso_open(r1,&img)==0&&img.physical_stride==2352&&img.data_offset==16,"detect raw Mode1");iso_close(&img);remove(r1);
 const char*r2="/tmp/r1330m2.bin";CHECK(make_image(r2,2352,24)==0,"create raw Mode2 Form1 image");CHECK(iso_open(r2,&img)==0&&img.physical_stride==2352&&img.data_offset==24,"detect raw Mode2 Form1");CHECK(iso_find_path(&img,"cdrom1:/DATA/GAME.ELF;1",&e)==0,"cdrom1 prefix and explicit version");iso_close(&img);remove(r2);
 /* PVD points outside the physical image: iso_open must reject it. */
 FILE*f=fopen(p,"wb");uint8_t z[2048];memset(z,0,sizeof(z));for(int i=0;i<16;i++)fwrite(z,1,2048,f);z[0]=1;memcpy(z+1,"CD001",5);z[6]=1;uint8_t*rr=z+156;rr[0]=34;both(rr+2,0xfffffffeu);both(rr+10,2048);rr[25]=2;rr[32]=1;rr[33]=0;fwrite(z,1,2048,f);fclose(f);CHECK(iso_open(p,&img)!=0,"root extent outside image rejected");remove(p);
 printf("\n%s\n",fail?"SOME CHECKS FAILED":"All checks passed.");return fail?1:0;
}
