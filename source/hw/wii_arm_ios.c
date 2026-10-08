/* GPL-3.0+. Opt-in startup IOS selection. Never installs or patches IOS. */
#include "core/hw/wii_arm_ios.h"
static int status;static unsigned original,active;
int wii_arm_ios_status(void){return status;}
unsigned wii_arm_ios_original(void){return original;}
unsigned wii_arm_ios_active(void){return active;}
const char *wii_arm_ios_message(void)
{
 switch(status){
 case 1:return "MLOAD found";
 case -1:return "IOS222 missing / stub";
 case -2:return "IOS reload failed";
 case -3:return "MLOAD unavailable";
 case -4:return "IOS recovery failed";
 case -5:return "SD remount failed";
 default:return "Current IOS retained";
 }
}
#ifdef GEKKO
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/es.h>
#include <ogc/ipc.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <malloc.h>
#include <stdlib.h>
static int installed(unsigned slot)
{
 u64 title=0x100000000ULL|slot;u32 size=0;
 if(ES_GetTMDViewSize(title,&size)<0||size<92||size>16384)return 0;
 u8 *data=memalign(32,(size+31)&~31);if(!data)return 0;
 int ok=ES_GetTMDView(title,data,size)>=0;
 if(ok){const tmd_view *v=(const tmd_view*)data;
  ok=v->title_id==title&&v->title_version!=65280&&v->num_contents>1;
 }free(data);return ok;
}
static void stop_storage(int *mounted)
{
 if(*mounted){fatUnmount("sd:");fatUnmount("usb:");*mounted=0;}
 if(__io_wiisd.shutdown)__io_wiisd.shutdown();
 if(__io_usbstorage.shutdown)__io_usbstorage.shutdown();
 WPAD_Shutdown();
}
static void start_input(void)
{
 PAD_Init();WPAD_Init();WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC);WPAD_SetIdleTimeout(UINT32_MAX);
}
int wii_arm_ios_start(unsigned slot,int *mounted)
{
 original=active=IOS_GetVersion();status=0;
 if(!mounted||!slot)return 0;
 /* Only the explicitly supported Hermes slot is selected by this beta.
  * Missing/stub IOS never causes a reload. MLOAD is verified afterwards. */
 if(slot!=222||!installed(slot))return status=-1;
 stop_storage(mounted);
 int r=IOS_ReloadIOS(slot);active=IOS_GetVersion();
 int fd=r>=0&&active==slot?IOS_Open("/dev/mload",0):-1;
 status=fd>=0?1:(r<0||active!=slot?-2:-3);
 if(fd>=0)IOS_Close(fd);
 if(status==1){*mounted=fatInitDefault()?1:0;if(!*mounted)status=-5;}
 if(status<0&&active!=original){
  if(IOS_ReloadIOS(original)<0||IOS_GetVersion()!=original){active=IOS_GetVersion();start_input();return status=-4;}
  active=original;
 }
 if(!*mounted)*mounted=fatInitDefault()?1:0;
 start_input();return status;
}
#else
int wii_arm_ios_start(unsigned slot,int *mounted){(void)slot;(void)mounted;return status=0;}
#endif
