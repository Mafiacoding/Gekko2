// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2026 Gekko2 contributors
// SPDX-License-Identifier: GPL-3.0+
/* Scalar C adaptation of PCSX2 IPU_MultiISA.cpp VDEC at
 * 3c8df07e8e367caef8386fd949d6ca44bf2f7ad0. No guest pointers or globals. */
#include "core/hw/ipu.h"
#include "core/hw/ipu_vlc_tables.h"
uint32_t ipu_vlc_decode(uint32_t bits,unsigned table,uint32_t ctrl,unsigned *consumed)
{
 unsigned used=0;uint32_t value=0;
 if(table==0){
  const MBAtab *t=0;unsigned code=bits>>16;
  if(code>=4096)t=&MBA.mba5[(bits>>27)-2];
  else if(code>=768)t=&MBA.mba11[(bits>>21)-24];
  else if((bits>>21)==8){used=11;value=0xb0023;}
  else if((bits>>21)==15&&(ctrl&(1u<<23))){used=11;value=0xb0022;}
  if(t){used=t->len;value=(t->mba+1)|(used<<16);}
 }else if(table==1){
  unsigned pct=(ctrl>>24)&7;if(!pct)pct=1;
  const MBtab *t=0;
  if(pct==1&&(bits>>30))t=&MB_I[bits>>31];
  else if(pct==2&&(bits>>26))t=&MB_P[bits>>27];
  else if(pct==3&&(bits>>26))t=&MB_B[bits>>26];
  else if(pct==4&&(bits>>31)){used=1;value=0x10001;}
  if(t){used=t->len;value=t->modes;
   if(pct==2&&(value&MACROBLOCK_MOTION_FORWARD))value|=MC_FRAME;
   if(pct==3)value|=MC_FRAME|(used<<16);
  }
 }else if(table==2){
  unsigned code=bits>>16;
  if(code&0x8000){used=1;value=0x10000;}
  else{
   const MVtab *t;
   if((code&0xf000)||((code&0xfc00)==0x0c00))t=&MV_4[bits>>28];
   else t=&MV_10[bits>>22];
   used=t->len+1;
   int delta=t->delta+1;if((bits>>(32-used))&1)delta=-delta;
   value=(uint32_t)delta|(t->len<<16);
  }
 }else if(table==3){
  const DMVtab *t=&DMV_2[bits>>30];used=t->len;
  value=(uint32_t)(int32_t)t->dmv|(used<<16);
 }
 if(consumed)*consumed=used;
 return value;
}
