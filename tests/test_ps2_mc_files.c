/* Synthetic FAT/RPC tests. No BIOS, user card, icon or game assets. */
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/hw/ps2_mc_files.h"
#include "core/ee/ee_core.c"
#include <assert.h>
#include <unistd.h>
static void card_create(char path[128])
{
 snprintf(path,128,"/tmp/gekko2-fs-%ld.ps2",(long)getpid());FILE *f=fopen(path,"wb");assert(f);
 uint8_t page[512];memset(page,255,sizeof page);
 for(unsigned i=0;i<16384;i++)assert(fwrite(page,1,512,f)==512);assert(!fclose(f));assert(!ps2_mc_mount(path,0));
}
static int rpc(ee_state_t *s,unsigned fno,const uint8_t *arg,unsigned size)
{memcpy(s->ram+0x10000,arg,size);return ee_mcserv_files_rpc(s,fno,0xa0010000u,size);}
static void put(uint8_t *p,unsigned at,uint32_t value){ee_mc_store32(p+at,value);}
static uint32_t get(const uint8_t *p,unsigned at){return ee_mc_load32(p+at);}
static void imported_fixture(void)
{
 /* Independently described card: data starts at cluster48. Non-contiguous
  * file 4->6, nested directory at2 whose '.' points to root slot2. */
 assert(!ps2_mc_erase_all());uint8_t b[1024];memset(b,255,sizeof b);
 memcpy(b,"Sony PS2 Memory Card Format ",28);memcpy(b+28,"1.2.0.0",8);
 b[40]=0;b[41]=2;b[42]=2;b[43]=0;put(b,48,8192);put(b,52,48);put(b,56,8000);put(b,60,0);put(b,80,8);
 assert(!ps2_mc_logical_write(0,b,512));
 for(unsigned i=0;i<32;i++){put(b,0,16+i);assert(!ps2_mc_logical_write(8192+i*4,b,4));}
 for(unsigned i=0;i<8192;i++){
  uint32_t n=(i==0?0x80000001u:i==2?0x80000003u:i==1||i==3||i==6?UINT32_MAX:i==4?0x80000006u:0x7fffffffu);
  put(b,0,n);assert(!ps2_mc_logical_write(16384+i*4,b,4));
 }
 const unsigned clusters[]={0,0,0,2,2,2};const unsigned slots[]={0,1,2,0,1,2};
 const char *names[]={".","..","IMPORTED",".","..","SAVE"};
 for(unsigned i=0;i<6;i++){
  memset(b,0,512);unsigned mode=i==5?0x8497:0x8427;b[0]=(uint8_t)mode;b[1]=(uint8_t)(mode>>8);
  put(b,4,i==0||i==2?3:i==5?1500:0);put(b,16,i==2?2:i==5?4:0);if(i==3)put(b,20,2);
  memcpy(b+64,names[i],strlen(names[i]));unsigned logical=(48+clusters[i])*1024+slots[i]*512;
  /* Root third slot lives in its second FAT cluster. */
  assert(!ps2_mc_logical_write(logical,b,512));
 }
 for(unsigned i=0;i<1024;i++)b[i]=(uint8_t)(i*17+11);
 assert(!ps2_mc_logical_write(52*1024,b,1024));assert(!ps2_mc_logical_write(54*1024,b,1024));ps2_mc_fs_reset();
}
int main(void)
{
 bios_image_t bios={0};assert(!system_init(&bios,&bios));ee_state_t *s=ee_core_get_state();char path[128];card_create(path);
 assert(ps2_mc_fs_open("/save",1)==-2);assert(!ps2_mc_fs_format(0));unsigned fmt,free0,free1;
 assert(!ps2_mc_get_info(&fmt,&free0)&&fmt&&free0==8111);
 assert(!ps2_mc_fs_open("/SYNTH",0x40));assert(ps2_mc_fs_open("/SYNTH",0x40)<0);
 int fd=ps2_mc_fs_open("/SYNTH/save",0x203);assert(fd>=0);
 uint8_t source[5000],dest[5000];for(unsigned i=0;i<5000;i++)source[i]=(uint8_t)(i*31u+(i>>8));
 assert(ps2_mc_fs_write(fd,source,3000)==3000);assert(ps2_mc_fs_seek(fd,4000,0)==4000);
 assert(ps2_mc_fs_write(fd,source+4000,1000)==1000);assert(ps2_mc_fs_seek(fd,0,0)==0);
 memset(dest,77,sizeof dest);assert(ps2_mc_fs_read(fd,dest,5000)==5000);
 assert(!memcmp(dest,source,3000)&&!memcmp(dest+4000,source+4000,1000));for(unsigned i=3000;i<4000;i++)assert(!dest[i]);
 assert(ps2_mc_fs_seek(fd,-1,0)<0);assert(ps2_mc_fs_write(fd,source,8388608)==-3);assert(ps2_mc_fs_delete("/SYNTH/save")==-5);
 assert(!ps2_mc_fs_close(fd));assert(ps2_mc_fs_close(fd)==-5);assert(ps2_mc_fs_delete("/SYNTH")==-6);
 uint8_t table[4*64];assert(ps2_mc_fs_getdir("/SYNTH/s?ve",0,1,table)==1);assert(!memcmp(table+32,"save",5)&&get(table,16)==5000);
 assert(ps2_mc_fs_getdir("unused",1,1,table)==0);memcpy(table+32,"renamed",8);assert(!ps2_mc_fs_setinfo("/SYNTH/save",0x10,table));
 char cwd[1024];assert(!ps2_mc_fs_chdir("/SYNTH",cwd)&&!strcmp(cwd,"/SYNTH"));
 assert(ps2_mc_fs_open("../SYNTH/renamed",1)>=0);ps2_mc_fs_reset();
 assert(!ps2_mc_eject());assert(!ps2_mc_mount(path,0));fd=ps2_mc_fs_open("/SYNTH/renamed",1);assert(fd>=0);
 assert(ps2_mc_fs_read(fd,dest,5000)==5000&&!memcmp(dest,source,3000));assert(!ps2_mc_fs_close(fd));
 assert(!ps2_mc_fs_delete("/SYNTH/renamed"));assert(!ps2_mc_fs_delete("/SYNTH"));
 assert(!ps2_mc_get_info(&fmt,&free1)&&free1==free0-1); /* root retained its grown directory cluster */
 fd=ps2_mc_fs_open("/rpc",0x203);assert(fd>=0);assert(ps2_mc_fs_write(fd,source,5000)==5000);assert(!ps2_mc_fs_close(fd));
 /* Both READ ABIs, every buffer alignment, short/long request and EOF.
  * Apply the EE callback independently after checking it hasn't run early. */
 for(unsigned modern=0;modern<2;modern++)for(unsigned align=0;align<64;align++)for(unsigned trial=0;trial<4;trial++){
  unsigned requested[]={7,129,5001,0};unsigned n=requested[trial],actual=n<5000?n:5000;
  uint8_t arg[48]={0};put(arg,8,1);strcpy((char*)s->ram+0x10014,"/rpc");
  fd=ps2_mc_fs_open("/rpc",1);assert(fd>=0);put(arg,0,(unsigned)fd);put(arg,12,n);put(arg,24,0xa0020000u+align);put(arg,28,0xa0030000u);
  memset(s->ram+0x20000,0xee,5200);memset(s->ram+0x30000,0xdd,192);
  assert(rpc(s,modern?5:0x73,arg,48)==(int)actual);uint8_t *end=s->ram+0x30000,*data=s->ram+0x20000+align;
  unsigned head=get(end,0),tail=get(end,4),unit=modern?64:16;
  assert(head<=unit&&tail<unit&&head+tail<=actual);
  for(unsigned i=0;i<head;i++)assert(data[i]==0xee);for(unsigned i=actual-tail;i<actual;i++)assert(data[i]==0xee);
  if(head){assert(get(end,8)==0xa0020000u+align);memcpy(data,end+16,head);}
  if(tail){assert(get(end,12)==0xa0020000u+align+actual-tail);memcpy(data+actual-tail,end+16+unit,tail);}
  assert(!memcmp(data,source,actual));assert(s->ram[0x20000+align+actual]==0xee);assert(!ps2_mc_fs_close(fd));
 }
 /* Invalid DMA is rejected before position changes. */
 fd=ps2_mc_fs_open("/rpc",1);uint8_t arg[48]={0};put(arg,0,(unsigned)fd);put(arg,12,100);put(arg,24,0xa2000000u-10);put(arg,28,0xa0030000u);
 assert(rpc(s,5,arg,48)==-5);assert(ps2_mc_fs_seek(fd,0,1)==0);assert(!ps2_mc_fs_close(fd));
 /* WRITE prefix and EE bulk data, named OPEN/GETDIR/SETINFO and CLOSE,
  * dispatched through the two actual service opcode maps. */
 for(unsigned modern=0;modern<2;modern++){
  uint8_t request[1044]={0};put(request,8,0x203);strcpy((char*)request+20,"/rpc-write");
  fd=rpc(s,modern?2:0x71,request,sizeof request);assert(fd>=0);
  memset(arg,0,sizeof arg);put(arg,0,(unsigned)fd);put(arg,12,132);put(arg,20,13);put(arg,24,0xa0040000u);
  memcpy(arg+32,source,13);memcpy(s->ram+0x40000,source+13,132);assert(rpc(s,modern?6:0x74,arg,48)==145);
  assert(ps2_mc_fs_seek(fd,0,0)==0);assert(ps2_mc_fs_read(fd,dest,145)==145&&!memcmp(dest,source,145));
  put(arg,0,(unsigned)fd);assert(rpc(s,modern?3:0x72,arg,48)==0);
  memset(request,0,sizeof request);put(request,12,1);put(request,16,0xa0041000u);strcpy((char*)request+20,"/rpc-write");
  assert(rpc(s,modern?0x0d:0x76,request,sizeof request)==1);assert(get(s->ram+0x41000,16)==145);
  put(request,8,0x10);memset(s->ram+0x41020,0,32);strcpy((char*)s->ram+0x41020,"rpc-newname");
  assert(!rpc(s,modern?0x0e:0x7c,request,sizeof request));assert(!ps2_mc_fs_delete("/rpc-newname"));
 }
 /* Snapshot retains active file position, and rejects malformed handle state. */
 fd=ps2_mc_fs_open("/rpc",1);assert(ps2_mc_fs_seek(fd,1025,0)==1025);char snap[128];snprintf(snap,sizeof snap,"/tmp/gekko2-fs-%ld.ckpt",(long)getpid());
 assert(!checkpoint_save(snap));assert(ps2_mc_fs_read(fd,dest,300)==300);assert(!checkpoint_load(snap,&bios,&bios,NULL));
 assert(ps2_mc_fs_seek(fd,0,1)==1025);assert(ps2_mc_fs_read(fd,dest,300)==300&&!memcmp(dest,source+1025,300));assert(!ps2_mc_fs_close(fd));remove(snap);
 uint32_t snap_bytes;const void *fs_blob=ps2_mc_fs_snapshot(&snap_bytes);uint8_t invalid[4096];assert(snap_bytes<=sizeof invalid);
 memcpy(invalid,fs_blob,snap_bytes);invalid[4]=2;assert(!ps2_mc_fs_snapshot_valid(invalid,snap_bytes));
 /* Independently laid-out FAT with external parent metadata and a fragmented file. */
 imported_fixture();fd=ps2_mc_fs_open("/IMPORTED/SAVE",1);assert(fd>=0);assert(ps2_mc_fs_read(fd,dest,1500)==1500);
 for(unsigned i=0;i<1500;i++)assert(dest[i]==(uint8_t)((i%1024)*17+11));assert(!ps2_mc_fs_close(fd));
 /* A second entry sharing SAVE's chain is rejected before any mutation. */
 uint8_t alias[512]={0};alias[0]=0x97;alias[1]=0x84;put(alias,4,1500);put(alias,16,4);memcpy(alias+64,"ALIAS",6);
 assert(!ps2_mc_logical_write(51*1024+512,alias,512));put(arg,0,4);assert(!ps2_mc_logical_write(49*1024+4,arg,4));
 const void *before=ps2_mc_snapshot(&snap_bytes);uint8_t *copy=malloc(snap_bytes);assert(copy);memcpy(copy,before,snap_bytes);
 assert(ps2_mc_fs_open("/IMPORTED/NEW",0x202)==-13);assert(!memcmp(copy,ps2_mc_snapshot(&snap_bytes),snap_bytes));free(copy);
 put(arg,0,0x80000004u);assert(!ps2_mc_logical_write(16*1024+6*4,arg,4));assert(ps2_mc_fs_open("/IMPORTED/SAVE",1)==-13);
 assert(!ps2_mc_eject());assert(!ps2_mc_mount(path,1));assert(ps2_mc_fs_format(0)==-5);assert(ps2_mc_fs_open("/new",0x202)==-5);
 assert(!ps2_mc_eject());remove(path);puts("PASS standard PS2 FAT, independent fragmented card, 512 READ ABI/alignment/EOF cases, persistence, readonly, corrupt chain and open-file checkpoint");return 0;
}
