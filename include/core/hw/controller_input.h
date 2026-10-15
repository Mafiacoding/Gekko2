#ifndef GEKKO2_CONTROLLER_INPUT_H
#define GEKKO2_CONTROLLER_INPUT_H
#include <stdint.h>
#include <stddef.h>
/* Menu bits deliberately match libogc PAD. Guest state is separate. */
enum { CI_LEFT=1,CI_RIGHT=2,CI_DOWN=4,CI_UP=8,CI_SELECT=16,
 CI_R=32,CI_L=64,CI_A=256,CI_B=512,CI_X=1024,CI_Y=2048,CI_START=4096 };
enum { CI_NONE,CI_GAMECUBE,CI_REMOTE,CI_NUNCHUK,CI_CLASSIC,
 CI_CLASSIC_PRO,CI_WIIU_PRO,CI_GAMEPAD,CI_USB_GAMECUBE };
#define CI_SLOTS 13 /* four native GC, four WPAD, DRC, four USB GC */
typedef struct {
 uint16_t menu,guest;
 uint8_t lx,ly,rx,ry,home,connected,kind,full_pad;
} controller_pad;
typedef struct {
 controller_pad previous[CI_SLOTS],pad;
 uint16_t down;
 uint8_t home_down,changed;
 int active;
} controller_focus;
void controller_neutral(controller_pad *pad);
uint8_t controller_axis(int pos,int lo,int center,int hi,int invert);
void controller_map_gc(controller_pad *pad,uint16_t held,int lx,int ly,int rx,int ry,unsigned tl,unsigned tr);
void controller_map_remote(controller_pad *pad,uint32_t held,int nunchuk,uint8_t lx,uint8_t ly);
void controller_map_classic(controller_pad *pad,uint32_t held,unsigned type,uint8_t lx,uint8_t ly,uint8_t rx,uint8_t ry);
void controller_map_drc(controller_pad *pad,uint16_t held,uint8_t extra,int lx,int ly,int rx,int ry);
int controller_map_usb_gc(const uint8_t *report,size_t size,controller_pad pad[4]);
void controller_focus_init(controller_focus *focus);
void controller_focus_update(controller_focus *focus,const controller_pad pad[CI_SLOTS]);
/* 0=game, 1=pause, 2=HUD, 3=exit; does not alter analog-mode negotiation. */
unsigned controller_session(const controller_focus *focus,uint16_t *guest);
const char *controller_kind_name(unsigned kind);
void controller_host_init(void);
void controller_host_scan(controller_focus *focus);
void controller_host_shutdown(void);
int controller_host_drc_available(void);
int controller_host_usb_status(void);
#endif
