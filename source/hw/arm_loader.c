/* GPL-3.0+. MLOAD wire interface follows Hermes' GPL-2.0-or-later
 * mload.c (2009), maintained in wiidev/usbloadergx/source/mload.
 * This is a bounded implementation, not an IOS installer. */
#include "core/hw/arm_loader.h"
#include <string.h>
static int status,validation_reason;
static uint32_t diagnostics[5]; /* file bytes, IOS base/size, ELF entry, CRC32 */
static uint32_t transport_operation,transport_address,transport_length;
static int transport_result;
uint32_t arm_loader_stat(unsigned n){return n<5?diagnostics[n]:n==5?(uint32_t)validation_reason:n==6?transport_operation:n==7?(uint32_t)transport_result:n==8?transport_address:n==9?transport_length:0;}
static int invalid(int reason){validation_reason=reason;return 0;}
static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static unsigned be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static int span(uint32_t start,uint32_t n,uint32_t base,uint32_t size){return start>=base&&n<=size&&start-base<=size-n;}
int arm_loader_status(void){return status;}
int arm_loader_validate(const uint8_t *e,unsigned n,uint32_t base,uint32_t capacity,arm_load_plan_t *p)
{
 int reason=1;validation_reason=0;
 if(!e||!p||n<52||n>1024*1024||!capacity||base>UINT32_MAX-capacity)return invalid(reason);
 memset(p,0,sizeof *p);
 if(memcmp(e,"\177ELF\1\2\1",7)||be16(e+16)!=2||be16(e+18)!=40||be32(e+20)!=1||be16(e+40)!=52)return invalid(reason);
 unsigned phnum=be16(e+44),shnum=be16(e+48),names=be16(e+50);
 uint32_t ph=be32(e+28),sh=be32(e+32);
 if(!phnum||phnum>16||be16(e+42)!=32||!span(ph,phnum*32,0,n)||!shnum||shnum>128||names>=shnum||be16(e+46)!=40||!span(sh,shnum*40,0,n))return invalid(reason);
 reason=2;
 p->entry=be32(e+24);int entry_ok=0;
 for(unsigned i=0;i<phnum;i++){
  const uint8_t *h=e+ph+i*32;if(be32(h)!=1)continue;
  if(p->count==ARM_LOADER_MAX_SEGMENTS)return invalid(reason);
  arm_load_segment_t *s=&p->segment[p->count++];
  s->offset=be32(h+4);s->address=be32(h+8);s->file_size=be32(h+16);s->memory_size=be32(h+20);s->flags=be32(h+24);
  if(!s->memory_size||s->file_size>s->memory_size||!span(s->offset,s->file_size,0,n)||!span(s->address,s->memory_size,base,capacity)||be32(h+12)!=s->address)return invalid(reason);
  if((s->flags&1)&&span(p->entry,4,s->address,s->file_size))entry_ok=1;
  for(unsigned j=0;j+1<p->count;j++){
   arm_load_segment_t *t=&p->segment[j];
   if(s->address<t->address+t->memory_size&&t->address<s->address+s->memory_size)return invalid(reason);
  }
 }
 reason=3;
 if(!entry_ok||(p->entry&3))return invalid(reason);
 reason=4;
 const uint8_t *nh=e+sh+names*40;uint32_t no=be32(nh+16),ns=be32(nh+20);
 if(be32(nh+4)!=3||!span(no,ns,0,n))return invalid(reason);
 unsigned found=0;
 for(unsigned i=0;i<shnum;i++){
  const uint8_t *h=e+sh+i*40;uint32_t name=be32(h);
  if(name>=ns)return invalid(reason);
  if(ns-name<16||memcmp(e+no+name,".ios_info_table",16))continue;
  uint32_t off=be32(h+16),size=be32(h+20);
  if(found++||be32(h+4)!=1||size!=52||!span(off,size,0,n))return invalid(reason);
  const uint8_t *m=e+off;
  if(be32(m)||be32(m+4)!=0x28||be32(m+8)!=6||be32(m+12)!=0xb||be32(m+16)!=4||be32(m+20)!=9||be32(m+24)!=p->entry||be32(m+28)!=0x7d||be32(m+36)!=0x7e||be32(m+44)!=0x7f)return invalid(reason);
  p->priority=be32(m+32);p->stack_size=be32(m+40);p->stack=be32(m+48);
 }
 reason=5;
 if(!found||p->priority>0x7f||p->stack_size<1024||p->stack_size>65536||(p->stack&31)||p->stack<p->stack_size)return invalid(reason);
 for(unsigned i=0;i<p->count;i++)if((p->segment[i].flags&2)&&span(p->stack-p->stack_size,p->stack_size,p->segment[i].address,p->segment[i].memory_size))return 1;
 return invalid(reason);
}
#ifdef GEKKO
#include <gccore.h>
#include <ogc/ipc.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
static int loader_heap=-1;
static int loader_seek(int fd,uint32_t address,unsigned length)
{
 transport_operation=1;transport_address=address;transport_length=length;
 transport_result=IOS_Seek(fd,address,SEEK_SET);return transport_result>=0;
}
/* Hermes clients accept nonnegative status, not necessarily a byte count.
 * Never infer completeness from that status: all-zero preflight checks every
 * poisoned byte and complemented write readback proves every target byte. */
static int loader_transfer_ok(int result,unsigned length)
{(void)length;return result>=0;}
int arm_loader_start(void)
{
 /* Loading is opt-in twice: ARM option and this separately installed ELF.
  * GET_LOAD_BASE authorizes only MLOAD's module-loading area. Every target
  * byte must also be zero before any write. No general IOS RAM writes. */
 memset(diagnostics,0,sizeof diagnostics);validation_reason=0;
 transport_operation=transport_address=transport_length=0;transport_result=0;
 FILE *f=fopen("sd:/pcsx2/arm/Gekko2-ARM-Worker.elf","rb");
 if(!f)return status=-1;
 uint8_t *elf=0;int fd=-1,hid=-1;status=-2;
 if(fseek(f,0,SEEK_END))goto finish;
 long size=ftell(f);if(size<52||size>1024*1024||fseek(f,0,SEEK_SET))goto finish;
 diagnostics[0]=(uint32_t)size;
 elf=memalign(32,(size+31)&~31);if(!elf||fread(elf,1,size,f)!=(unsigned)size)goto finish;
 diagnostics[3]=be32(elf+24);
 uint32_t crc=UINT32_MAX;for(unsigned i=0;i<(unsigned)size;i++){crc^=elf[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
 diagnostics[4]=~crc;
 fd=IOS_Open("/dev/mload",0);if(fd<0){status=-3;goto finish;}
 if(loader_heap<0)loader_heap=iosCreateHeap(4096);
 hid=loader_heap;if(hid<0){status=-4;goto finish;}
 uint32_t base=0,capacity=0;
 if(IOS_IoctlvFormat(hid,fd,0x4d4c4490,":ii",&base,&capacity)<0){status=-5;goto finish;}
 diagnostics[1]=base;diagnostics[2]=capacity;
 arm_load_plan_t plan;
 if(!arm_loader_validate(elf,size,base,capacity,&plan)){status=-6;goto finish;}
 uint8_t transfer[1024] __attribute__((aligned(32)));
 status=-7;
 for(unsigned i=0;i<plan.count;i++){
  arm_load_segment_t *s=&plan.segment[i];
  for(unsigned at=0;at<s->memory_size;){
   unsigned len=s->memory_size-at;if(len>sizeof transfer)len=sizeof transfer;
   if(!loader_seek(fd,s->address+at,len))goto finish;
   memset(transfer,0xa5,len);transport_operation=2;
   transport_result=IOS_Read(fd,transfer,len);
   if(!loader_transfer_ok(transport_result,len))goto finish;
   for(unsigned j=0;j<len;j++)if(transfer[j]){status=-8;goto finish;}at+=len;
  }
 }
 status=-9;
 for(unsigned i=0;i<plan.count;i++){
  arm_load_segment_t *s=&plan.segment[i];
  for(unsigned at=0;at<s->memory_size;){
   unsigned len=s->memory_size-at;if(len>sizeof transfer)len=sizeof transfer;memset(transfer,0,len);
   if(at<s->file_size){unsigned copy=s->file_size-at;if(copy>len)copy=len;memcpy(transfer,elf+s->offset+at,copy);}
   if(!loader_seek(fd,s->address+at,len))goto finish;
   transport_operation=3;transport_result=IOS_Write(fd,transfer,len);
   if(!loader_transfer_ok(transport_result,len))goto finish;
   uint8_t verify[1024] __attribute__((aligned(32)));
   if(!loader_seek(fd,s->address+at,len))goto finish;
   for(unsigned j=0;j<len;j++)verify[j]=(uint8_t)~transfer[j];
   transport_operation=4;transport_result=IOS_Read(fd,verify,len);
   if(!loader_transfer_ok(transport_result,len)||memcmp(verify,transfer,len)) {status=-11;goto finish;}
   at+=len;
  }
 }
 status=IOS_IoctlvFormat(hid,fd,0x4d4c4482,"iiii:",plan.entry,plan.stack,plan.stack_size,plan.priority)<0?-10:1;
finish:
 if(fd>=0)IOS_Close(fd);
 free(elf);fclose(f);return status;
}
#else
int arm_loader_start(void){return status=-3;}
#endif
