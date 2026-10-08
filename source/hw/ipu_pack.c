// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2026 Gekko2 contributors
// SPDX-License-Identifier: GPL-3.0+
/* Portable guest-byte-order PACK: RGB32 -> dithered RGB16 -> optional
 * nearest VQ palette entry. See PCSX2 IPUdither.cpp and ipu_vq. */
#include "core/hw/ipu.h"
static unsigned clamp(int v){return v<0?0:v>255?255:(unsigned)v;}
static unsigned rd16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
void ipu_pack_convert(const uint8_t in[1024],uint8_t *out,
 uint32_t ofm,uint32_t dte,const uint8_t palette[32])
{
 static const int8_t matrix[4][4]={{-4,0,-3,1},{2,-2,3,-1},{-3,1,-4,0},{3,-1,2,-2}};
 uint8_t rgb16[512];
 for(unsigned i=0;i<256;i++){
  int d=dte?matrix[(i>>4)&3][i&3]:0;
  unsigned v=(clamp(in[i*4]+d)>>3)|((clamp(in[i*4+1]+d)>>3)<<5)|
   ((clamp(in[i*4+2]+d)>>3)<<10)|((in[i*4+3]==0x40u)<<15);
  rgb16[i*2]=v;rgb16[i*2+1]=v>>8;
 }
 if(ofm){for(unsigned i=0;i<512;i++)out[i]=rgb16[i];return;}
 for(unsigned i=0;i<256;i++){
  unsigned v=rd16(rgb16+i*2),best=0,minimum=~0u;
  for(unsigned j=0;j<16;j++){
   unsigned p=rd16(palette+j*2);
   int dr=(int)(v&31)-(int)(p&31),dg=(int)((v>>5)&31)-(int)((p>>5)&31),db=(int)((v>>10)&31)-(int)((p>>10)&31);
   unsigned distance=dr*dr+dg*dg+db*db;
   if(distance<minimum){minimum=distance;best=j;}
  }
  if(!(i&1))out[i>>1]=best;else out[i>>1]|=best<<4;
 }
}
