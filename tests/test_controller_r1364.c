#include "core/hw/controller_input.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/frontend_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static void clear(controller_pad p[CI_SLOTS]){for(int i=0;i<CI_SLOTS;i++)controller_neutral(p+i);}
int main(void)
{
 controller_pad p,q[CI_SLOTS];controller_focus f;
 /* Calibration, sign, clipping, centered/invalid ranges and dead zone. */
 CHECK(controller_axis(128,48,128,208,0)==128);
 CHECK(controller_axis(134,48,128,208,0)==128);
 CHECK(controller_axis(208,48,128,208,0)==255);
 CHECK(controller_axis(48,48,128,208,0)==0);
 CHECK(controller_axis(48,48,128,208,1)==255);
 CHECK(controller_axis(208,48,128,208,1)==0);
 CHECK(controller_axis(128,128,128,128,0)==128);
 CHECK(controller_axis(1000,48,128,208,0)==255);
 controller_map_gc(&p,CI_A|CI_B|CI_X|CI_Y|CI_L|CI_R|CI_SELECT|CI_START,0,0,0,0,255,255);
 CHECK(p.guest==(IOP_PAD_BTN_CROSS|IOP_PAD_BTN_CIRCLE|IOP_PAD_BTN_SQUARE|IOP_PAD_BTN_TRIANGLE|IOP_PAD_BTN_L1|IOP_PAD_BTN_R1|IOP_PAD_BTN_L2|IOP_PAD_BTN_R2|IOP_PAD_BTN_SELECT|IOP_PAD_BTN_START));
 CHECK(p.lx==128&&p.ry==128);
 controller_map_remote(&p,0x30000,0,0,255);
 CHECK(p.guest==0&&p.lx==128&&p.ly==128); /* expansion bits ignored */
 controller_map_remote(&p,0x30000,1,0,255);
 CHECK(p.guest==(IOP_PAD_BTN_L1|IOP_PAD_BTN_R1|IOP_PAD_BTN_LEFT|IOP_PAD_BTN_DOWN));
 controller_map_classic(&p,0x30afu<<16,0,128,128,128,128);
 CHECK(p.kind==CI_CLASSIC&&p.full_pad);
 CHECK((p.guest&(IOP_PAD_BTN_L2|IOP_PAD_BTN_R2))==(IOP_PAD_BTN_L2|IOP_PAD_BTN_R2));
 controller_map_classic(&p,(0x0400u<<16)|3u,2,128,128,128,128);
 CHECK(p.kind==CI_WIIU_PRO);
 CHECK(p.guest==(IOP_PAD_BTN_START|IOP_PAD_BTN_L3|IOP_PAD_BTN_R3));
 CHECK(!(p.guest&(IOP_PAD_BTN_SQUARE|IOP_PAD_BTN_TRIANGLE))); /* low bits not Wii 1/2 */
 controller_map_classic(&p,3u,1,128,128,128,128);
 CHECK(p.kind==CI_CLASSIC_PRO&&p.guest==0);
 controller_map_drc(&p,0x80fcu,0xc0,-75,75,75,-75);
 CHECK(p.kind==CI_GAMEPAD&&p.full_pad);
 CHECK(p.lx==0&&p.ly==0&&p.rx==255&&p.ry==255);
 CHECK((p.guest&(IOP_PAD_BTN_CROSS|IOP_PAD_BTN_START|IOP_PAD_BTN_SELECT|IOP_PAD_BTN_L1|IOP_PAD_BTN_R1|IOP_PAD_BTN_L2|IOP_PAD_BTN_R2|IOP_PAD_BTN_L3|IOP_PAD_BTN_R3))==(IOP_PAD_BTN_CROSS|IOP_PAD_BTN_START|IOP_PAD_BTN_SELECT|IOP_PAD_BTN_L1|IOP_PAD_BTN_R1|IOP_PAD_BTN_L2|IOP_PAD_BTN_R2|IOP_PAD_BTN_L3|IOP_PAD_BTN_R3));
 /* USB report lengths/status and each independent port, including WaveBird. */
 uint8_t r[37]={0x21};controller_pad usb[4];
 for(unsigned i=0;i<4;i++){uint8_t *a=r+1+i*9;a[0]=(i&1)?0x20:0x10;a[1]=1u<<i;a[3]=a[4]=a[5]=a[6]=128;}
 CHECK(controller_map_usb_gc(r,37,usb));
 for(unsigned i=0;i<4;i++){CHECK(usb[i].connected&&usb[i].kind==CI_USB_GAMECUBE);CHECK(usb[i].guest==(IOP_PAD_BTN_CROSS>>0)*(i==0)+(IOP_PAD_BTN_CIRCLE)*(i==1)+(IOP_PAD_BTN_SQUARE)*(i==2)+(IOP_PAD_BTN_TRIANGLE)*(i==3));}
 CHECK(!controller_map_usb_gc(r,36,usb));CHECK(!usb[0].connected);
 r[0]=0;CHECK(!controller_map_usb_gc(r,37,usb));r[0]=0x21;r[1]=0;
 CHECK(controller_map_usb_gc(r,37,usb));CHECK(!usb[0].connected&&usb[1].connected);
 /* Focus: channel 4 works; releases/stale inputs cannot create mixed chords. */
 clear(q);controller_focus_init(&f);
 controller_map_classic(&q[7],0,2,128,128,128,128);
 controller_focus_update(&f,q);CHECK(f.active==7&&f.changed&&!f.down);
 controller_map_gc(&q[3],CI_A,0,0,0,0,0,0);
 controller_focus_update(&f,q);CHECK(f.active==3&&f.down==CI_A);
 controller_map_classic(&q[7],0x0400u<<16,2,128,128,128,128);
 controller_focus_update(&f,q);CHECK(f.active==3&&f.pad.guest==IOP_PAD_BTN_CROSS);
 controller_neutral(&q[3]);controller_focus_update(&f,q);
 CHECK(f.active==7&&f.changed&&f.pad.guest==IOP_PAD_BTN_START);
 clear(q);controller_focus_update(&f,q);
 CHECK(f.active==-1&&!f.pad.guest&&f.pad.lx==128&&!f.pad.connected);
 /* New pad PLUS belongs to guest; HOME pauses, HOME+PLUS HUD. */
 uint16_t guest=0;clear(q);controller_map_classic(&q[4],0x0400u<<16,1,128,128,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==0&&guest==IOP_PAD_BTN_START);
 controller_map_classic(&q[4],0x0800u<<16,1,128,128,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==1);
 controller_map_classic(&q[4],0x0c00u<<16,1,128,128,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==2&&!(guest&IOP_PAD_BTN_START));
 controller_map_classic(&q[4],0x1800u<<16,1,128,128,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==3);
 /* Legacy remote pause/guest START/HUD shortcuts remain intact. */
 clear(q);controller_focus_init(&f);controller_map_remote(&q[4],0x1000,0,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==1&&guest==0);
 controller_map_remote(&q[4],0x18,0,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==0&&guest==IOP_PAD_BTN_START);
 controller_map_remote(&q[4],0x1010,0,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==2&&guest==0);
 /* The observed multi-button state must pause, never silently exit. */
 controller_map_remote(&q[4],0x1014,0,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==1);
 controller_map_remote(&q[4],0x1094,0,128,128);
 controller_focus_update(&f,q);CHECK(controller_session(&f,&guest)==3); /* explicit HOME+MINUS */
 /* Press retention still observes actual guest samples. */
 frontend_pad_latch latch={0,0,0};
 CHECK(frontend_pad_latch_update(&latch,IOP_PAD_BTN_CROSS,0)==IOP_PAD_BTN_CROSS);
 CHECK(frontend_pad_latch_update(&latch,0,0)==IOP_PAD_BTN_CROSS);
 CHECK(frontend_pad_latch_update(&latch,0,1)==0);
 printf("R1364 controller mappings/focus/session/axes: %u checks passed (hardware not tested)\n",checks);
 return 0;
}
