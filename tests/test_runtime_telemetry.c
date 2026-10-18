#include "core/hw/runtime_telemetry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
 runtime_telemetry_config c;
 assert(runtime_telemetry_parse("GEKKO2_TELEMETRY 0\n",&c)==0&&!c.ip&&!c.port);
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
 if(argc==2){packet[7]=0;FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(packet,1,RT_PACKET_BYTES,f)==RT_PACKET_BYTES);assert(!fclose(f));}
 puts("PASS telemetry default-off/strict LAN config, uint64 endian encoding, packet bounds, bounded FIFO full/wrap/empty");
}
