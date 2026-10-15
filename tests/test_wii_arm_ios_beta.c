/* Synthetic IOS/SD/controller lifecycle; no real console or NAND writes. */
#include "core/hw/wii_arm_ios.h"
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static int mode,current=58,reloads,mounts,unmounts,input_start,input_stop;
static int shut(void){return 1;}
const DISC_INTERFACE __io_wiisd={shut};DISC_INTERFACE __io_usbstorage={shut};
int IOS_GetVersion(void){return current;}
int IOS_ReloadIOS(int n){reloads++;if(mode==3&&n==222)return -1;if(mode==5&&n==58)return -1;current=n;return 0;}
int IOS_Open(const char *p,int n){(void)n;assert(!strcmp(p,"/dev/mload"));return mode==4||mode==5?-1:7;}
int IOS_Close(int n){assert(n==7);return 0;}
int ES_GetTMDViewSize(u64 title,u32 *size){assert(title==(0x100000000ULL|222));*size=92;return mode==1?-1:0;}
int ES_GetTMDView(u64 title,u8 *data,u32 size){assert(size==92);memset(data,0,size);tmd_view *v=(tmd_view*)data;v->title_id=title;v->title_version=mode==2?65280:65535;v->num_contents=3;return 0;}
void fatUnmount(const char *p){assert(!strcmp(p,"sd:")||!strcmp(p,"usb:"));unmounts++;}
int fatInitDefault(void){mounts++;return !(mode==6&&current==222);}
void WPAD_Shutdown(void){input_stop++;}
void PAD_Init(void){}
void WPAD_Init(void){input_start++;}
void WPAD_SetDataFormat(int a,int b){(void)a;(void)b;}
void WPAD_SetIdleTimeout(unsigned t){assert(t==0xffffffff);}
int main(int argc,char **argv)
{
 assert(argc==2);mode=atoi(argv[1]);int mounted=1;int r=wii_arm_ios_start(mode==7?0:222,&mounted);
 assert(wii_arm_ios_original()==58&&wii_arm_ios_active()==(unsigned)current);
 if(mode==0){assert(r==1&&current==222&&mounted&&reloads==1&&input_start==1&&input_stop==1&&unmounts==2);}
 else if(mode==1||mode==2){assert(r==-1&&current==58&&mounted&&!reloads&&!input_stop&&!unmounts);}
 else if(mode==3){assert(r==-2&&current==58&&mounted&&input_start==1);}
 else if(mode==4){assert(r==-3&&current==58&&mounted&&reloads==2&&input_start==1);}
 else if(mode==5){assert(r==-4&&current==222&&input_start==1);}
 else if(mode==6){assert(r==-5&&current==58&&mounted&&reloads==2&&mounts==2);}
 else if(mode==7){assert(r==0&&current==58&&mounted&&!reloads&&!input_stop);}
 printf("PASS IOS beta mode=%d status=%d active=%d reloads=%d mounted=%d\n",mode,r,current,reloads,mounted);
}
