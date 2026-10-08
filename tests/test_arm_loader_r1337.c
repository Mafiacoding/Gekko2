/* Synthetic IOS transport; this test never talks to a console. */
#include "core/hw/arm_loader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
static uint8_t memory[65536];static unsigned cursor,writes,started;static int mode;
int IOS_Open(const char *p,int flags){(void)flags;assert(!strcmp(p,"/dev/mload"));return mode==1?-1:7;}
int IOS_Close(int fd){assert(fd==7);return 0;}
int iosCreateHeap(int n){assert(n==4096);return 2;}
int IOS_Seek(int fd,int where,int whence){assert(fd==7&&whence==SEEK_SET);assert((unsigned)where>=0x137f0000);cursor=(unsigned)where-0x137f0000;assert(cursor<sizeof memory);return where;}
int IOS_Read(int fd,void *p,int n){assert(fd==7&&cursor+n<=sizeof memory);memcpy(p,memory+cursor,n);cursor+=n;return mode==2?n-1:n;}
int IOS_Write(int fd,const void *p,int n){assert(fd==7&&cursor+n<=sizeof memory);writes++;memcpy(memory+cursor,p,n);cursor+=n;return mode==3?n-1:n;}
int IOS_IoctlvFormat(int heap,int fd,int request,const char *format,...)
{
 assert(heap==2&&fd==7);va_list a;va_start(a,format);
 if((unsigned)request==0x4d4c4490){assert(!strcmp(format,":ii"));*va_arg(a,unsigned*)=0x137f0000;*va_arg(a,unsigned*)=mode==4?1024:65536;}
 else{assert((unsigned)request==0x4d4c4482&&!strcmp(format,"iiii:"));assert(va_arg(a,unsigned)==0x137f0000);assert(va_arg(a,unsigned)==0x137f4000);assert(va_arg(a,unsigned)==0x3000);assert(va_arg(a,unsigned)==0x40);started++;}
 va_end(a);return 0;
}
static void wr(uint8_t *p,unsigned v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int main(int argc,char **argv)
{
 assert(argc==2);mode=atoi(argv[1]);FILE *f=fopen("sd:/pcsx2/arm/Gekko2-ARM-Worker.elf","rb");assert(f);
 fseek(f,0,SEEK_END);unsigned n=ftell(f);rewind(f);uint8_t *e=malloc(n),*copy=malloc(n);assert(fread(e,1,n,f)==n);fclose(f);
 arm_load_plan_t p;assert(arm_loader_validate(e,n,0x137f0000,65536,&p));unsigned count=p.count;
 for(unsigned cut=0;cut<52;cut++)assert(!arm_loader_validate(e,cut,0x137f0000,65536,&p));
 unsigned fields[]={16,18,20,24,28,32,40,42,44,46,48,50};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++){memcpy(copy,e,n);copy[fields[i]]=255;assert(!arm_loader_validate(copy,n,0x137f0000,65536,&p));}
 unsigned ph=(unsigned)e[28]<<24|(unsigned)e[29]<<16|(unsigned)e[30]<<8|e[31];
 for(unsigned i=0;i<count;i++)for(unsigned off=4;off<=20;off+=4){memcpy(copy,e,n);wr(copy+ph+i*32+off,0xffffffff);assert(!arm_loader_validate(copy,n,0x137f0000,65536,&p));}
 if(mode==5)memory[0x4000]=1; /* Occupied second segment: no first-segment writes. */
 int r=arm_loader_start();
 if(!mode){assert(r==1&&writes&&started==1);assert(!memcmp(memory,e+0x10000,4));}
 else{assert(r<0&&!started);if(mode!=3)assert(!writes);}
 free(e);free(copy);printf("PASS ARM loader mode=%d status=%d writes=%u started=%u\n",mode,r,writes,started);
}
