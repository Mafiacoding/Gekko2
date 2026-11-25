#include "core/hw/runtime_telemetry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
 runtime_telemetry_config c;
 assert(runtime_telemetry_parse("GEKKO2_TELEMETRY 0\n",&c)==0&&!c.ip&&!c.port);
 assert(runtime_telemetry_parse("GEKKO2_TELEMETRY 0\r\n",&c)==0);
 const char *bad[]={"","GEKKO2_TELEMETRY 1 8.8.8.8 26890","GEKKO2_TELEMETRY 1 127.0.0.1 26890","GEKKO2_TELEMETRY 1 192.168.1.2 65536","GEKKO2_TELEMETRY 1 192.168.1.2 0","GEKKO2_TELEMETRY 1 192.168.1.999 26890","GEKKO2_TELEMETRY 1 192.168.1.2 26890 extra","GEKKO2_TELEMETRY 1 192.168.1.2 4294994186"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(runtime_telemetry_parse(bad[i],&c)==-1&&!c.ip&&!c.port);
 assert(runtime_telemetry_parse("GEKKO2_TELEMETRY 1 192.168.1.2 26890\n",&c)==1&&c.ip==0xc0a80102&&c.port==26890);
 runtime_telemetry_sample s={0};s.value[RT_BOOT]=0x123456789abcdef0ull;s.value[RT_MASK]=0x787dff;s.value[RT_SESSION]=2;s.value[RT_EPOCH]=1;
 for(unsigned i=0;i<GP_COUNT;i++){s.cpu.ticks[i]=i+1;s.cpu.entries[i]=i+20;s.cpu.work[i]=(1ull<<40)+i;s.present.ticks[i]=100+i;}
 s.cpu.samples=99;s.cpu.overflows=3;
 uint8_t packet[RT_PACKET_BYTES+2],copy[RT_PACKET_BYTES];memset(packet,0xa5,sizeof packet);
 assert(!runtime_telemetry_encode(packet,RT_PACKET_BYTES-1,&s)&&packet[0]==0xa5);
 assert(runtime_telemetry_encode(packet,RT_PACKET_BYTES,&s)==RT_PACKET_BYTES&&packet[RT_PACKET_BYTES]==0xa5);
 assert(!memcmp(packet,"GK2T\1\0\0\0",8));assert(packet[128]==0x12&&packet[135]==0xf0);
 runtime_telemetry_queue q={0};assert(!runtime_telemetry_pop(&q,copy));
 for(unsigned i=0;i<2000;i++){
  for(unsigned j=0;j<RT_QUEUE;j++){packet[7]=(uint8_t)j;assert(runtime_telemetry_push(&q,packet));}
  assert(!runtime_telemetry_push(&q,packet));
  for(unsigned j=0;j<RT_QUEUE;j++){assert(runtime_telemetry_pop(&q,copy)&&copy[7]==j);}
  assert(!runtime_telemetry_pop(&q,copy));
 }
 uint8_t ack[RT_ACK_BYTES]={0};memcpy(ack,"GK2A",4);
 memcpy(ack+4,packet+128,8);ack[19]=7;
 assert(runtime_telemetry_ack_matches(ack,sizeof ack,s.value[RT_BOOT],7,9));
 assert(!runtime_telemetry_ack_matches(ack,sizeof ack-1,s.value[RT_BOOT],7,9));
 assert(!runtime_telemetry_ack_matches(ack,sizeof ack,s.value[RT_BOOT]+1,7,9));
 assert(!runtime_telemetry_ack_matches(ack,sizeof ack,s.value[RT_BOOT],8,9));
 assert(!runtime_telemetry_ack_matches(ack,sizeof ack,s.value[RT_BOOT],0,6));
 ack[0]='X';assert(!runtime_telemetry_ack_matches(ack,sizeof ack,s.value[RT_BOOT],0,9));
 runtime_telemetry_settings settings={{0xc0a80102,26890},1},loaded;
 assert(runtime_telemetry_adjust(&settings,RT_EDIT_IP4,-1000)&&settings.endpoint.ip==0xc0a80100);
 assert(runtime_telemetry_adjust(&settings,RT_EDIT_IP4,1000)&&settings.endpoint.ip==0xc0a801ff);
 assert(runtime_telemetry_adjust(&settings,RT_EDIT_PORT,-100000)&&settings.endpoint.port==1);
 assert(runtime_telemetry_adjust(&settings,RT_EDIT_PORT,100000)&&settings.endpoint.port==65535);
 assert(!runtime_telemetry_adjust(&settings,RT_EDIT_SAVE,1));
 assert(runtime_telemetry_adjust(&settings,RT_EDIT_ENABLE,1)&&!settings.enabled);
 char path[128];snprintf(path,sizeof path,"/tmp/gekko2-net-settings-%lu.cfg",(unsigned long)s.value[RT_BOOT]);
 assert(!runtime_telemetry_save(path,&settings));assert(runtime_telemetry_load(path,&loaded)==0);
 assert(!loaded.enabled&&loaded.endpoint.ip==settings.endpoint.ip&&loaded.endpoint.port==65535);
 settings.enabled=1;assert(!runtime_telemetry_save(path,&settings));assert(runtime_telemetry_load(path,&loaded)==1&&loaded.enabled);
 assert(!runtime_telemetry_apply(&settings));runtime_telemetry_get_settings(&loaded);assert(loaded.endpoint.ip==settings.endpoint.ip);
 settings.endpoint.ip=0x08080808;assert(runtime_telemetry_save(path,&settings)==-1);
 assert(runtime_telemetry_load(path,&loaded)==1&&loaded.endpoint.ip==0xc0a801ff);remove(path);
 assert(runtime_telemetry_load(path,&loaded)==-2&&!loaded.enabled);
 if(argc==2){packet[7]=0;FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(packet,1,RT_PACKET_BYTES,f)==RT_PACKET_BYTES);assert(!fclose(f));}
 puts("PASS telemetry default-off/strict LAN config, uint64 endian encoding, packet bounds, bounded FIFO full/wrap/empty; ACK boot/sequence rejection and IP/port editor/save validation");
}
