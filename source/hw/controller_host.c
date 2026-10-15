/* GPL-3.0+. Native libogc transports plus MIT libwiidrc (FIX94).
 * Wii Remote Plus uses the same button interface as the original remote.
 * Wii wheels/grips are mechanical accessories and use their inserted pad.
 * This frontend currently routes the focused controller to PS2 port 1 only. */
#include "core/hw/controller_input.h"
#include "core/hw/controller_usb.h"
#ifdef GEKKO
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "wiidrc.h"
static int drc_ready;
static uint8_t js_axis(const struct joystick_t *j,int y)
{
 return y?controller_axis(j->pos.y,j->min.y,j->center.y,j->max.y,1):
          controller_axis(j->pos.x,j->min.x,j->center.x,j->max.x,0);
}
void controller_host_init(void)
{
 PAD_Init();WPAD_Init();
 /* Buttons-only also contains extension bytes; avoid unused IR/accelerometer. */
 WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS);
 WPAD_SetIdleTimeout(UINT32_MAX);
 drc_ready=WiiDRC_Inited()||WiiDRC_Init();
 controller_usb_init();
}
void controller_host_shutdown(void)
{ controller_usb_shutdown();WiiDRC_Reset();drc_ready=0;WPAD_Shutdown(); }
int controller_host_drc_available(void){return drc_ready;}
int controller_host_usb_status(void){return controller_usb_status();}
void controller_host_scan(controller_focus *f)
{
 controller_pad p[CI_SLOTS];
 for(unsigned i=0;i<CI_SLOTS;i++)controller_neutral(&p[i]);
 unsigned connected=PAD_ScanPads();WPAD_ScanPads();
 for(unsigned i=0;i<4;i++){
  if(connected&(1u<<i))
   controller_map_gc(&p[i],PAD_ButtonsHeld(i),PAD_StickX(i),PAD_StickY(i),PAD_SubStickX(i),PAD_SubStickY(i),PAD_TriggerL(i),PAD_TriggerR(i));
  const WPADData *d=WPAD_Data(i);
  if(!d||d->err!=WPAD_ERR_NONE)continue;
  if(d->exp.type==WPAD_EXP_CLASSIC){
   const struct classic_ctrl_t *c=&d->exp.classic;
   controller_map_classic(&p[4+i],d->btns_h,c->type,
     js_axis(&c->ljs,0),js_axis(&c->ljs,1),js_axis(&c->rjs,0),js_axis(&c->rjs,1));
  }else{
   int nc=d->exp.type==WPAD_EXP_NUNCHUK;
   controller_map_remote(&p[4+i],d->btns_h,nc,
     nc?js_axis(&d->exp.nunchuk.js,0):128,nc?js_axis(&d->exp.nunchuk.js,1):128);
  }
 }
 if(drc_ready&&WiiDRC_ScanPads()&&WiiDRC_Connected()){
  const struct WiiDRCData *d=WiiDRC_Data();
  controller_map_drc(&p[8],d->button,d->extra,d->xAxisL,d->yAxisL,d->xAxisR,d->yAxisR);
  if(WiiDRC_ShutdownRequested())p[8].home=1;
 }
 controller_usb_scan(p+9);
 controller_focus_update(f,p);
}
#else
void controller_host_init(void){}
void controller_host_shutdown(void){}
int controller_host_drc_available(void){return 0;}
int controller_host_usb_status(void){return 0;}
void controller_host_scan(controller_focus *f)
{controller_pad p[CI_SLOTS];for(unsigned i=0;i<CI_SLOTS;i++)controller_neutral(&p[i]);controller_focus_update(f,p);}
#endif
