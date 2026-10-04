#include "core/hw/cdvd_config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
static uint8_t banks[3][7][16];
static uint8_t active,mode,bank,count,block_index;
static char filename[512];
static const unsigned blocks[3]={4,2,7};
static uint32_t crc(const uint8_t*p,unsigned n){uint32_t x=~0u;while(n--){x^=*p++;for(unsigned i=0;i<8;i++)x=(x>>1)^(0xedb88320u&-(x&1u));}return ~x;}
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t get32(const uint8_t*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void cdvd_config_reset_session(void){active=mode=bank=count=block_index=0;}
void cdvd_config_reset(void){memset(banks,0,sizeof banks);filename[0]=0;cdvd_config_reset_session();}
unsigned cdvd_config_count(void){return active?count:0;}
uint8_t cdvd_config_open(unsigned m,unsigned b,unsigned n){cdvd_config_reset_session();if(m>1||b>2||n>255)return 0x80;active=1;mode=m;bank=b;count=n;return 0;}
uint8_t cdvd_config_close(void){cdvd_config_reset_session();return 0;}
uint8_t cdvd_config_read(uint8_t out[16]){
 memset(out,0,16);if(!active||mode!=0||block_index>=count){out[0]=0x80;return 0x80;}
 /* PCSX2 cdvdReadConfig: requests beyond implemented blocks read zeros,
  * without advancing the physical block cursor. */
 if(block_index>=blocks[bank])return 0;
 memcpy(out,banks[bank][block_index++],16);return 0;
}
static int load_file(const char*p,uint8_t out[336]){
 FILE*f=fopen(p,"rb");if(!f)return errno==ENOENT?1:-1;uint8_t b[344];int n=fread(b,1,sizeof b,f),extra=fgetc(f);int ok=!ferror(f)&&n==sizeof b&&extra==EOF&&!memcmp(b,"WNV1",4)&&get32(b+340)==crc(b,340);fclose(f);if(!ok)return -1;memcpy(out,b+4,336);return 0;
}
int cdvd_config_bind_file(const char*p){
 filename[0]=0;if(!p||!p[0]||strlen(p)>500)return -1;uint8_t b[336];int rc=load_file(p,b);
 if(rc==1){char bak[512];snprintf(bak,sizeof bak,"%s.bak",p);int br=load_file(bak,b);if(br==0)rc=0;else if(br<0)return -1;}
 if(rc<0)return -1;if(!rc)memcpy(banks,b,sizeof banks);strcpy(filename,p);cdvd_config_reset_session();return rc;
}
static int persist(void){
 if(!filename[0])return 0;uint8_t b[344];memcpy(b,"WNV1",4);memcpy(b+4,banks,336);put32(b+340,crc(b,340));char tmp[512],bak[512];size_t len=strlen(filename);memcpy(tmp,filename,len);memcpy(tmp+len,".tmp",5);memcpy(bak,filename,len);memcpy(bak+len,".bak",5);
 FILE*f=fopen(tmp,"wb");if(!f)return -1;int ok=fwrite(b,1,sizeof b,f)==sizeof b;if(fflush(f))ok=0;if(fclose(f))ok=0;if(!ok){remove(tmp);return -1;}
 if(!rename(tmp,filename))return 0;
 /* Some libfat versions refuse rename over an existing destination. Keep
  * the old valid file until the new one is committed; recover .bak on load. */
 if(remove(bak)&&errno!=ENOENT){remove(tmp);return -1;}
 if(rename(filename,bak)){remove(tmp);return -1;}
 if(rename(tmp,filename)){rename(bak,filename);remove(tmp);return -1;}
 remove(bak);return 0;
}
uint8_t cdvd_config_write(const uint8_t in[16]){
 if(!active||mode!=1||block_index>=count)return 0x80;if(block_index>=blocks[bank])return 0;
 uint8_t old[16];memcpy(old,banks[bank][block_index],16);memcpy(banks[bank][block_index],in,16);
 if(persist()){memcpy(banks[bank][block_index],old,16);return 0x80;}block_index++;return 0;
}
void cdvd_config_snapshot_save(uint8_t out[CDVD_CONFIG_SNAPSHOT_SIZE]){memset(out,0,CDVD_CONFIG_SNAPSHOT_SIZE);out[0]=active;out[1]=mode;out[2]=bank;out[3]=count;out[4]=block_index;memcpy(out+8,banks,336);}
int cdvd_config_snapshot_valid(const uint8_t in[CDVD_CONFIG_SNAPSHOT_SIZE]){return in[0]<=1&&in[1]<=1&&in[2]<=2&&in[4]<=in[3]&&in[4]<=blocks[in[2]]&&!in[5]&&!in[6]&&!in[7]&&(in[0]||(!in[1]&&!in[2]&&!in[3]&&!in[4]));}
void cdvd_config_snapshot_load(const uint8_t in[CDVD_CONFIG_SNAPSHOT_SIZE]){active=in[0];mode=in[1];bank=in[2];count=in[3];block_index=in[4];memcpy(banks,in+8,336);}
