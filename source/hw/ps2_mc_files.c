/* Standard PS2 8 MiB filesystem HLE. Layout/protocol reference: PS2SDK
 * 329e2bb43287e16af16c4dde1c614e2662c24468, MCDevInfo/McFsEntry and
 * libmc-common.h. Independent bounded implementation, no host-directory
 * emulation, implicit formatting, authentication keys or copied backend. */
#include "core/hw/ps2_mc_files.h"
#include "core/hw/ps2_memory_card.h"
#include "core/hw/iop_cdvd.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define EOC UINT32_MAX
#define CLUSTERS 8192u
#define EXISTS 0x8000u
#define DIR 0x20u
#define FILE_MODE 0x10u
typedef struct {uint32_t active,mode,entry,pos;} handle_t;
typedef struct {
 uint32_t magic;handle_t handles[16];char cwd[1024],pattern[1024];uint32_t cursor;
} fs_state_t;
static fs_state_t fs;
static struct {unsigned offset,count,root;uint32_t fat[32];} geo;
/* Single EE HLE thread; bounded shared scratch avoids a 16 KiB Wii stack. */
static uint16_t chain[CLUSTERS];
static uint8_t visited[CLUSTERS/8];
static unsigned chain_count;
static uint16_t directory_queue[CLUSTERS];
static uint8_t owned[CLUSTERS/8];
static uint32_t r32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned r16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static void w32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static void w16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static uint32_t read32(unsigned at){uint8_t p[4];return ps2_mc_logical_read(at,p,4)?EOC:r32(p);}
static void write32(unsigned at,uint32_t n){uint8_t p[4];w32(p,n);ps2_mc_logical_write(at,p,4);}
static unsigned data_at(unsigned c){return (geo.offset+c)*1024u;}
static unsigned fat_at(unsigned c){return geo.fat[c/256u]*1024u+(c%256u)*4u;}
static uint32_t fat_get(unsigned c){return read32(fat_at(c));}
static void fat_set(unsigned c,uint32_t n){write32(fat_at(c),n);}
void ps2_mc_fs_reset(void){memset(&fs,0,sizeof fs);fs.magic=0x3146534du;fs.cwd[0]='/';}
int ps2_mc_fs_busy(void)
{for(unsigned i=0;i<16;i++)if(fs.handles[i].active)return 1;return 0;}
static int geometry(void)
{
 uint8_t b[512];if(!ps2_mc_present())return -11;
 if(ps2_mc_logical_read(0,b,sizeof b))return -13;
 if(memcmp(b,"Sony PS2 Memory Card Format ",28))return -2;
 geo.offset=r32(b+52);geo.count=r32(b+56);geo.root=r32(b+60);
 if(r16(b+40)!=512||r16(b+42)!=2||r32(b+48)!=8192||geo.offset<2||
    geo.offset>=8192||!geo.count||geo.count>8192-geo.offset||geo.root>=geo.count)return -13;
 unsigned indirect=r32(b+80);if(!indirect||indirect>=geo.offset)return -13;
 for(unsigned i=0;i<(geo.count+255)/256;i++){
  geo.fat[i]=read32(indirect*1024+i*4);
  if(!geo.fat[i]||geo.fat[i]>=geo.offset||geo.fat[i]==indirect)return -13;
  for(unsigned j=0;j<i;j++)if(geo.fat[i]==geo.fat[j])return -13;
 }
 if(!fs.magic)ps2_mc_fs_reset();return 0;
}
static int load_chain(unsigned first)
{
 chain_count=0;memset(visited,0,sizeof visited);
 if(first==EOC)return 0;
 for(unsigned c=first;;){
  if(c>=geo.count||(visited[c/8]&(1u<<(c%8))))return -13;
  visited[c/8]|=(uint8_t)(1u<<(c%8));chain[chain_count++]=(uint16_t)c;
  uint32_t next=fat_get(c);if(!(next&0x80000000u))return -13;
  if(next==EOC)return 0;c=next&0x7fffffffu;
 }
}
static int nth(unsigned first,unsigned index,unsigned *at)
{
 if(index>=geo.count)return -13;
 unsigned c=first;
 for(unsigned i=0;i<index;i++){
  if(c>=geo.count)return -13;uint32_t next=fat_get(c);
  if(next==EOC||!(next&0x80000000u))return -13;c=next&0x7fffffffu;
 }
 if(c>=geo.count||!(fat_get(c)&0x80000000u))return -13;*at=data_at(c);return 0;
}
/* Validate and admit all required clusters before touching FAT/data. */
static int resize(unsigned *first,unsigned count)
{
 int r=load_chain(*first);if(r)return r;if(count>geo.count)return -3;
 unsigned old=chain_count,n=old;
 for(unsigned c=0;n<count&&c<geo.count;c++)if(!(fat_get(c)&0x80000000u))chain[n++]=(uint16_t)c;
 if(n<count)return -3;
 for(unsigned i=count;i<old;i++)fat_set(chain[i],0x7fffffffu);
 uint8_t zero[1024]={0};
 for(unsigned i=old;i<count;i++)ps2_mc_logical_write(data_at(chain[i]),zero,sizeof zero);
 for(unsigned i=0;i<count;i++)fat_set(chain[i],i+1<count?0x80000000u|chain[i+1]:EOC);
 *first=count?chain[0]:EOC;return 0;
}
static int dir_meta(unsigned dir,unsigned *meta)
{
 unsigned at=0;if(nth(dir,0,&at))return -13;uint8_t e[512];ps2_mc_logical_read(at,e,512);
 if((r16(e)&(EXISTS|DIR))!=(EXISTS|DIR)||e[64]!='.'||e[65])return -13;
 if(dir==geo.root){*meta=at;return 0;}
 unsigned slot=r32(e+20);if(slot>=geo.count*2u||nth(r32(e+16),slot/2,&at))return -13;
 at+=slot%2u*512u;ps2_mc_logical_read(at,e,512);
 if((r16(e)&(EXISTS|DIR))!=(EXISTS|DIR)||r32(e+16)!=dir)return -13;*meta=at;return 0;
}
static int dir_slots(unsigned dir,unsigned *slots)
{
 int r=load_chain(dir);if(r)return r;unsigned capacity=chain_count*2u,at=0;
 if(dir_meta(dir,&at))return -13;*slots=read32(at+4);
 return *slots>=2&&*slots<=capacity?0:-13;
}
static int slot_at(unsigned dir,unsigned slot,unsigned *at)
{int r=nth(dir,slot/2,at);if(!r)*at+=slot%2u*512u;return r;}
static int own_chain(unsigned first,unsigned size,int directory)
{
 int r=load_chain(first);if(r)return r;
 if((directory&&(!chain_count||size<2||size>chain_count*2u))||(!directory&&size>chain_count*1024u))return -13;
 for(unsigned i=0;i<chain_count;i++){
  unsigned c=chain[i];if(owned[c/8]&(1u<<(c%8)))return -13;owned[c/8]|=(uint8_t)(1u<<(c%8));
 }
 return 0;
}
/* Read-only admission sweep before mutation: cycles, cross-links, invalid
 * lengths and directory parent pointers must not damage an imported image. */
static int validate_tree(void)
{
 memset(owned,0,sizeof owned);unsigned slots;
 int r=dir_slots(geo.root,&slots);if(r)return r;r=own_chain(geo.root,slots,1);if(r)return r;
 unsigned next=0,count=1;directory_queue[0]=(uint16_t)geo.root;
 while(next<count){
  unsigned dir=directory_queue[next++];r=dir_slots(dir,&slots);if(r)return r;
  for(unsigned i=2;i<slots;i++){
   unsigned at=0;uint8_t e[512];if(slot_at(dir,i,&at))return -13;ps2_mc_logical_read(at,e,512);
   unsigned mode=r16(e);if(!(mode&EXISTS))continue;
   unsigned kind=mode&(DIR|FILE_MODE);if(kind!=DIR&&kind!=FILE_MODE)return -13;
   unsigned first=r32(e+16);r=own_chain(first,r32(e+4),kind==DIR);if(r)return r;
   if(kind==DIR){unsigned meta=0;if(count>=CLUSTERS||dir_meta(first,&meta)||meta!=at)return -13;directory_queue[count++]=(uint16_t)first;}
  }
 }
 return 0;
}
static int find(unsigned dir,const char *name,unsigned *at,uint8_t entry[512])
{
 unsigned slots;int r=dir_slots(dir,&slots);if(r)return r;
 for(unsigned i=0;i<slots;i++){
  if(slot_at(dir,i,at))return -13;ps2_mc_logical_read(*at,entry,512);
  if((r16(entry)&EXISTS)&&strlen(name)<=32u&&!strncmp((char*)entry+64,name,32))return 0;
 }
 return -4;
}
static int normalize(const char *name,char out[1024])
{
 if(!name)return -5;if(!strncmp(name,"mc0:",4))name+=4;
 if(strchr(name,':')||strlen(name)>1023)return -5;
 if(!fs.magic)ps2_mc_fs_reset();
 if(*name=='/')strcpy(out,"/");else strcpy(out,fs.cwd);
 while(*name){
  while(*name=='/')name++;if(!*name)break;
  const char *start=name;while(*name&&*name!='/')name++;unsigned n=(unsigned)(name-start);
  if(n==1&&*start=='.')continue;
  if(n==2&&start[0]=='.'&&start[1]=='.'){
   char *p=strrchr(out,'/');if(p==out)out[1]=0;else if(p)*p=0;continue;
  }
  if(!n||n>32)return -5;unsigned len=(unsigned)strlen(out),slash=len>1;
  if(len+slash+n>=1024)return -5;if(slash)out[len++]='/';memcpy(out+len,start,n);out[len+n]=0;
 }
 return 0;
}
/* Walk all but final component; no unbounded directory recursion. */
static int parent(const char *path,unsigned *dir,char leaf[33])
{
 *dir=geo.root;const char *p=path+1;
 if(!*p){strcpy(leaf,".");return 0;}
 while(*p){const char *end=strchr(p,'/');unsigned n=end?(unsigned)(end-p):(unsigned)strlen(p);
  memcpy(leaf,p,n);leaf[n]=0;if(!end)return 0;
  unsigned at;uint8_t e[512];int r=find(*dir,leaf,&at,e);if(r)return r;
  if(!(r16(e)&DIR))return -5;*dir=r32(e+16);p=end+1;
 }
 return -5;
}
static int resolve(const char *name,unsigned *at,uint8_t e[512],unsigned *dir,char leaf[33])
{char path[1024];int r=normalize(name,path);if(r)return r;r=parent(path,dir,leaf);return r?r:find(*dir,leaf,at,e);}
static void stamp(uint8_t date[8])
{
 uint8_t clock[8]={0};memset(date,0,8);if(!iop_cdvd_read_clock(clock))return;
 for(unsigned i=1;i<=3;i++)date[i]=(uint8_t)((clock[i]>>4)*10+(clock[i]&15));
 date[4]=(uint8_t)((clock[5]>>4)*10+(clock[5]&15));date[5]=(uint8_t)((clock[6]>>4)*10+(clock[6]&15));
 w16(date+6,2000u+(clock[7]>>4)*10+(clock[7]&15));
}
static void entry_init(uint8_t e[512],const char *name,unsigned mode,unsigned first,unsigned size)
{memset(e,0,512);w16(e,mode);w32(e+4,size);w32(e+16,first);memcpy(e+64,name,strlen(name));stamp(e+8);memcpy(e+24,e+8,8);}
static int new_slot(unsigned dir,unsigned *at,unsigned *slot)
{
 unsigned meta=0;if(dir_meta(dir,&meta))return -13;
 uint8_t mode[2];ps2_mc_logical_read(meta,mode,2);if(!(r16(mode)&2u))return -5;
 unsigned slots;int r=dir_slots(dir,&slots);if(r)return r;uint8_t e[512];
 for(unsigned i=2;i<slots;i++){
  slot_at(dir,i,at);ps2_mc_logical_read(*at,e,512);if(!(r16(e)&EXISTS)){*slot=i;return 0;}
 }
 unsigned first=dir;r=resize(&first,(slots+2)/2);if(r)return r;
 *slot=slots;write32(meta+4,slots+1);return slot_at(dir,slots,at);
}
int ps2_mc_fs_format(int unformat)
{
 if(!ps2_mc_present())return -11;if(!ps2_mc_writable()||ps2_mc_fs_busy())return -5;
 if(ps2_mc_erase_all())return -5;ps2_mc_fs_reset();if(unformat)return ps2_mc_flush()?-5:0;
 /* Standard legal layout: IFC8, FAT16..47, data64..8175, backup blocks1022/3. */
 uint8_t b[512];memset(b,0xff,sizeof b);memcpy(b,"Sony PS2 Memory Card Format ",28);
 memset(b+28,0,12);memcpy(b+28,"1.2.0.0",7);w16(b+40,512);w16(b+42,2);w16(b+44,16);
 w32(b+48,8192);w32(b+52,64);w32(b+56,8112);w32(b+60,0);w32(b+64,1023);w32(b+68,1022);
 w32(b+80,8);b[336]=2;b[337]=0x2b;ps2_mc_logical_write(0,b,512);
 for(unsigned i=0;i<32;i++)write32(8*1024+i*4,16+i);
 for(unsigned i=0;i<8192;i++)write32(16*1024+i*4,i?0x7fffffffu:EOC);
 geometry();entry_init(b,".",0x8427,0,2);ps2_mc_logical_write(data_at(0),b,512);
 entry_init(b,"..",0xa426,0,0);ps2_mc_logical_write(data_at(0)+512,b,512);
 return ps2_mc_flush()? -5:0;
}
static handle_t *handle(int fd,unsigned need)
{if(fd<0||fd>=16||!fs.handles[fd].active||(fs.handles[fd].mode&need)!=need)return NULL;return fs.handles+fd;}
static int open_entry(int fd,unsigned need,handle_t **h,uint8_t e[512])
{
 int r=geometry();if(r)return r;*h=handle(fd,need);if(!*h)return -5;
 if(ps2_mc_logical_read((*h)->entry,e,512)||(r16(e)&(EXISTS|FILE_MODE))!=(EXISTS|FILE_MODE))return -13;
 r=load_chain(r32(e+16));if(r||r32(e+4)>chain_count*1024u)return -13;return 0;
}
int ps2_mc_fs_open(const char *name,unsigned flags)
{
 int r=geometry();if(r)return r;
 if(flags&~0x3e43u)return -5;if(flags&0x200u)flags|=2u;
 if(!(flags&(3u|0x40u)))return -5;
 if((flags&(2u|0x240u))&&!ps2_mc_writable())return -5;
 if(flags&(2u|0x240u)){r=validate_tree();if(r)return r;}
 unsigned at=0,dir=0,slot=0;char leaf[33];uint8_t e[512];r=resolve(name,&at,e,&dir,leaf);
 if(r&&r!=-4)return r;if(strchr(leaf,'*')||strchr(leaf,'?'))return -5;
 int fd=0;while(fd<16&&fs.handles[fd].active)fd++;
 if(!(flags&0x40u)&&fd==16)return -7;
 if(r==-4){
  if(!(flags&(0x200u|0x40u)))return -4;if(!strcmp(leaf,".")||!strcmp(leaf,".."))return -5;
  /* Directory allocation is admitted before extending the parent. */
  unsigned first=EOC;
  if(flags&0x40u){r=resize(&first,1);if(r)return r;}
  r=new_slot(dir,&at,&slot);if(r){if(first!=EOC)resize(&first,0);return r;}
  entry_init(e,leaf,((flags&0x40u)?0x8427u:0x8417u)|(flags&0x3800u),first,(flags&0x40u)?2u:0u);
  ps2_mc_logical_write(at,e,512);
  if(flags&0x40u){
   entry_init(e,".",0x8427,dir,0);w32(e+20,slot);ps2_mc_logical_write(data_at(first),e,512);
   uint8_t dot[512];ps2_mc_logical_read(data_at(dir),dot,512);
   entry_init(e,"..",0x8427,r32(dot+16),0);w32(e+20,r32(dot+20));ps2_mc_logical_write(data_at(first)+512,e,512);return 0;
  }
 }else{
  if((flags&0x40u)||(r16(e)&DIR)||!(r16(e)&FILE_MODE))return -5;
  for(unsigned i=0;i<16;i++)if(fs.handles[i].active&&fs.handles[i].entry==at&&((flags|fs.handles[i].mode)&2))return -5;
 }
 if(!(flags&3u))return -5;
 if(((flags&1u)&&!(r16(e)&1u))||((flags&2u)&&(!(r16(e)&2u)||!ps2_mc_writable())))return -5;
 unsigned first=r32(e+16);r=load_chain(first);if(r||r32(e+4)>chain_count*1024u)return -13;
 /* libmc O_CREAT replaces an existing file, unlike POSIX O_CREAT. */
 if(flags&0x200u){r=resize(&first,0);if(r)return r;w32(e+4,0);w32(e+16,EOC);ps2_mc_logical_write(at,e,512);}
 fs.handles[fd]=(handle_t){1,flags&3u,at,0};return fd;
}
int ps2_mc_fs_close(int fd)
{handle_t *h=handle(fd,0);if(!h)return -5;
 if((h->mode&2u)&&ps2_mc_writable()){uint8_t e[512];ps2_mc_logical_read(h->entry,e,512);w16(e,r16(e)|0x80u);ps2_mc_logical_write(h->entry,e,512);}
 if(ps2_mc_flush())return -5;memset(fs.handles+fd,0,sizeof(handle_t));return 0;}
int ps2_mc_fs_flush(int fd){return handle(fd,0)?(ps2_mc_flush()?-5:0):-5;}
int ps2_mc_fs_seek(int fd,int32_t offset,unsigned origin)
{
 handle_t *h;uint8_t e[512];int r=open_entry(fd,0,&h,e);if(r)return r;if(origin>2)return -5;
 int64_t p=(origin==0?0:origin==1?h->pos:r32(e+4))+(int64_t)offset;
 if(p<0||p>INT_MAX)return -5;h->pos=(uint32_t)p;return (int)p;
}
int ps2_mc_fs_read_size(int fd,unsigned bytes)
{
 handle_t *h;uint8_t e[512];int r=open_entry(fd,1,&h,e);if(r)return r;
 unsigned n=r32(e+4);if(h->pos>=n)return 0;n-=h->pos;if(n>bytes)n=bytes;return (int)n;
}
int ps2_mc_fs_read(int fd,void *dest,unsigned bytes)
{
 int n=ps2_mc_fs_read_size(fd,bytes);if(n<=0)return n;
 handle_t *h=handle(fd,1);uint8_t e[512];ps2_mc_logical_read(h->entry,e,512);unsigned left=(unsigned)n;uint8_t *p=dest;
 while(left){unsigned at=0;int r=nth(r32(e+16),h->pos/1024,&at);if(r)return r;
  unsigned len=1024-h->pos%1024;if(len>left)len=left;
  ps2_mc_logical_read(at+h->pos%1024,p,len);p+=len;left-=len;h->pos+=len;}
 return n;
}
int ps2_mc_fs_write(int fd,const void *src,unsigned bytes)
{
 handle_t *h;uint8_t e[512];int r=open_entry(fd,2,&h,e);if(r)return r;if(!ps2_mc_writable())return -5;
 r=validate_tree();if(r)return r;
 if(bytes>geo.count*1024u||h->pos>geo.count*1024u-bytes)return -3;if(!bytes)return 0;
 unsigned size=r32(e+4),end=h->pos+bytes,first=r32(e+16);if(end>size){
  r=resize(&first,(end+1023)/1024);if(r)return r;
  /* Explicitly zero a seek-created hole, including previously allocated tails. */
  uint8_t zero[1024]={0};unsigned pos=size;
  while(pos<h->pos){unsigned at=0;nth(first,pos/1024,&at);unsigned len=1024-pos%1024;if(len>h->pos-pos)len=h->pos-pos;
   ps2_mc_logical_write(at+pos%1024,zero,len);pos+=len;}
 }
 const uint8_t *p=src;unsigned left=bytes;
 while(left){unsigned at=0;nth(first,h->pos/1024,&at);unsigned len=1024-h->pos%1024;if(len>left)len=left;
  ps2_mc_logical_write(at+h->pos%1024,p,len);left-=len;p+=len;h->pos+=len;}
 w32(e+16,first);if(end>size)w32(e+4,end);stamp(e+24);ps2_mc_logical_write(h->entry,e,512);return (int)bytes;
}
int ps2_mc_fs_delete(const char *name)
{
 int r=geometry();if(r)return r;if(!ps2_mc_writable())return -5;
 r=validate_tree();if(r)return r;
 unsigned at,dir;char leaf[33],path[1024];uint8_t e[512];r=resolve(name,&at,e,&dir,leaf);if(r)return r;
 if(!strcmp(leaf,".")||!strcmp(leaf,".."))return -5;
 for(unsigned i=0;i<16;i++)if(fs.handles[i].active&&fs.handles[i].entry==at)return -5;
 if(r16(e)&DIR){
  normalize(name,path);unsigned len=(unsigned)strlen(path);if(!strncmp(fs.cwd,path,len)&&(!fs.cwd[len]||fs.cwd[len]=='/'))return -5;
  unsigned slots;r=dir_slots(r32(e+16),&slots);if(r)return r;
  for(unsigned i=2;i<slots;i++){unsigned ea=0;uint8_t child[512];slot_at(r32(e+16),i,&ea);ps2_mc_logical_read(ea,child,512);if(r16(child)&EXISTS)return -6;}
 }
 unsigned first=r32(e+16);r=resize(&first,0);if(r)return r;w16(e,r16(e)&~EXISTS);ps2_mc_logical_write(at,e,512);return 0;
}
int ps2_mc_fs_chdir(const char *name,char out[1024])
{
 int r=geometry();if(r)return r;unsigned at,dir;char leaf[33],path[1024];uint8_t e[512];
 r=normalize(name,path);if(r)return r;r=resolve(path,&at,e,&dir,leaf);if(r)return r;if(!(r16(e)&DIR))return -5;
 strcpy(fs.cwd,path);memset(out,0,1024);strcpy(out,path);return 0;
}
static int wildcard(const char *pattern,const char *name)
{
 const char *star=NULL,*retry=NULL;
 while(*name){if(*pattern=='?'||*pattern==*name){pattern++;name++;}
  else if(*pattern=='*'){star=pattern++;retry=name;}
  else if(star){pattern=star+1;name=++retry;}else return 0;}
 while(*pattern=='*')pattern++;return !*pattern;
}
static void pack(uint8_t out[64],const uint8_t e[512])
{memset(out,0,64);memcpy(out,e+8,8);memcpy(out+8,e+24,8);if(!(r16(e)&DIR))memcpy(out+16,e+4,4);memcpy(out+20,e,4);memcpy(out+24,e+32,4);memcpy(out+32,e+64,32);}
int ps2_mc_fs_getdir(const char *pattern,unsigned flags,unsigned count,uint8_t *table)
{
 int r=geometry();if(r)return r;if(flags>1)return -5;
 if(!flags){r=normalize(pattern,fs.pattern);if(r)return r;fs.cursor=0;}
 if(!fs.pattern[0])return -5;unsigned dir;char leaf[33];r=parent(fs.pattern,&dir,leaf);if(r)return r;
 unsigned slots;r=dir_slots(dir,&slots);if(r)return r;unsigned n=0;
 while(fs.cursor<slots&&n<count){unsigned at=0;uint8_t e[512];char name[33];slot_at(dir,fs.cursor++,&at);ps2_mc_logical_read(at,e,512);
  memcpy(name,e+64,32);name[32]=0;if((r16(e)&EXISTS)&&wildcard(leaf,name))pack(table+64*n++,e);}
 return (int)n;
}
int ps2_mc_fs_setinfo(const char *name,unsigned flags,const uint8_t table[64])
{
 int r=geometry();if(r)return r;if(!ps2_mc_writable())return -5;
 r=validate_tree();if(r)return r;
 /* libmc SetInfo: creation1, modification2, attributes4, rename0x10. */
 if(flags&~0x21fu)return -5;unsigned at,dir;char leaf[33];uint8_t e[512];r=resolve(name,&at,e,&dir,leaf);if(r)return r;
 if(!strcmp(leaf,".")||!strcmp(leaf,".."))return -5;
 if(flags&0x10u){char dest[33];memcpy(dest,table+32,32);dest[32]=0;
  if(!dest[0]||strchr(dest,'/')||strchr(dest,':')||strchr(dest,'*')||strchr(dest,'?')||!strcmp(dest,".")||!strcmp(dest,".."))return -5;
  unsigned other;uint8_t b[512];r=find(dir,dest,&other,b);if(!r&&other!=at)return -8;if(r&&r!=-4)return r;
  if(r16(e)&DIR){/* Renaming the cwd/ancestor would invalidate relative paths. */
   char path[1024];normalize(name,path);unsigned len=(unsigned)strlen(path);if(!strncmp(fs.cwd,path,len)&&(!fs.cwd[len]||fs.cwd[len]=='/'))return -5;}
  memset(e+64,0,32);memcpy(e+64,dest,strlen(dest));
 }
 if(flags&1)memcpy(e+8,table,8);if(flags&2)memcpy(e+24,table+8,8);
 if(flags&(4u|0x200u))w16(e,(r16(e)&~0x180fu)|(r16(table+20)&0x180fu));
 if(flags&8)memcpy(e+32,table+24,4);ps2_mc_logical_write(at,e,512);return 0;
}
const void *ps2_mc_fs_snapshot(uint32_t *bytes)
{if(!fs.magic)ps2_mc_fs_reset();*bytes=sizeof fs;return &fs;}
int ps2_mc_fs_snapshot_valid(const void *data,uint32_t bytes)
{
 if(!data||bytes!=sizeof(fs))return 0;const fs_state_t *p=data;
 if(p->magic!=0x3146534du||p->cwd[0]!='/'||!memchr(p->cwd,0,1024)||!memchr(p->pattern,0,1024)||p->cursor>16384)return 0;
 for(unsigned i=0;i<16;i++){
  const handle_t *h=p->handles+i;if(h->active>1)return 0;
  if(h->active&&(!h->mode||h->mode>3||h->entry<1024||h->entry>=8388608||h->entry%512||h->pos>INT_MAX))return 0;
 }
 return 1;
}
int ps2_mc_fs_snapshot_load(const void *data,uint32_t bytes)
{if(!ps2_mc_fs_snapshot_valid(data,bytes))return -1;memcpy(&fs,data,bytes);return 0;}
