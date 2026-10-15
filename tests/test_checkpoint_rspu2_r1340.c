#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/checkpoint.h"
#include "core/system.h"
#include "core/hw/cdvd_config.h"
#include "core/hw/ipu.h"
#include "core/hw/dma.h"
#include "core/hw/rspu2_stream.h"
#define C(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 bios_image_t b={0}; C(!system_init(&b,&b));
 char path[]="/tmp/pcsx2-config-XXXXXX";int fd=mkstemp(path);C(fd>=0);close(fd);
 uint8_t data[16],saved[344],got[344];memset(data,0x53,16);
 C(!cdvd_config_open(1,2,2));C(!cdvd_config_write(data));cdvd_config_snapshot_save(saved);
 uint8_t ipu_data[16];memset(ipu_data,0x9a,sizeof ipu_data);
 ipu_mmio_write32(0x10002000,7);ipu_mmio_write32(0x10002000,0x50000000);
 ipu_process_quadwords(4,ipu_data,1);ipu_state_t ipu_saved=*ipu_get_state();
 C(ipu_saved.busy&&ipu_saved.pos>0&&ipu_saved.pos<64);
 dma_get_state()->ipu_tag_pending=1;dma_get_state()->ipu_tag_end=1;
 uint32_t films[3]={8192,4096,2048},audio[3]={2048,1024,512};
 rspu2_stream_state_t stream_saved,stream_got;
 rspu2_stream_configure(100,65536,films,audio);
 uint32_t args[7]={0,0x2237,0x20008000,0,0,0,0};int32_t reply;
 C(rspu2_stream_rpc(0x2000,args,7,0,0,&reply)&&reply==0);
 rspu2_stream_resource_result(0);rspu2_stream_checkpoint_save(&stream_saved);
 C(!checkpoint_save(path));rspu2_stream_reset();cdvd_config_reset();ipu_init();dma_get_state()->ipu_tag_pending=0;
 C(!checkpoint_load(path,&b,&b,0));C(!memcmp(&ipu_saved,ipu_get_state(),sizeof ipu_saved));
 C(dma_get_state()->ipu_tag_pending==1&&dma_get_state()->ipu_tag_end==1);
 rspu2_stream_checkpoint_save(&stream_got);C(!memcmp(&stream_saved,&stream_got,sizeof stream_saved));
 cdvd_config_snapshot_save(got);C(!memcmp(saved,got,344));
 /* Reject corrupt/duplicate RSPU before applying any live state. */
 FILE *sf=fopen(path,"r+b");C(sf);char stag[4];uint32_t sn;long spos=-1;
 fseek(sf,8,SEEK_SET);
 while(fread(stag,1,4,sf)==4&&fread(&sn,4,1,sf)==1){
  if(!memcmp(stag,"RSPU",4)){spos=ftell(sf);break;}C(sn<40000000);C(!fseek(sf,sn,SEEK_CUR));}
 C(spos>=0&&sn==sizeof stream_saved);fseek(sf,spos+4,SEEK_SET);uint32_t invalid=2;fwrite(&invalid,4,1,sf);fclose(sf);
 C(checkpoint_load(path,&b,&b,0)<0);rspu2_stream_checkpoint_save(&stream_got);C(!memcmp(&stream_saved,&stream_got,sizeof stream_saved));
 C(!checkpoint_save(path));sf=fopen(path,"r+b");C(sf);fseek(sf,-8,SEEK_END);fwrite("RSPU",1,4,sf);sn=sizeof stream_saved;fwrite(&sn,4,1,sf);fwrite(&stream_saved,1,sn,sf);fwrite("END0",1,4,sf);sn=0;fwrite(&sn,4,1,sf);fclose(sf);
 C(checkpoint_load(path,&b,&b,0)<0);rspu2_stream_checkpoint_save(&stream_got);C(!memcmp(&stream_saved,&stream_got,sizeof stream_saved));
 C(!checkpoint_save(path));
 FILE*f=fopen(path,"r+b");C(f);long pos=-1;char tag[4];uint32_t n;
 /* Header is read through the writer's framing: locate CNFG by walking blocks. */
 for(long off=0;off<256;off++){fseek(f,off,SEEK_SET);if(fread(tag,1,4,f)==4&&!memcmp(tag,"EES1",4)){pos=off;break;}}
 C(pos>=0);fseek(f,pos,SEEK_SET);
 while(fread(tag,1,4,f)==4&&fread(&n,4,1,f)==1){long start=ftell(f)-8;if(!memcmp(tag,"CNFG",4)){pos=start;break;}C(n<40000000);C(!fseek(f,n,SEEK_CUR));}
 C(!memcmp(tag,"CNFG",4)&&n==344);fseek(f,pos+8+5,SEEK_SET);fputc(1,f);fclose(f);
 C(checkpoint_load(path,&b,&b,0)<0);cdvd_config_snapshot_save(got);C(!memcmp(saved,got,344));
 C(!checkpoint_save(path));f=fopen(path,"r+b");C(f);fseek(f,-8,SEEK_END);long original_end=ftell(f);fwrite("CNFG",1,4,f);n=344;fwrite(&n,4,1,f);fwrite(saved,1,344,f);fwrite("END0",1,4,f);n=0;fwrite(&n,4,1,f);fclose(f);
 C(checkpoint_load(path,&b,&b,0)<0);cdvd_config_snapshot_save(got);C(!memcmp(saved,got,344));
 f=fopen(path,"r+b");C(f);fseek(f,pos,SEEK_SET);fwrite("END0",1,4,f);n=0;fwrite(&n,4,1,f);fflush(f);ftruncate(fileno(f),pos+8);fclose(f);
 C(!checkpoint_load(path,&b,&b,0));cdvd_config_snapshot_save(got);uint8_t zero[344]={0};C(!memcmp(got,zero,344));rspu2_stream_checkpoint_save(&stream_got);C(!stream_got.configured&&!stream_got.initialized);unlink(path);
 puts("RSPU/CNFG checkpoint: round trip, malformed/duplicate rejection and legacy cold defaults pass");return 0;
}
