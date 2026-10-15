/* GPL-3.0+. Raw layouts: devkitPro libogc wpad.h/pad.h, FIX94 libwiidrc,
 * ToadKing/wii-u-gc-adapter. All Wii-to-PS2 assignments are our mapping. */
#include "core/hw/controller_input.h"
#include "core/hw/iop_sio2.h"
#include <string.h>
void controller_neutral(controller_pad *p)
{ memset(p,0,sizeof(*p));p->lx=p->ly=p->rx=p->ry=128; }
uint8_t controller_axis(int pos,int lo,int mid,int hi,int invert)
{
 if(lo>=mid||mid>=hi)return 128;
 if(pos<lo)pos=lo;
 if(pos>hi)pos=hi;
 int d=pos-mid,span=d<0?mid-lo:hi-mid;
 /* 12% calibrated dead zone, rescale the remaining range to the full axis. */
 int mag=d<0?-d:d,dead=span*12/100;
 if(mag<=dead)return 128;
 int v=(mag-dead)*(d<0?128:127)/(span-dead);
 v=128+(d<0?-v:v);return (uint8_t)(invert?(v==128?128:255-v):v);
}
static uint16_t menu_guest(uint16_t m)
{
 uint16_t p=0;
 if(m&CI_A)p|=IOP_PAD_BTN_CROSS;
 if(m&CI_B)p|=IOP_PAD_BTN_CIRCLE;
 if(m&CI_X)p|=IOP_PAD_BTN_SQUARE;
 if(m&CI_Y)p|=IOP_PAD_BTN_TRIANGLE;
 if(m&CI_SELECT)p|=IOP_PAD_BTN_SELECT;
 if(m&CI_START)p|=IOP_PAD_BTN_START;
 if(m&CI_L)p|=IOP_PAD_BTN_L1;
 if(m&CI_R)p|=IOP_PAD_BTN_R1;
 if(m&CI_LEFT)p|=IOP_PAD_BTN_LEFT;
 if(m&CI_RIGHT)p|=IOP_PAD_BTN_RIGHT;
 if(m&CI_UP)p|=IOP_PAD_BTN_UP;
 if(m&CI_DOWN)p|=IOP_PAD_BTN_DOWN;
 return p;
}
static void stick_menu(controller_pad *p)
{
 if(p->lx<80)p->menu|=CI_LEFT;
 if(p->lx>176)p->menu|=CI_RIGHT;
 if(p->ly<80)p->menu|=CI_UP;
 if(p->ly>176)p->menu|=CI_DOWN;
}
void controller_map_gc(controller_pad *p,uint16_t b,int lx,int ly,int rx,int ry,unsigned tl,unsigned tr)
{
 controller_neutral(p);p->connected=1;p->kind=CI_GAMECUBE;
 p->menu=b&0x1f7f;p->guest=menu_guest(p->menu);
 p->lx=controller_axis(lx,-100,0,100,0);p->ly=controller_axis(ly,-100,0,100,1);
 p->rx=controller_axis(rx,-100,0,100,0);p->ry=controller_axis(ry,-100,0,100,1);
 if(tl>=96)p->guest|=IOP_PAD_BTN_L2;
 if(tr>=96)p->guest|=IOP_PAD_BTN_R2;
 stick_menu(p);
}
void controller_map_remote(controller_pad *p,uint32_t b,int nc,uint8_t lx,uint8_t ly)
{
 controller_neutral(p);p->connected=1;p->kind=nc?CI_NUNCHUK:CI_REMOTE;
 if(b&8)p->menu|=CI_A;
 if(b&4)p->menu|=CI_B;
 if(b&2)p->menu|=CI_X;
 if(b&1)p->menu|=CI_Y;
 if(b&0x1000)p->menu|=CI_START;
 if(b&0x10)p->menu|=CI_SELECT;
 if(b&0x100)p->menu|=CI_LEFT;
 if(b&0x200)p->menu|=CI_RIGHT;
 if(b&0x400)p->menu|=CI_DOWN;
 if(b&0x800)p->menu|=CI_UP;
 p->home=(b&0x80)!=0;
 if(nc){if(b&0x20000)p->menu|=CI_L;
 if(b&0x10000)p->menu|=CI_R;
  p->lx=lx;p->ly=ly;
 }
 p->guest=menu_guest(p->menu);stick_menu(p);
 /* Digital-mode compatibility: Nunchuk stick also supplies directions. */
 p->guest|=menu_guest(p->menu)&(IOP_PAD_BTN_LEFT|IOP_PAD_BTN_RIGHT|IOP_PAD_BTN_UP|IOP_PAD_BTN_DOWN);
}
void controller_map_classic(controller_pad *p,uint32_t b,unsigned type,uint8_t lx,uint8_t ly,uint8_t rx,uint8_t ry)
{
 controller_neutral(p);p->connected=1;p->full_pad=1;
 p->kind=type==2?CI_WIIU_PRO:type==1?CI_CLASSIC_PRO:CI_CLASSIC;
 uint32_t c=b>>16;
 if(c&0x10)p->menu|=CI_A;
 if(c&0x40)p->menu|=CI_B;
 if(c&8)p->menu|=CI_X;
 if(c&0x20)p->menu|=CI_Y;
 if(c&0x400)p->menu|=CI_START;
 if(c&0x1000)p->menu|=CI_SELECT;
 if(c&2)p->menu|=CI_LEFT;
 if(c&0x8000)p->menu|=CI_RIGHT;
 if(c&1)p->menu|=CI_UP;
 if(c&0x4000)p->menu|=CI_DOWN;
 if(c&0x2000)p->menu|=CI_L;
 if(c&0x200)p->menu|=CI_R;
 p->home=(c&0x800)!=0;p->guest=menu_guest(p->menu);
 if(c&0x80)p->guest|=IOP_PAD_BTN_L2;
 if(c&4)p->guest|=IOP_PAD_BTN_R2;
 /* Wii U Pro stick-click bits occupy the LOW word; they are not remote 1/2. */
 if(type==2){if(b&2)p->guest|=IOP_PAD_BTN_L3;
 if(b&1)p->guest|=IOP_PAD_BTN_R3;}
 p->lx=lx;p->ly=ly;p->rx=rx;p->ry=ry;stick_menu(p);
}
void controller_map_drc(controller_pad *p,uint16_t b,uint8_t extra,int lx,int ly,int rx,int ry)
{
 uint32_t c=0;
 if(b&0x8000)c|=0x10;
 if(b&0x4000)c|=0x40;
 if(b&0x2000)c|=8;
 if(b&0x1000)c|=0x20;
 if(b&0x800)c|=2;
 if(b&0x400)c|=0x8000;
 if(b&0x200)c|=1;
 if(b&0x100)c|=0x4000;
 if(b&0x80)c|=0x80;
 if(b&0x40)c|=4;
 if(b&0x20)c|=0x2000;
 if(b&0x10)c|=0x200;
 if(b&8)c|=0x400;
 if(b&4)c|=0x1000;
 if(b&2)c|=0x800;
 controller_map_classic(p,c<<16,2,
  controller_axis(lx,-75,0,75,0),controller_axis(ly,-75,0,75,1),
  controller_axis(rx,-75,0,75,0),controller_axis(ry,-75,0,75,1));
 p->kind=CI_GAMEPAD;
 if(extra&0x80)p->guest|=IOP_PAD_BTN_L3;
 if(extra&0x40)p->guest|=IOP_PAD_BTN_R3;
}
int controller_map_usb_gc(const uint8_t *r,size_t size,controller_pad p[4])
{
 for(unsigned i=0;i<4;i++)controller_neutral(&p[i]);
 if(!r||size!=37||r[0]!=0x21)return 0;
 for(unsigned i=0;i<4;i++){
  const uint8_t *a=r+1+i*9;unsigned type=(a[0]>>4)&3;
  if(type!=1&&type!=2)continue;
  uint16_t b=0;
  if(a[1]&1)b|=CI_A;
 if(a[1]&2)b|=CI_B;
 if(a[1]&4)b|=CI_X;
 if(a[1]&8)b|=CI_Y;
  if(a[1]&16)b|=CI_LEFT;
 if(a[1]&32)b|=CI_RIGHT;
 if(a[1]&64)b|=CI_DOWN;
 if(a[1]&128)b|=CI_UP;
  if(a[2]&1)b|=CI_START;
 if(a[2]&2)b|=CI_SELECT;
 if(a[2]&4)b|=CI_R;
 if(a[2]&8)b|=CI_L;
  controller_map_gc(&p[i],b,(int)a[3]-128,(int)a[4]-128,(int)a[5]-128,(int)a[6]-128,a[7],a[8]);
  p[i].kind=CI_USB_GAMECUBE;
 }
 return 1;
}
void controller_focus_init(controller_focus *f)
{ memset(f,0,sizeof(*f));f->active=-1;controller_neutral(&f->pad); }
static int deflected(const controller_pad *p)
{ return p->lx<80||p->lx>176||p->ly<80||p->ly>176||p->rx<80||p->rx>176||p->ry<80||p->ry>176; }
void controller_focus_update(controller_focus *f,const controller_pad p[CI_SLOTS])
{
 int a=f->active,chosen=a;
 if(a<0||!p[a].connected)chosen=-1;
 /* An active held button/stick owns the session until it is released. */
 int locked=a>=0&&p[a].connected&&(p[a].guest||p[a].home||deflected(&p[a]));
 for(int i=0;i<CI_SLOTS;i++)if(p[i].connected){
  if(chosen<0)chosen=i;
  if(!locked&&((p[i].guest&~f->previous[i].guest)||(p[i].home&&!f->previous[i].home)||
     (deflected(&p[i])&&!deflected(&f->previous[i])))){chosen=i;break;}
 }
 f->changed=chosen!=a;f->active=chosen;
 if(chosen>=0){
  const controller_pad *old=&f->previous[chosen];f->pad=p[chosen];
  f->down=f->pad.menu&~old->menu;f->home_down=f->pad.home&&!old->home;
 }else{controller_neutral(&f->pad);f->down=0;f->home_down=0;}
 memcpy(f->previous,p,sizeof(f->previous));
}
unsigned controller_session(const controller_focus *f,uint16_t *guest)
{
 const controller_pad *p=&f->pad;uint16_t m=p->menu,g=p->guest;
 unsigned action=0;
 if(p->home&&(m&CI_SELECT))action=3;
 else if(p->full_pad){
  if(p->home&&(m&CI_START)){if(f->home_down||(f->down&CI_START))action=2;g&=~IOP_PAD_BTN_START;}
  else if(f->home_down)action=1;
 }else{
  int hud=(m&(CI_START|CI_SELECT))==(CI_START|CI_SELECT);
  int start=(m&(CI_A|CI_SELECT))==(CI_A|CI_SELECT);
  g&=~IOP_PAD_BTN_START;
  if(hud)g&=~IOP_PAD_BTN_SELECT;
  if(start)g=(g&~(IOP_PAD_BTN_CROSS|IOP_PAD_BTN_SELECT))|IOP_PAD_BTN_START;
  if((m&(CI_B|CI_SELECT|CI_START))==(CI_B|CI_SELECT|CI_START))action=3;
  else if(hud&&(f->down&(CI_START|CI_SELECT)))action=2;
  else if(f->home_down||((f->down&CI_START)&&!hud))action=1;
 }
 if(guest)*guest=g;
 return action;
}
const char *controller_kind_name(unsigned k)
{
 static const char *n[]={"none","GameCube","Wii Remote","Wii Remote + Nunchuk","Classic","Classic Pro","Wii U Pro","Wii U GamePad","GameCube USB"};
 return k<sizeof(n)/sizeof(n[0])?n[k]:"unknown";
}
