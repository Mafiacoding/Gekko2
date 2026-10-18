#include "core/hw/runtime_telemetry.h"
#include "build_version.h"
#include <stdio.h>
#include <string.h>
#ifndef GEKKO2_TELEMETRY_COMMIT
#define GEKKO2_TELEMETRY_COMMIT "unknown"
#endif
_Static_assert(RT_PACKET_BYTES==1016u,"update receiver wire schema explicitly");
int runtime_telemetry_parse(const char *text,runtime_telemetry_config *out)
{
 unsigned a,b,c,d,port;int used=0;
 if(!text||!out)return -1;
 memset(out,0,sizeof *out);
 if(!strcmp(text,"GEKKO2_TELEMETRY 0")||!strcmp(text,"GEKKO2_TELEMETRY 0\n"))return 0;
 if(sscanf(text,"GEKKO2_TELEMETRY 1 %3u.%3u.%3u.%3u %5u %n",&a,&b,&c,&d,&port,&used)!=5||!used||text[used])return -1;
 if(a>255||b>255||c>255||d>255||!port||port>65535)return -1;
 /* Explicit RFC1918 unicast destination, never a public service or discovery. */
 if(!(a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168)))return -1;
 out->ip=(a<<24)|(b<<16)|(c<<8)|d;out->port=(uint16_t)port;return 1;
}
static void be64(uint8_t **p,uint64_t v)
{for(unsigned i=0;i<8;i++)*(*p)++=(uint8_t)(v>>(56-8*i));}
size_t runtime_telemetry_encode(uint8_t *out,size_t size,const runtime_telemetry_sample *s)
{
 if(!out||!s||size<RT_PACKET_BYTES)return 0;
 memset(out,0,RT_PACKET_BYTES);memcpy(out,"GK2T",4);out[4]=1;
 snprintf((char*)out+8,80,"%s",GEKKO2_RELEASE);
 memcpy(out+88,GEKKO2_TELEMETRY_COMMIT,sizeof(GEKKO2_TELEMETRY_COMMIT)-1<40?sizeof(GEKKO2_TELEMETRY_COMMIT)-1:40);
 uint8_t *p=out+128;
 for(unsigned i=0;i<RT_FIELDS;i++)be64(&p,s->value[i]);
 const gekko2_profile *scope[2]={&s->cpu,&s->present};
 for(unsigned k=0;k<2;k++){
  for(unsigned i=0;i<GP_COUNT;i++)be64(&p,scope[k]->ticks[i]);
  for(unsigned i=0;i<GP_COUNT;i++)be64(&p,scope[k]->entries[i]);
  for(unsigned i=0;i<GP_COUNT;i++)be64(&p,scope[k]->work[i]);
  be64(&p,scope[k]->samples);be64(&p,scope[k]->overflows);
 }
 return (size_t)(p-out);
}
int runtime_telemetry_push(runtime_telemetry_queue *q,const uint8_t *p)
{
 if(q->count==RT_QUEUE)return 0;
 memcpy(q->data[(q->head+q->count)%RT_QUEUE],p,RT_PACKET_BYTES);q->count++;return 1;
}
int runtime_telemetry_pop(runtime_telemetry_queue *q,uint8_t *p)
{
 if(!q->count)return 0;
 memcpy(p,q->data[q->head],RT_PACKET_BYTES);q->head=(q->head+1)%RT_QUEUE;q->count--;return 1;
}
#ifdef GEKKO
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <fcntl.h>
#include <unistd.h>
static runtime_telemetry_config target;
static runtime_telemetry_queue queue;
static volatile int state;
static uint64_t boot_id,sequence,dropped,errors,sent;
static lwp_t thread;
static uint8_t stack[16384] __attribute__((aligned(32)));
static void *sender(void *arg)
{
 (void)arg;char ip[16],mask[16],gateway[16];
 if(net_init()<0||if_config(ip,mask,gateway,true,3)<0){state=3;return NULL;}
 int fd=net_socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
 if(fd<0){state=3;return NULL;}
 if(net_fcntl(fd,F_SETFL,O_NONBLOCK)<0){net_close(fd);state=3;return NULL;}
 struct sockaddr_in addr;memset(&addr,0,sizeof addr);
 addr.sin_family=AF_INET;addr.sin_len=sizeof addr;addr.sin_port=htons(target.port);addr.sin_addr.s_addr=htonl(target.ip);
 state=2;
 for(;;){
  uint8_t packet[RT_PACKET_BYTES];uint32_t irq=IRQ_Disable();
  int have=runtime_telemetry_pop(&queue,packet);IRQ_Restore(irq);
  if(!have){usleep(10000);continue;}
  int n=net_sendto(fd,packet,sizeof packet,0,(struct sockaddr*)&addr,sizeof addr);
  irq=IRQ_Disable();if(n==(int)sizeof packet)sent++;else errors++;IRQ_Restore(irq);
 }
 return NULL;
}
void runtime_telemetry_start(const char *path)
{
 if(state)return; /* Startup only; no IOS reload or live transport reset. */
 FILE *f=fopen(path,"r");if(!f)return;
 char buf[128];size_t n=fread(buf,1,sizeof buf-1,f);int ok=!ferror(f)&&feof(f);fclose(f);buf[n]=0;
 if(!ok||strlen(buf)!=n||runtime_telemetry_parse(buf,&target)!=1)return;
 boot_id=gettime();state=1;
 if(LWP_CreateThread(&thread,sender,NULL,stack,sizeof stack,10)<0)state=3;
}
int runtime_telemetry_state(void){return state;}
void runtime_telemetry_submit(runtime_telemetry_sample *s)
{
 if(state!=1&&state!=2)return;
 uint8_t packet[RT_PACKET_BYTES];s->value[RT_BOOT]=boot_id;s->value[RT_SEQ]=sequence++;
 uint32_t irq=IRQ_Disable();s->value[RT_DROPPED]=dropped;s->value[RT_ERRORS]=errors;s->value[RT_SENT]=sent;IRQ_Restore(irq);
 if(runtime_telemetry_encode(packet,sizeof packet,s)!=sizeof packet)return;
 irq=IRQ_Disable();if(!runtime_telemetry_push(&queue,packet))dropped++;IRQ_Restore(irq);
}
#else
void runtime_telemetry_start(const char *path){(void)path;}
int runtime_telemetry_state(void){return 0;}
void runtime_telemetry_submit(runtime_telemetry_sample *s){(void)s;}
#endif
