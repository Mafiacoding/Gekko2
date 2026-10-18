/* Synthetic card and EE RPC fixtures; no user memory card contents. */
#include "core/system.h"
#include "core/hw/ps2_mc_files.h"
#include "core/ee/ee_core.c"
#include <assert.h>
#include <unistd.h>
static void put(uint8_t *p,unsigned at,uint32_t n){ee_mc_store32(p+at,n);}
int main(void)
{
 bios_image_t bios={0};assert(!system_init(&bios,&bios));ee_state_t *s=ee_core_get_state();
 assert(ps2_mc_fs_getentspace("/")==-11);
 char path[]="/tmp/gekko2-ent-XXXXXX";int tmp=mkstemp(path);assert(tmp>=0);
 FILE *f=fdopen(tmp,"wb");assert(f);uint8_t page[512];memset(page,255,512);
 for(unsigned i=0;i<PS2_MC_PAGES;i++)assert(fwrite(page,1,512,f)==512);assert(!fclose(f));
 assert(!ps2_mc_mount(path,0));assert(ps2_mc_fs_getentspace("/")==-2);
 assert(!ps2_mc_fs_format(0));assert(ps2_mc_fs_getentspace("/")==0);
 assert(!ps2_mc_fs_open("/DIR",0x40));assert(ps2_mc_fs_getentspace("/DIR")==0);
 const char *names[]={"/DIR/a","/DIR/b","/DIR/c","/DIR/d"};
 for(unsigned i=0;i<4;i++){
  int fd=ps2_mc_fs_open(names[i],0x203);assert(fd>=0);assert(!ps2_mc_fs_close(fd));
  assert(ps2_mc_fs_getentspace("/DIR")==((i+1)&1));
 }
 assert(ps2_mc_fs_getentspace("/DIR/a")==-5);
 assert(ps2_mc_fs_getentspace("/absent")==-4);
 assert(!ps2_mc_fs_delete("/DIR/b"));assert(ps2_mc_fs_getentspace("/DIR")==1);
 assert(!ps2_mc_fs_delete("/DIR/d"));assert(ps2_mc_fs_getentspace("/DIR")==2);
 uint8_t arg[1044]={0};strcpy((char*)arg+20,"/DIR");
 memcpy(s->ram+0x10000,arg,sizeof arg);
 assert(ee_mcserv_files_rpc(s,0x12,0xa0010000,sizeof arg)==2);
 /* Name shape, bounded termination, unsupported port/slot and payload length. */
 assert(ee_mcserv_files_rpc(s,0x12,0xa0010000,20)==-5);
 memset(s->ram+0x10000+20,'x',1024);assert(ee_mcserv_files_rpc(s,0x12,0xa0010000,sizeof arg)==-5);
 strcpy((char*)arg+20,"/DIR");put(arg,0,1);memcpy(s->ram+0x10000,arg,sizeof arg);
 assert(ee_mcserv_files_rpc(s,0x12,0xa0010000,sizeof arg)==-11);
 put(arg,0,0);put(arg,4,1);memcpy(s->ram+0x10000,arg,sizeof arg);
 assert(ee_mcserv_files_rpc(s,0x12,0xa0010000,sizeof arg)==-11);
 assert(!ps2_mc_eject());assert(!ps2_mc_mount(path,1));assert(ps2_mc_fs_getentspace("/DIR")==2);
 assert(!ps2_mc_eject());unlink(path);
 puts("PASS GETENTSPACE odd/even directories, deleted entries, RPC shape/ports, errors and readonly persistence");return 0;
}
