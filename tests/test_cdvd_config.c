#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "core/hw/cdvd_config.h"
#include "core/hw/iop_cdvd.h"
static unsigned fails;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fails++;}}while(0)
int main(void){
 uint8_t a[16],b[16],snap[CDVD_CONFIG_SNAPSHOT_SIZE];for(unsigned i=0;i<16;i++)a[i]=i*9;
 cdvd_config_reset();CHECK(cdvd_config_read(b)==0x80&&b[0]==0x80,"closed session read fails");CHECK(cdvd_config_open(0,3,1)==0x80,"invalid bank rejected");
 const unsigned lens[]={4,2,7};
 for(unsigned bank=0;bank<3;bank++){
 CHECK(!cdvd_config_open(1,bank,lens[bank]+1),"open hardware bank");for(unsigned j=0;j<lens[bank];j++){a[0]=bank*20+j;CHECK(!cdvd_config_write(a),"write implemented block");}
 CHECK(!cdvd_config_write(a),"outside implemented bank write ignored without advancing physical cursor");
 cdvd_config_open(0,bank,lens[bank]+1);for(unsigned j=0;j<lens[bank];j++){CHECK(!cdvd_config_read(b)&&b[0]==bank*20+j,"bank block data retained");}
 CHECK(!cdvd_config_read(b)&&b[0]==0&&b[15]==0,"outside implemented bank read returns zeros");cdvd_config_close();
 }
 cdvd_config_open(0,1,2);CHECK(cdvd_config_write(a)==0x80,"read mode cannot write");cdvd_config_read(b);cdvd_config_snapshot_save(snap);CHECK(cdvd_config_snapshot_valid(snap)&&snap[0]==1&&snap[2]==1&&snap[4]==1,"session snapshot has explicit byte layout");
 iop_cdvd_init();CHECK(cdvd_config_read(b)==0x80,"soft peripheral reset clears session");cdvd_config_snapshot_load(snap);CHECK(!cdvd_config_read(b)&&b[0]==21,"snapshot restores cursor and bank data");CHECK(cdvd_config_read(b)==0x80,"requested block count exhaustion detected");
 snap[4]=3;CHECK(!cdvd_config_snapshot_valid(snap),"invalid cursor rejected");snap[4]=1;snap[7]=1;CHECK(!cdvd_config_snapshot_valid(snap),"reserved snapshot byte rejected");
 char path[]="/tmp/pcsx2-config-test-XXXXXX";int fd=mkstemp(path);close(fd);unlink(path);
 cdvd_config_reset();CHECK(cdvd_config_bind_file(path)==1,"missing config file creates clean persistence binding");cdvd_config_open(1,2,1);memset(a,0x5a,16);CHECK(!cdvd_config_write(a),"write persists accepted data");
 cdvd_config_reset();CHECK(!cdvd_config_bind_file(path),"file reload validates format and CRC");cdvd_config_open(0,2,1);CHECK(!cdvd_config_read(b)&&!memcmp(a,b,16),"data survives genuinely fresh model reset");
 char backup[128];snprintf(backup,sizeof backup,"%s.bak",path);CHECK(!rename(path,backup),"simulate replacement interrupted after backup");
 cdvd_config_reset();CHECK(!cdvd_config_bind_file(path),"missing primary recovers validated backup");cdvd_config_open(0,2,1);CHECK(!cdvd_config_read(b)&&!memcmp(a,b,16),"backup recovery retains committed data");CHECK(!rename(backup,path),"restore primary for corrupt-file fixture");
 FILE*f=fopen(path,"r+b");fseek(f,50,SEEK_SET);fputc(0xcc,f);fclose(f);CHECK(cdvd_config_bind_file(path)==-1,"corrupt file rejected without replacing existing banks");cdvd_config_open(0,2,1);CHECK(!cdvd_config_read(b)&&!memcmp(a,b,16),"failed load retains live bank data");unlink(path);
 CHECK(cdvd_config_bind_file("/missing/config/dir/settings.bin")==1,"missing path does not invent existing data");cdvd_config_open(1,0,1);CHECK(cdvd_config_write(a)==0x80,"failed save cannot acknowledge persistent write");cdvd_config_open(0,0,1);CHECK(!cdvd_config_read(b)&&!b[0],"failed save rolls memory back");
 printf("CDVD config banks/storage: %u failures\n",fails);return fails?1:0;
}
