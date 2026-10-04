#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/checkpoint.h"
#include "core/system.h"
#include "core/hw/cdvd_config.h"
#define C(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 bios_image_t b={0}; C(!system_init(&b,&b));
 char path[]="/tmp/pcsx2-config-XXXXXX";int fd=mkstemp(path);C(fd>=0);close(fd);
 uint8_t data[16],saved[344],got[344];memset(data,0x53,16);
 C(!cdvd_config_open(1,2,2));C(!cdvd_config_write(data));cdvd_config_snapshot_save(saved);
 C(!checkpoint_save(path));cdvd_config_reset();C(!checkpoint_load(path,&b,&b,0));
 cdvd_config_snapshot_save(got);C(!memcmp(saved,got,344));
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
 C(!checkpoint_load(path,&b,&b,0));cdvd_config_snapshot_save(got);uint8_t zero[344]={0};C(!memcmp(got,zero,344));unlink(path);
 puts("CNFG checkpoint: round trip, malformed/duplicate rejection and legacy cold defaults pass");return 0;
}
