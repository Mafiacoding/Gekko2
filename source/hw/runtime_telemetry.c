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
 unsigned on,a,b,c,d,port;int used=0;
 if(!text||!out)return -1;
 memset(out,0,sizeof *out);
 int off=0;sscanf(text,"GEKKO2_TELEMETRY 0 %n",&off);
 if(off&&!text[off])return 0;
 if(sscanf(text,"GEKKO2_TELEMETRY %1u %3u.%3u.%3u.%3u %5u %n",&on,&a,&b,&c,&d,&port,&used)!=6||!used||text[used]||on>1)return -1;
 if(a>255||b>255||c>255||d>255||!port||port>65535)return -1;
 if(!(a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168)))return -1;
 out->ip=(a<<24)|(b<<16)|(c<<8)|d;out->port=(uint16_t)port;return (int)on;
}
int runtime_telemetry_load(const char *path,runtime_telemetry_settings *s)
{
 *s=(runtime_telemetry_settings){{0,26890},0};
 FILE *f=fopen(path,"r");if(!f)return -2;
 char buf[128];size_t n=fread(buf,1,sizeof buf-1,f);int ok=!ferror(f)&&feof(f);fclose(f);buf[n]=0;
 runtime_telemetry_config endpoint;int enabled=ok&&strlen(buf)==n?runtime_telemetry_parse(buf,&endpoint):-1;
 if(enabled<0)return -1;
 s->enabled=enabled;if(endpoint.ip)s->endpoint=endpoint;return enabled;
}
static int settings_text(char *text,size_t size,const runtime_telemetry_settings *s)
{
 if(s->enabled!=0&&s->enabled!=1)return -1;
 uint32_t ip=s->endpoint.ip;
 int n=(!s->enabled&&!ip)?snprintf(text,size,"GEKKO2_TELEMETRY 0\n"):
 snprintf(text,size,"GEKKO2_TELEMETRY %d %u.%u.%u.%u %u\n",s->enabled,
 (unsigned)(ip>>24),(unsigned)((ip>>16)&255),(unsigned)((ip>>8)&255),(unsigned)(ip&255),(unsigned)s->endpoint.port);
 runtime_telemetry_config checked;
 return n>0&&(size_t)n<size&&runtime_telemetry_parse(text,&checked)>=0?0:-1;
}
int runtime_telemetry_save(const char *path,const runtime_telemetry_settings *s)
{
 char text[128],temp[320],backup[320];
 if(settings_text(text,sizeof text,s)||snprintf(temp,sizeof temp,"%s.tmp",path)>=(int)sizeof temp||snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return -1;
 FILE *f=fopen(temp,"w");if(!f)return -1;
 int ok=fputs(text,f)>=0;if(fclose(f))ok=0;
 if(!ok){remove(temp);return -1;}
 if(!rename(temp,path)){remove(backup);return 0;}
 remove(backup);if(rename(path,backup)){remove(temp);return -1;}
 if(rename(temp,path)){rename(backup,path);remove(temp);return -1;}
 remove(backup);return 0;
}
int runtime_telemetry_adjust(runtime_telemetry_settings *s,unsigned row,int delta)
{
 if(!delta)return 0;
 if(row==RT_EDIT_ENABLE){s->enabled=!s->enabled;return 1;}
 if(row>=RT_EDIT_IP1&&row<=RT_EDIT_IP4){
  unsigned shift=(RT_EDIT_IP4-row)*8;int64_t value=(int64_t)((s->endpoint.ip>>shift)&255)+delta;
  if(value<0)value=0;
  if(value>255)value=255;
  s->endpoint.ip=(s->endpoint.ip&~(255u<<shift))|((uint32_t)value<<shift);return 1;
 }
 if(row==RT_EDIT_PORT){int64_t value=(int64_t)s->endpoint.port+delta;
  if(value<1)value=1;
  if(value>65535)value=65535;
  s->endpoint.port=(uint16_t)value;return 1;}
 return 0;
}
const char *runtime_telemetry_status_text(const runtime_telemetry_status *s)
{
 if(!s->state)return s->config_reason==RT_CONFIG_INVALID?"INVALID CONFIG - OFF":s->config_reason==RT_CONFIG_MISSING?"NO CONFIG - OFF":"LOGGER OFF";
 if(s->state==1)return "NETWORK STARTING";
 if(s->state==3)return "NETWORK FAILED - SEE ERROR";
 return s->acknowledged?"PC RECEIPT CONFIRMED":"SOCKET READY - NO PC ACK";
}
const char *runtime_telemetry_stage_text(int stage)
{
 static const char *names[]={"config","thread","net_init","DHCP","socket","nonblock","ready","send"};
 return stage>=0&&stage<8?names[stage]:"unknown";
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
static uint64_t read64(const uint8_t *p)
{uint64_t v=0;for(unsigned i=0;i<8;i++)v=(v<<8)|p[i];return v;}
int runtime_telemetry_ack_matches(const uint8_t *p,size_t n,uint64_t boot,uint64_t minimum,uint64_t maximum)
{
 return p&&n==RT_ACK_BYTES&&!memcmp(p,"GK2A",4)&&read64(p+4)==boot&&
  read64(p+12)>=minimum&&read64(p+12)<=maximum;
}
static runtime_telemetry_settings requested={{0,26890},0};
static runtime_telemetry_status status;
#ifdef GEKKO
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <fcntl.h>
#include <unistd.h>
static runtime_telemetry_queue queue;
static unsigned generation;
static int thread_created;
static uint64_t boot_id,sequence,minimum_ack,last_sent;
static int have_sent;
static lwp_t thread;
static uint8_t stack[16384] __attribute__((aligned(32)));
void runtime_telemetry_get_settings(runtime_telemetry_settings *s)
{uint32_t irq=IRQ_Disable();*s=requested;IRQ_Restore(irq);}
void runtime_telemetry_get_status(runtime_telemetry_status *s)
{uint32_t irq=IRQ_Disable();*s=status;IRQ_Restore(irq);}
static void progress(int state,int stage,int error,unsigned gen)
{
 uint32_t irq=IRQ_Disable();
 if(generation==gen){status.state=state;status.stage=stage;status.error=error;}
 IRQ_Restore(irq);
}
static void *sender(void *arg)
{
 (void)arg;int fd=-1,net_ready=0;unsigned attempt=0;
 struct sockaddr_in addr;memset(&addr,0,sizeof addr);
 for(;;){
  uint32_t irq=IRQ_Disable();unsigned gen=generation;runtime_telemetry_settings cfg=requested;IRQ_Restore(irq);
  if(!cfg.enabled){if(fd>=0){net_close(fd);fd=-1;}usleep(10000);continue;}
  if(attempt!=gen){
   attempt=gen;if(fd>=0){net_close(fd);fd=-1;}
   progress(1,RT_STAGE_NET,0,gen);int rc=0;
   if(!net_ready){rc=net_init();if(rc>=0)net_ready=1;}
   if(rc<0){progress(3,RT_STAGE_NET,rc,gen);continue;}
   progress(1,RT_STAGE_DHCP,0,gen);char ip[16],mask[16],gateway[16];
   rc=if_config(ip,mask,gateway,true,3);
   if(rc<0){progress(3,RT_STAGE_DHCP,rc,gen);continue;}
   irq=IRQ_Disable();if(generation==gen)status.local_ip=ntohl(inet_addr(ip));IRQ_Restore(irq);
   progress(1,RT_STAGE_SOCKET,0,gen);fd=net_socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
   if(fd<0){progress(3,RT_STAGE_SOCKET,fd,gen);continue;}
   rc=net_fcntl(fd,F_SETFL,O_NONBLOCK);
   if(rc<0){net_close(fd);fd=-1;progress(3,RT_STAGE_NONBLOCK,rc,gen);continue;}
   addr.sin_family=AF_INET;addr.sin_len=sizeof addr;addr.sin_port=htons(cfg.endpoint.port);addr.sin_addr.s_addr=htonl(cfg.endpoint.ip);
   progress(2,RT_STAGE_READY,0,gen);
   continue; /* Recheck a changed/disabled target before consuming queued data. */
  }
  if(fd<0){usleep(10000);continue;}
  uint8_t packet[RT_PACKET_BYTES];irq=IRQ_Disable();int have=runtime_telemetry_pop(&queue,packet);IRQ_Restore(irq);
  if(have){
   int n=net_sendto(fd,packet,sizeof packet,0,(struct sockaddr*)&addr,sizeof addr);
   irq=IRQ_Disable();
   if(n==(int)sizeof packet){status.sent++;if(generation==gen){last_sent=read64(packet+128+RT_SEQ*8);have_sent=1;}}
   else{status.errors++;if(generation==gen){status.error=n;status.stage=RT_STAGE_SEND;}}
   IRQ_Restore(irq);
  }
  for(unsigned tries=0;tries<4;tries++){
   uint8_t ack[RT_ACK_BYTES+1];struct sockaddr_in peer;socklen_t length=sizeof peer;
   int n=net_recvfrom(fd,ack,sizeof ack,0,(struct sockaddr*)&peer,&length);if(n<0)break;
   irq=IRQ_Disable();
   if(generation==gen&&have_sent&&length==sizeof peer&&peer.sin_family==AF_INET&&
      peer.sin_addr.s_addr==addr.sin_addr.s_addr&&peer.sin_port==addr.sin_port&&
      runtime_telemetry_ack_matches(ack,(size_t)n,boot_id,minimum_ack,last_sent))status.acknowledged=1;
   IRQ_Restore(irq);
  }
  if(!have)usleep(10000);
 }
 return NULL;
}
int runtime_telemetry_apply(const runtime_telemetry_settings *s)
{
 char text[128];if(settings_text(text,sizeof text,s))return -1;
 uint32_t irq=IRQ_Disable();requested=*s;generation++;
 status.dropped+=queue.count;memset(&queue,0,sizeof queue);status.acknowledged=0;status.error=0;have_sent=0;minimum_ack=sequence;
 status.config_reason=s->enabled?RT_CONFIG_ON:RT_CONFIG_OFF;status.stage=RT_STAGE_CONFIG;status.state=s->enabled?1:0;
 if(!boot_id)boot_id=gettime();
 int create=s->enabled&&!thread_created;
 if(create)thread_created=1;
 IRQ_Restore(irq);
 if(create){int rc=LWP_CreateThread(&thread,sender,NULL,stack,sizeof stack,10);
  if(rc<0){irq=IRQ_Disable();thread_created=0;status.state=3;status.stage=RT_STAGE_THREAD;status.error=rc;IRQ_Restore(irq);return -1;}}
 return 0;
}
void runtime_telemetry_start(const char *path)
{
 runtime_telemetry_settings s;int rc=runtime_telemetry_load(path,&s);
 if(rc<0){status.config_reason=rc==-2?RT_CONFIG_MISSING:RT_CONFIG_INVALID;return;}
 runtime_telemetry_apply(&s);
}
int runtime_telemetry_state(void){runtime_telemetry_status s;runtime_telemetry_get_status(&s);return s.state;}
void runtime_telemetry_submit(runtime_telemetry_sample *s)
{
 uint32_t irq=IRQ_Disable();
 if(status.state!=1&&status.state!=2){IRQ_Restore(irq);return;}
 s->value[RT_BOOT]=boot_id;s->value[RT_SEQ]=sequence++;
 s->value[RT_DROPPED]=status.dropped;s->value[RT_ERRORS]=status.errors;s->value[RT_SENT]=status.sent;IRQ_Restore(irq);
 uint8_t packet[RT_PACKET_BYTES];if(runtime_telemetry_encode(packet,sizeof packet,s)!=sizeof packet)return;
 irq=IRQ_Disable();if(!runtime_telemetry_push(&queue,packet))status.dropped++;IRQ_Restore(irq);
}
int runtime_telemetry_test(void)
{
 uint32_t irq=IRQ_Disable();
 if(status.state!=1&&status.state!=2){IRQ_Restore(irq);return -1;}
 if(queue.count==RT_QUEUE){IRQ_Restore(irq);return -2;}
 status.acknowledged=0;minimum_ack=sequence;have_sent=0;IRQ_Restore(irq);
 runtime_telemetry_sample sample={0};sample.value[RT_EVENT]=6;
 runtime_telemetry_submit(&sample);return 0;
}
#else
void runtime_telemetry_get_settings(runtime_telemetry_settings *s){*s=requested;}
void runtime_telemetry_get_status(runtime_telemetry_status *s){*s=status;}
int runtime_telemetry_apply(const runtime_telemetry_settings *s)
{char text[128];if(settings_text(text,sizeof text,s))return -1;requested=*s;return 0;}
void runtime_telemetry_start(const char *path){runtime_telemetry_settings s;if(runtime_telemetry_load(path,&s)>=0)requested=s;}
int runtime_telemetry_state(void){return 0;}
void runtime_telemetry_submit(runtime_telemetry_sample *s){(void)s;}
int runtime_telemetry_test(void){return -1;}
#endif
