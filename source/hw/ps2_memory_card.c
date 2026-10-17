/* Independent scalar PS2 card device. Wire/spec references: PCSX2
 * 091ab687 SioTypes/MemoryCardProtocol; PS2SDK 329e2bb mcsio2 and
 * MCDevInfo/McDataChecksum (public protocol and parity equations).
 * No authentication keys, upstream filesystem or CPU backend copied. */
#include "core/hw/ps2_memory_card.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static uint8_t *state;
static char filename[256];
static uint32_t cursor,sector,mode,dirty,readonly,raw_format,path_hash;
static uint8_t terminator=0x55;
static uint8_t *image(void){return state+32;}
static uint32_t rd(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void wr(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
void ps2_mc_ecc128(const uint8_t in[128],uint8_t out[3])
{
 unsigned columns=0,odd=0,even=0;
 for(unsigned i=0;i<128;i++){
  unsigned code=0;
  /* Eight parity columns; linearity replaces a copied 256-byte table. */
  for(unsigned bit=0;bit<8;bit++)if(in[i]&(1u<<bit))code^=0x87u+15u*bit;
  columns^=code;if(code&128u){odd^=~i;even^=i;}
 }
 out[0]=(uint8_t)(~columns&0x77u);out[1]=(uint8_t)(~odd&0x7fu);out[2]=(uint8_t)(~even&0x7fu);
}
static void page_ecc(unsigned page)
{
 uint8_t *p=image()+page*PS2_MC_PAGE_BYTES;
 for(unsigned i=0;i<4;i++)ps2_mc_ecc128(p+i*128u,p+512u+i*3u);
}
int ps2_mc_present(void){return state!=NULL;}
void ps2_mc_reset_protocol(void){cursor=sector=mode=0;terminator=0x55;}
int ps2_mc_flush(void)
{
 if(!state||!dirty)return 0;
 if(readonly||!filename[0])return -1;
 char temp[264],backup[264];snprintf(temp,sizeof temp,"%s.tmp",filename);snprintf(backup,sizeof backup,"%s.bak",filename);
 FILE *f=fopen(temp,"wb");if(!f)return -1;
 int ok=1;
 for(unsigned p=0;p<PS2_MC_PAGES;p++){
  unsigned bytes=raw_format?512u:528u;
  if(fwrite(image()+p*528u,1,bytes,f)!=bytes){ok=0;break;}
 }
 if(fclose(f))ok=0;if(!ok){remove(temp);return -1;}
 if(rename(temp,filename)){
  remove(backup);if(rename(filename,backup)){remove(temp);return -1;}
  if(rename(temp,filename)){rename(backup,filename);remove(temp);return -1;}
 }
 remove(backup);dirty=0;return 0;
}
int ps2_mc_eject(void)
{
 if(ps2_mc_flush())return -1;
 free(state);state=NULL;filename[0]=0;path_hash=dirty=0;ps2_mc_reset_protocol();return 0;
}
int ps2_mc_mount(const char *path,int read_only)
{
 if(!path||strlen(path)>=sizeof filename)return -1;
 if(ps2_mc_flush())return -1;
 FILE *f=fopen(path,"rb");char backup[264];
 if(!f){snprintf(backup,sizeof backup,"%s.bak",path);f=fopen(backup,"rb");}
 if(!f)return -1;
 if(fseek(f,0,SEEK_END)){fclose(f);return -1;}long size=ftell(f);rewind(f);
 if(size!=PS2_MC_IMAGE_BYTES&&size!=PS2_MC_PAGES*512u){fclose(f);return -1;}
 uint8_t *next=malloc(32u+PS2_MC_IMAGE_BYTES);if(!next){fclose(f);return -1;}
 memset(next,0xff,32u+PS2_MC_IMAGE_BYTES);
 unsigned bytes=size==PS2_MC_IMAGE_BYTES?528u:512u;int ok=1;
 for(unsigned p=0;p<PS2_MC_PAGES;p++)if(fread(next+32u+p*528u,1,bytes,f)!=bytes){ok=0;break;}
 if(ferror(f))ok=0;fclose(f);
 if(!ok){free(next);return -1;}
 free(state);state=next;raw_format=bytes==512u;readonly=!!read_only;dirty=0;
 memcpy(filename,path,strlen(path)+1);path_hash=2166136261u;
 for(const unsigned char *p=(const unsigned char*)path;*p;p++)path_hash=(path_hash^*p)*16777619u;
 if(raw_format)for(unsigned p=0;p<PS2_MC_PAGES;p++)page_ecc(p);
 ps2_mc_reset_protocol();return 0;
}
static uint32_t logical32(uint32_t address)
{
 if(address>PS2_MC_PAGES*512u-4u)return UINT32_MAX;
 uint8_t bytes[4];for(unsigned i=0;i<4;i++)bytes[i]=image()[((address+i)/512u)*528u+(address+i)%512u];
 return rd(bytes);
}
int ps2_mc_get_info(unsigned *formatted,unsigned *free_clusters)
{
 if(formatted)*formatted=0;if(free_clusters)*free_clusters=0;
 if(!state)return -11;
 if(memcmp(image(),"Sony PS2 Memory Card Format ",28))return -2;
 unsigned page=image()[40]|((unsigned)image()[41]<<8),ppc=image()[42]|((unsigned)image()[43]<<8);
 unsigned clusters=logical32(48),offset=logical32(52),count=logical32(56);
 if(page!=512u||ppc!=2u||clusters!=8192u||offset>=clusters||count>clusters-offset||!count)return -13;
 unsigned free_count=0;
 for(unsigned i=0;i<count;i++){
  unsigned indirect=logical32(80u+(i/65536u)*4u);
  if(indirect>=clusters)return -13;
  unsigned fat=logical32(indirect*1024u+((i/256u)%256u)*4u);if(fat>=clusters)return -13;
  uint32_t entry=logical32(fat*1024u+(i%256u)*4u);
  if(!(entry&0x80000000u))free_count++;
 }
 if(formatted)*formatted=1;if(free_clusters)*free_clusters=free_count;return 0;
}
static void ack(uint8_t *out,unsigned n)
{memset(out,0,n);out[n-2]=0x2b;out[n-1]=terminator;}
void ps2_mc_transfer(const uint8_t *in,uint8_t *out,unsigned n)
{
 if(!out)return;memset(out,0xff,n);
 if(!state||!in||n<4u||in[0]!=0x81)return;
 unsigned cmd=in[1];
 if(cmd==0x11u||cmd==0x12u||cmd==0x81u){ack(out,4);return;}
 if(cmd==0x21u||cmd==0x22u||cmd==0x23u){
  if(n<9u)return;uint32_t value=rd(in+2);uint8_t sum=in[2]^in[3]^in[4]^in[5];
  if(in[6]!=sum||value>=PS2_MC_PAGES){mode=0;return;}
  sector=value;cursor=value*528u;mode=cmd;ack(out,9);return;
 }
 if(cmd==0x26u){
  if(n<13u)return;memset(out,0,13);out[2]=0x2b;out[4]=2;out[5]=16;out[8]=0x40;
  out[11]=0x52;out[12]=terminator;return; /* 512-byte page, 16 pages/block, 16384 pages + XOR */
 }
 if(cmd==0x27u){if(n<5u)return;terminator=in[2];ack(out,5);return;}
 if(cmd==0x28u){if(n<5u)return;out[0]=out[1]=0;out[2]=0x2b;out[3]=out[4]=terminator;return;}
 if(cmd==0x42u||cmd==0x43u){
  unsigned bytes=in[2];if(!bytes||n<bytes+6u||cursor>PS2_MC_IMAGE_BYTES-bytes||
   mode!=(cmd==0x42u?0x22u:0x23u)||(readonly&&cmd==0x42u))return;
  unsigned sum=0;
  if(cmd==0x42u){for(unsigned i=0;i<bytes;i++)sum^=in[3u+i];if(sum!=in[3u+bytes])return;}
  memset(out,0,bytes+6u);out[3]=0x2b;
  for(unsigned i=0;i<bytes;i++){
   if(cmd==0x42u)image()[cursor+i]&=in[3u+i];
   else{out[4u+i]=image()[cursor+i];sum^=out[4u+i];}
  }
  if(cmd==0x42u){dirty=1;if(raw_format){page_ecc(cursor/528u);page_ecc((cursor+bytes-1u)/528u);}}
  out[4u+bytes]=(uint8_t)sum;out[5u+bytes]=terminator;cursor+=bytes;return;
 }
 if(cmd==0x82u){
  if(readonly||mode!=0x21u)return;
  memset(image()+(sector&~15u)*528u,0xff,16u*528u);dirty=1;ack(out,4);return;
 }
 if(cmd==0xbfu||cmd==0xf3u||cmd==0xf7u){if(n<5u)return;if(cmd==0xf3u)terminator=0x55;ack(out,5);return;}
 /* MagicGate F0/F7 and unknown operations remain explicit unsupported. */
}
const void *ps2_mc_snapshot(uint32_t *bytes)
{
 static uint8_t empty[32];uint8_t *p=state?state:empty;
 memset(p,0,32);wr(p,0x31434d50u);wr(p+4,state?1u:0u);wr(p+8,cursor);wr(p+12,sector);
 wr(p+16,mode|((uint32_t)terminator<<8));wr(p+20,raw_format|(readonly<<1)|(dirty<<2));wr(p+24,path_hash);
 if(bytes)*bytes=state?32u+PS2_MC_IMAGE_BYTES:32u;return p;
}
int ps2_mc_snapshot_valid(const void *data,uint32_t bytes)
{
 if(!data||bytes<32u)return 0;const uint8_t *p=data;unsigned present=rd(p+4),m=rd(p+16)&255u;
 return rd(p)==0x31434d50u&&present<=1u&&bytes==(present?32u+PS2_MC_IMAGE_BYTES:32u)&&
  rd(p+8)<=PS2_MC_IMAGE_BYTES&&rd(p+12)<PS2_MC_PAGES&&(!m||m==0x21u||m==0x22u||m==0x23u)&&
  !(rd(p+16)&0xffff0000u)&&rd(p+20)<=7u&&((rd(p+20)&6u)!=6u)&&!rd(p+28)&&
  (!present||(state&&rd(p+24)==path_hash&&(rd(p+20)&3u)==(raw_format|(readonly<<1))));
}
int ps2_mc_snapshot_load(const void *data,uint32_t bytes)
{
 if(!ps2_mc_snapshot_valid(data,bytes))return -1;const uint8_t *p=data;
 uint8_t *next=NULL;if(rd(p+4)){next=malloc(bytes);if(!next)return -1;memcpy(next,p,bytes);}
 cursor=rd(p+8);sector=rd(p+12);mode=rd(p+16)&255u;terminator=(uint8_t)(rd(p+16)>>8);
 raw_format=rd(p+20)&1u;readonly=(rd(p+20)>>1)&1u;dirty=(rd(p+20)>>2)&1u;path_hash=rd(p+24);
 if(next&&!readonly)dirty=1; /* A restored mutable image must also restore persistent contents. */
 free(state);state=next;return 0;
}
