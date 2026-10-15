// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2026 Gekko2 contributors
// SPDX-License-Identifier: GPL-3.0+
/* Scalar adaptation of PCSX2 yuv2rgb_reference, ipu_csc and
 * ipu_dither_reference, revision 3c8df07e8e367caef8386fd949d6ca44bf2f7ad0.
 * Explicit guest byte order; identical portable CPU/worker kernel. */
#include "core/hw/ipu.h"
static int floor_shift(int v,unsigned n)
{ return v>=0?v>>n:-(((-v)+(1<<n)-1)>>n); }
static uint8_t clamp(int v){return v<0?0:v>255?255:(uint8_t)v;}
void ipu_csc_convert(const uint8_t in[384],uint8_t *out,
 uint32_t ofm,uint32_t dte,uint16_t th0,uint16_t th1)
{
 static const int8_t matrix[4][4]={{-4,0,-3,1},{2,-2,3,-1},{-3,1,-4,0},{3,-1,2,-2}};
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++) {
  unsigned i=y*16+x,c=(y>>1)*8+(x>>1);
  int lum=in[i]>16?((in[i]-16)*149)>>6:0;
  int cb=(int)in[256+c]-128,cr=(int)in[320+c]-128;
  uint8_t r=clamp(floor_shift(lum+floor_shift(204*cr,6)+1,1));
  uint8_t g=clamp(floor_shift(lum+floor_shift(-104*cr,6)+floor_shift(-50*cb,6)+1,1));
  uint8_t b=clamp(floor_shift(lum+floor_shift(258*cb,6)+1,1));
  uint8_t a=0x80;
  if(r<th0&&g<th0&&b<th0){r=g=b=a=0;}
  else if(r<th1&&g<th1&&b<th1)a=0x40;
  if(ofm){
   int d=dte?matrix[y&3][x&3]:0;
   uint16_t v=(clamp(r+d)>>3)|((clamp(g+d)>>3)<<5)|((clamp(b+d)>>3)<<10)|((a==0x40u)<<15);
   out[i*2]=(uint8_t)v;out[i*2+1]=(uint8_t)(v>>8);
  }else{out[i*4]=r;out[i*4+1]=g;out[i*4+2]=b;out[i*4+3]=a;}
 }
}
