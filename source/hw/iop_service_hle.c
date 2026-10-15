/* R1353 optional ABI-level SIO2 service replacement. Ordinals and register
 * contracts: ps2dev/ps2sdk iop/sio/sio2man/include/sio2man.h and src/sio2man.c.
 * No guest import is rewritten. Only loaded, registered, unchanged exports
 * are intercepted. Unhandled transfer/IRQ/thread operations keep real code. */
#include "core/hw/iop_service_hle.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/arm_ram.h"
#include "core/recompiler/optimization.h"
#include <string.h>
#define SERVICE_FIRST 4u
#define SERVICE_LAST 20u
#define SERVICE_WORDS 16u
typedef struct {uint32_t pc,words[SERVICE_WORDS];unsigned ordinal,family;} service_t;
/* 17 SIO2 services plus 13 explicitly supported sysclib ordinals. */
static service_t services[30];
static unsigned service_count;
static uint64_t stats[10];
void iop_service_hle_reset(void){memset(services,0,sizeof services);service_count=0;memset(stats,0,sizeof stats);}
uint64_t iop_service_hle_stat(unsigned index){return index<10u?stats[index]:0;}
static uint32_t service_word(const iop_state_t *st,uint32_t pc)
{
 const uint8_t *p=st->ram+(pc&0x1fffffffu);
 return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int ram_function(const iop_state_t *st,uint32_t pc)
{
 unsigned segment=pc>>29;
 uint32_t phys=pc&0x1fffffffu;
 return !(pc&3u)&&(segment==0u||segment==4u||segment==5u)&&st->ram&&
  st->ram_size>=SERVICE_WORDS*4u&&phys<=st->ram_size-SERVICE_WORDS*4u;
}
void iop_service_hle_register(iop_state_t *st,const char *library,uint32_t table,unsigned count)
{
 unsigned family,first,last;
 if(!library)return;
 if(!strncmp(library,"sio2man",8)){family=0;first=SERVICE_FIRST;last=SERVICE_LAST;}
 else if(!strncmp(library,"sysclib",8)){family=1;first=10;last=32;}
 else return;
 /* Replacing one library never discards unrelated registered services. */
 unsigned kept=0;
 for(unsigned k=0;k<service_count;k++)if(services[k].family!=family)services[kept++]=services[k];
 service_count=kept;
 for(unsigned ordinal=first;ordinal<=last&&ordinal<count;ordinal++){
  if(family==1u&&!(ordinal==10u||ordinal==11u||(ordinal>=12u&&ordinal<=15u)||
     ordinal==21u||ordinal==22u||(ordinal>=25u&&ordinal<=27u)||ordinal==29u||ordinal==32u))continue;
  if(service_count>=sizeof services/sizeof services[0])break;
  uint32_t pc=iop_mem_read32(st,table+ordinal*4u);
  if(!pc||!ram_function(st,pc))continue;
  service_t *s=&services[service_count++];s->pc=pc;s->ordinal=ordinal;s->family=family;
  for(unsigned k=0;k<SERVICE_WORDS;k++)s->words[k]=service_word(st,pc+k*4u);
 }
 stats[0]=service_count;
}
/* Pure normal RAM only. MMIO, ROM, mirrors outside the real RAM allocation,
 * oversized work, overlapping memcpy and self-modifying function bodies keep
 * the original guest implementation. memmove explicitly supports overlap. */
static uint8_t *service_ram(iop_state_t *st,uint32_t address,uint32_t bytes)
{
 unsigned segment=address>>29;uint32_t offset=address&0x1fffffffu;
 if((segment!=0u&&segment!=4u&&segment!=5u)||!st->ram||
    offset>=st->ram_size||bytes>st->ram_size-offset)return 0;
 return st->ram+offset;
}
static int service_bulk(iop_state_t *st,const service_t *s)
{
 if(!gekko2_opt_enabled(GEKKO2_OPT_HLE_RAM))return 0;
 uint32_t destination=st->gpr[4],source=st->gpr[5],bytes=st->gpr[6];
 if(bytes>65536u){stats[6]++;return 0;}
 uint8_t *dst=service_ram(st,destination,bytes);
 uint8_t *src=s->ordinal==14u?0:service_ram(st,source,bytes);
 uint32_t d=destination&0x1fffffffu,f=s->pc&0x1fffffffu;
 if(!dst||(s->ordinal!=14u&&!src)||(bytes&&d<f+64u&&f<d+bytes)){
  stats[6]++;return 0;
 }
 if(s->ordinal==12u&&bytes){
  uint32_t a=source&0x1fffffffu;
  if(d<a+bytes&&a<d+bytes){stats[6]++;return 0;}
 }
 if(bytes){
  int arm_done=0;
  if(gekko2_opt_enabled(GEKKO2_OPT_ARM_CPU_RAM)&&bytes>=65536u){
   if(s->ordinal==14u)arm_done=arm_ram_fill(dst,(uint8_t)source,bytes);
   else {
    uint32_t a=source&0x1fffffffu;
    if(d>=a+bytes||a>=d+bytes)arm_done=arm_ram_copy(dst,src,bytes);
   }
  }
  if(!arm_done){
   if(s->ordinal==14u)memset(dst,(int)(source&255u),bytes);
   else if(s->ordinal==13u)memmove(dst,src,bytes);
   else memcpy(dst,src,bytes);
  }
 }
 st->gpr[2]=destination;st->gpr[0]=0;st->gpr_generation++;
 st->pc=st->gpr[31];st->next_pc=st->pc+4u;
 stats[1]++;stats[4]++;stats[5]+=bytes;return 1;
}
/* Read-only sysclib calls. All reads stay inside real allocated IOP RAM,
 * with a 64-KiB cap; a missing terminator or crossing the cap falls through
 * without altering guest registers. Guest pointer aliases are preserved. */
static int service_read(iop_state_t *st,const service_t *s)
{
 if(!gekko2_opt_enabled(GEKKO2_OPT_HLE_RAM))return 0;
 uint32_t a=st->gpr[4],b=st->gpr[5],n=st->gpr[6],result=0;
 uint64_t read_bytes=0;
 if(s->ordinal==10u||s->ordinal==11u||s->ordinal==15u){
  if(n>65536u)goto decline;
  const uint8_t *left=n?service_ram(st,a,n):0;
  const uint8_t *right=(n&&s->ordinal!=10u)?service_ram(st,b,n):0;
  if(n&&(!left||(s->ordinal!=10u&&!right)))goto decline;
  for(uint32_t k=0;k<n;k++){
   read_bytes++;
   if(s->ordinal==10u){if(left[k]==(uint8_t)b){result=a+k;break;}}
   else {read_bytes++;if(left[k]!=right[k]){result=(uint32_t)((int)left[k]-(int)right[k]);break;}}
  }
 }else{
  /* strncmp(n=0) does not dereference either argument. */
  if(s->ordinal==29u&&!n)goto publish;
  const uint8_t *left=service_ram(st,a,1),*right=0;
  if(!left)goto decline;
  uint32_t limit=st->ram_size-(a&0x1fffffffu);
  if(limit>65536u)limit=65536u;
  int compare=s->ordinal==22u||s->ordinal==29u;
  if(compare){
   right=service_ram(st,b,1);if(!right)goto decline;
   uint32_t available=st->ram_size-(b&0x1fffffffu);
   if(available<limit)limit=available;
  }
  if(s->ordinal==29u){if(n>65536u)goto decline;if(n<limit)limit=n;}
  int finished=0;
  for(uint32_t k=0;k<limit;k++){
   uint8_t c=left[k];read_bytes++;
   if(compare){
    uint8_t d=right[k];read_bytes++;
    if(c!=d||!c){result=(uint32_t)((int)c-(int)d);finished=1;break;}
   }else if(s->ordinal==27u){if(!c){result=k;finished=1;break;}}
   else {
    int last=s->ordinal==26u||s->ordinal==32u;
    if(c==(uint8_t)b){result=a+k;if(!last){finished=1;break;}}
    if(!c){finished=1;break;}
   }
  }
  if(!finished&&!(s->ordinal==29u&&limit==n))goto decline;
 }
 publish:
 st->gpr[2]=result;st->gpr[0]=0;st->gpr_generation++;
 st->pc=st->gpr[31];st->next_pc=st->pc+4u;
 stats[1]++;stats[7]++;stats[8]+=read_bytes;return 1;
 decline:
 stats[6]++;stats[9]++;return 0;
}

int iop_service_hle_try_handle(iop_state_t *st,uint32_t pc)
{
 if(!gekko2_opt_enabled(GEKKO2_OPT_IOP_SERVICE_HLE)||!service_count||
    st->load_delay_reg||st->branch_delay_pending||st->pipe_active)return 0;
 const service_t *s=0;
 for(unsigned k=0;k<service_count;k++)if(services[k].pc==pc){s=&services[k];break;}
 if(!s)return 0;
 if(!ram_function(st,pc)){stats[2]++;return 0;}
 for(unsigned k=0;k<SERVICE_WORDS;k++)if(service_word(st,pc+k*4u)!=s->words[k]){stats[2]++;return 0;}
 if(s->family==1u)return s->ordinal>=12u&&s->ordinal<=14u?service_bulk(st,s):service_read(st,s);
 uint32_t address=0,value=st->gpr[4],result=0;int write=0,byte=0;
 unsigned n=st->gpr[4];
 switch(s->ordinal){
 case 4:address=0x1f808268u;write=1;break;
 case 5:address=0x1f808268u;break;
 case 6:address=0x1f80826cu;break;
 case 7:case 8:case 9:case 10:
  if(n>=4u){stats[3]++;return 0;}
  address=0x1f808240u+n*8u+((s->ordinal>=9u)?4u:0u);
  write=s->ordinal==7u||s->ordinal==9u;value=st->gpr[5];break;
 case 11:address=0x1f808270u;break;
 case 12:case 13:
  if(n>=16u){stats[3]++;return 0;}
  address=0x1f808200u+n*4u;write=s->ordinal==12u;value=st->gpr[5];break;
 case 14:address=0x1f808274u;break;
 case 15:case 16:address=0x1f808278u;write=s->ordinal==15u;break;
 case 17:case 18:address=0x1f80827cu;write=s->ordinal==17u;break;
 case 19:address=0x1f808260u;write=1;byte=1;break;
 case 20:address=0x1f808264u;byte=1;break;
 default:return 0;
 }
 if(write){if(byte)iop_mem_write8(st,address,(uint8_t)value);else iop_mem_write32(st,address,value);}
 else if(byte)result=iop_mem_read8(st,address);else result=iop_mem_read32(st,address);
 if(!write){st->gpr[2]=result;st->gpr_generation++;}
 st->gpr[0]=0;st->pc=st->gpr[31];st->next_pc=st->pc+4u;
 stats[1]++;return 1;
}
