// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2000-2002 Michel Lespinasse
// SPDX-FileCopyrightText: 1999-2000 Aaron Holtzman
// SPDX-License-Identifier: GPL-3.0+
/* Bounded, resumable scalar coefficient reconstruction. Tables and IPU
 * quantization follow PCSX2 3c8df07 IPU_MultiISA.cpp. Tokens are consumed
 * atomically: starvation never partially applies a DC/escape/run symbol. */
#include "core/hw/ipu.h"
#include "core/hw/ipu_vlc_tables.h"
#include <string.h>
static const uint8_t scans[2][64]={
 {0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63},
 {0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63}};
static const uint8_t nonlinear[32]={0,1,2,3,4,5,6,7,8,10,12,14,16,18,20,22,24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112};
static unsigned get(uint32_t bits,unsigned at,unsigned n){return (bits>>(32-at-n))&((1u<<n)-1);}
static int signed_bits(unsigned v,unsigned n){return (v&(1u<<(n-1)))?(int)v-(1<<n):(int)v;}
static int shift(int v,unsigned n){return v>=0?v>>n:-(((-v)+(1<<n)-1)>>n);}
static int saturate(int v){return v<-2048?-2048:v>2047?2047:v;}
void ipu_mpeg_begin(ipu_state_t *s)
{
 ipu_mpeg_state_t *m=&s->mpeg;
 unsigned cmd=s->command>>28;
 if(cmd==1||(s->command&(1u<<26)))for(unsigned i=0;i<3;i++)m->dc[i]=128<<((s->ctrl>>16)&3);
 m->phase=0;m->block=0;m->index=0;m->intra=cmd==1?1:(s->command>>27)&1;
 m->interlaced=cmd==1?0:(s->command>>25)&1;
 unsigned q=(s->command>>16)&31;m->quant=(s->ctrl&(1u<<22))?nonlinear[q]:q*2;
 m->cbp=m->intra?63:0;memset(m->coeff,0,sizeof m->coeff);memset(s->block,0,384);memset(s->converted,0,768);
}
static const DCTtab *ac_table(unsigned code,unsigned intra,unsigned first,unsigned alternative)
{
 if(code>=16384&&!alternative)return first&&!intra?&DCT.first[(code>>12)-4]:&DCT.next[(code>>12)-4];
 if(code>=1024)return alternative?&DCT.tab0a[(code>>8)-4]:&DCT.tab0[(code>>8)-4];
 if(code>=512)return alternative?&DCT.tab1a[(code>>6)-8]:&DCT.tab1[(code>>6)-8];
 if(code>=256)return &DCT.tab2[(code>>4)-16];
 if(code>=128)return &DCT.tab3[(code>>3)-16];
 if(code>=64)return &DCT.tab4[(code>>2)-16];
 if(code>=32)return &DCT.tab5[(code>>1)-16];
 if(code>=16)return &DCT.tab6[code-16];
 return 0;
}
static void store_block(ipu_state_t *s)
{
 ipu_mpeg_state_t *m=&s->mpeg;ipu_idct(m->coeff);
 unsigned b=m->block,stride=b<4?(m->interlaced?32:16):8;
 unsigned offset=b<4?((b&1)*8+(b>>1)*(m->interlaced?16:128)):256+(b-4)*64;
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
  int v=m->coeff[y*8+x];unsigned p=offset+y*stride+x;
  if(m->intra){v=v<0?0:v>255?255:v;s->block[p]=(uint8_t)v;}
  uint16_t value=(uint16_t)v;s->converted[p*2]=(uint8_t)value;s->converted[p*2+1]=(uint8_t)(value>>8);
 }
 memset(m->coeff,0,sizeof m->coeff);m->block++;m->phase=1;
}
/* -1 invalid stream; 0 starved; 1 one macroblock reconstructed.
 * There are at most six blocks * 64 tokens per call. */
int ipu_mpeg_decode(ipu_state_t *s,int (*peek)(unsigned,int,uint32_t *),void (*consume)(unsigned))
{
 ipu_mpeg_state_t *m=&s->mpeg;uint32_t bits;
 if(!m->phase){
  if(!peek(32,0,&bits))return 0;
  if(!m->intra){const CBPtab *t=(bits>>16)>=0x2000?&CBP_7[(bits>>25)-16]:&CBP_9[bits>>23];
   if(!t->len){s->ctrl|=0x4000u;return -1;}m->cbp=t->cbp;consume(t->len);
  }
  m->phase=1;
 }
 for(unsigned guard=0;guard<400;guard++){
  if(m->block==6){s->ctrl=(s->ctrl&~0x3f00u)|(m->cbp<<8);return 1;}
  if(!(m->cbp&(32u>>m->block))){m->block++;continue;}
  if(m->phase==1){
   m->index=m->intra?1:0;
   if(m->intra){
    if(!peek(32,0,&bits))return 0;
    const DCtab *t;unsigned code=bits>>27,cc=m->block<4?0:m->block-3;
    if(!cc)t=code<31?&DCtable.lum0[code]:&DCtable.lum1[(bits>>23)-0x1f0];
    else t=code<31?&DCtable.chrom0[code]:&DCtable.chrom1[(bits>>22)-0x3e0];
    if(!t->len){s->ctrl|=0x4000u;return -1;}
    int diff=0;
    if(t->size){unsigned v=get(bits,t->len,t->size);diff=(v&(1u<<(t->size-1)))?(int)v:(int)v-((1<<t->size)-1);}
    consume(t->len+t->size);m->dc[cc]+=diff;
    m->coeff[0]=(int16_t)(m->dc[cc]*(1<<(3-((s->ctrl>>16)&3))));
   }
   m->phase=2;
  }
  if(!peek(32,0,&bits))return 0;
  unsigned mp1=(s->ctrl>>23)&1,alt=m->intra&&!mp1&&((s->ctrl>>21)&1);
  const DCTtab *t=ac_table(bits>>16,m->intra,!m->index,alt);
  if(!t||!t->len){s->ctrl|=0x4000u;return -1;}
  if(t->run==64){consume(t->len);store_block(s);continue;}
  unsigned n=t->len,run=t->run;int value;
  if(run==65){
   run=get(bits,n,6);n+=6;int level;
   if(!mp1){level=signed_bits(get(bits,n,12),12);n+=12;
    value=0;
   }else{
    level=signed_bits(get(bits,n,8),8);n+=8;
    if(!(level&127)){level=(int)get(bits,n,8)+2*level;n+=8;}
    value=0;
   }
   if(m->index+run>=64){s->ctrl|=0x4000u;return -1;}
   int q=m->quant*s->iq[m->intra?0:1][m->index+run];
   if(m->intra){value=shift(level*q,4);if(mp1)value=(value+(value<0?0:-1))|1;}
   else{
    int v=(2*(level+(level<0?-1:0))+1)*q;
    value=mp1?v/32:shift(v,5);if(mp1)value=(value+(value<0?0:-1))|1;
   }
  }else{
   if(m->index+run>=64){s->ctrl|=0x4000u;return -1;}
   int q=m->quant*s->iq[m->intra?0:1][m->index+run];
   value=m->intra?((int)t->level*q)>>4:((2*(int)t->level+1)*q)>>5;
   if(m->intra&&mp1)value=(value-1)|1;
   if(get(bits,n,1))value=-value;
   n++;
  }
  consume(n);unsigned index=m->index+run,j=scans[(s->ctrl>>20)&1][index];
  /* PCSX2 IDCT expects its permuted coefficient layout. */
  j=((j&0x36)>>1)|((j&9)<<2);m->coeff[j]=saturate(value);m->index=index+1;
 }
 s->ctrl|=0x4000u;return -1;
}
