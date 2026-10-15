/* Fake USB transport exercises real backend state transitions, not hardware. */
#include "core/hw/controller_usb.h"
#include <gccore.h>
#include <ogc/usb.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t now;static unsigned lists,opens,reads,writes,closes;
static int absent,fail_submit;static void *buffer;
static usbcallback callback;static void *userdata;
uint64_t gettime(void){return now;}
s32 USB_Initialize(void){return 0;}
s32 USB_CloseDevice(s32 *fd){closes++;*fd=-1;return 0;}
s32 USB_GetDeviceList(usb_device_entry *d,u8 n,u8 cls,u8 *cnt)
{assert(n==8&&cls==3);lists++;*cnt=absent?0:1;d[0]=(usb_device_entry){123,0x057e,0x0337,0};return 0;}
s32 USB_OpenDevice(s32 id,u16 vid,u16 pid,s32 *fd)
{assert(id==123&&vid==0x057e&&pid==0x0337);opens++;*fd=123;return 0;}
s32 USB_WriteIntrMsgAsync(s32 fd,u8 ep,u16 len,void *data,usbcallback cb,void *user)
{assert(fd==123&&ep==2&&len==1&&*(u8*)data==0x13&&!callback);writes++;buffer=data;callback=cb;userdata=user;return 0;}
s32 USB_ReadIntrMsgAsync(s32 fd,u8 ep,u16 len,void *data,usbcallback cb,void *user)
{assert(fd==123&&ep==0x81&&len==37&&!callback);reads++;if(fail_submit)return -1;buffer=data;callback=cb;userdata=user;return 0;}
static void complete(int result)
{assert(callback);usbcallback cb=callback;callback=NULL;cb(result,userdata);}
static void report(void)
{memset(buffer,0,37);uint8_t *p=buffer;p[0]=0x21;p[1]=0x10;p[2]=1;p[4]=p[5]=p[6]=p[7]=128;complete(37);}
int main(void)
{
 controller_pad p[4];controller_usb_init();controller_usb_scan(p);
 assert(opens==1&&writes==1&&reads==0&&!p[0].connected);
 controller_usb_scan(p);assert(writes==1);complete(1);
 controller_usb_scan(p);assert(reads==1);controller_usb_scan(p);assert(reads==1);
 now=5;report();controller_usb_scan(p);
 assert(p[0].connected&&p[0].guest&&reads==2);
 now=506;controller_usb_scan(p);assert(!p[0].connected&&reads==2);
 complete(-1);controller_usb_scan(p);assert(closes==1&&!p[0].connected);
 now=2505;controller_usb_scan(p);assert(opens==1);
 now=2506;controller_usb_scan(p);assert(opens==2&&writes==2);complete(1);
 controller_usb_scan(p);assert(reads==3);
 /* Shutdown leaves in-flight DMA storage alive; stale completion cannot leak input. */
 controller_usb_shutdown();controller_usb_scan(p);assert(!p[0].connected);
 controller_usb_init();controller_usb_scan(p);assert(opens==2);
 report();controller_usb_scan(p);assert(opens==3&&!p[0].connected);complete(1);
 fail_submit=1;controller_usb_scan(p);assert(controller_usb_status()==-2);
 controller_usb_scan(p);assert(!p[0].connected);controller_usb_shutdown();
 absent=1;now=10000;controller_usb_init();controller_usb_scan(p);
 unsigned previous=lists;now=10001;controller_usb_scan(p);assert(lists==previous);
 now=12000;controller_usb_scan(p);assert(lists==previous+1&&!p[0].connected);
 printf("R1364 async GC USB transport: init/single-flight/stale/error/reconnect/reload/absent throttling passed (mock)\n");
 return 0;
}
