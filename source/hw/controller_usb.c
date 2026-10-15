/* GPL-3.0+. Nintendo WUP-028-compatible adapter only (057e:0337).
 * Protocol reference: ToadKing/wii-u-gc-adapter, EP 81/02, init 13,
 * report 21 + four nine-byte ports. No arbitrary HID descriptors guessed.
 * Single-flight asynchronous transfers: no USB read waits in the EE loop.
 * Static DMA buffers remain alive after disconnect/exit/IOS reload. */
#include "core/hw/controller_usb.h"
#ifdef GEKKO
#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/lwp_watchdog.h>
#include <string.h>
static s32 fd=-1;
static volatile int pending,ready,fault;
static int enabled,usb_ok;
static uint64_t next_probe;
static volatile uint64_t received;
static uint8_t input[64] ATTRIBUTE_ALIGN(32);
static uint8_t command[32] ATTRIBUTE_ALIGN(32);
static uint8_t snapshot[37];
static volatile int valid;
static s32 init_done(s32 result,void *user)
{
 (void)user;pending=0;
 if(!enabled||fd<0)return 0;
 if(result==1)ready=1;else fault=1;
 return 0;
}
static s32 read_done(s32 result,void *user)
{
 (void)user;
 if(enabled&&fd>=0){
  if(result==37&&input[0]==0x21){
   memcpy(snapshot,input,37);received=gettime();valid=1;
  }else if(result<0){fault=1;valid=0;}
 }
 pending=0;return 0;
}
void controller_usb_init(void)
{
 enabled=1;usb_ok=USB_Initialize()>=0;ready=fault=valid=0;next_probe=0;
 /* Never reuse an outstanding buffer until its callback releases it. */
}
void controller_usb_shutdown(void)
{
 enabled=0;ready=valid=0;
 if(fd>=0){USB_CloseDevice(&fd);fd=-1;}
 /* Do not deinitialize shared USB hosts: SD/USB game storage uses them. */
}
int controller_usb_status(void)
{ return !enabled?0:!usb_ok?-1:fault?-2:ready?2:fd>=0?1:0; }
void controller_usb_scan(controller_pad p[4])
{
 for(unsigned i=0;i<4;i++)controller_neutral(&p[i]);
 if(!enabled||!usb_ok)return;
 uint64_t now=gettime();
 if(fault&& !pending){if(fd>=0)USB_CloseDevice(&fd);fd=-1;fault=ready=valid=0;next_probe=now+secs_to_ticks(2);}
 if(fd<0&&!pending&&now>=next_probe){
  usb_device_entry devices[8];u8 count=0;
  next_probe=now+secs_to_ticks(2);
  if(USB_GetDeviceList(devices,8,USB_CLASS_HID,&count)>=0)
   for(unsigned i=0;i<count&&i<8;i++)if(devices[i].vid==0x057e&&devices[i].pid==0x0337){
    if(USB_OpenDevice(devices[i].device_id,0x057e,0x0337,&fd)<0){fd=-1;break;}
    command[0]=0x13;pending=1;
    if(USB_WriteIntrMsgAsync(fd,0x02,1,command,init_done,NULL)<0){pending=0;fault=1;}
    break;
   }
 }
 if(ready&&!pending){
  pending=1;
  if(USB_ReadIntrMsgAsync(fd,0x81,37,input,read_done,NULL)<0){pending=0;fault=1;valid=0;}
 }
 uint8_t copy[37];int have;u32 level=IRQ_Disable();
 have=valid&&ticks_to_millisecs(now-received)<500;
 if(have)memcpy(copy,snapshot,37);
 IRQ_Restore(level);
 if(have)controller_map_usb_gc(copy,37,p);
}
#else
void controller_usb_init(void){}
void controller_usb_shutdown(void){}
int controller_usb_status(void){return 0;}
void controller_usb_scan(controller_pad p[4])
{for(unsigned i=0;i<4;i++)controller_neutral(&p[i]);}
#endif
