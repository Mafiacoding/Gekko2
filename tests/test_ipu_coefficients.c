/* Hand-encoded MPEG escapes and independent scalar inverse quantization.
 * IDCT is deliberately an observer here; pixel tests cover it separately. */
#include "core/hw/ipu.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static uint8_t data[128];
static unsigned length,position,available,calls,failures,cases;
static int16_t captured[64];
static unsigned precision,next_precision;
static void put(unsigned v,unsigned n)
{for(unsigned i=n;i;i--){if((v>>(i-1))&1)data[length>>3]|=1u<<(7-(length&7));length++;}}
static int peek(unsigned n,int consume,uint32_t *v)
{if(position+n>available)return 0;*v=0;for(unsigned i=0;i<n;i++)*v=(*v<<1)|((data[(position+i)>>3]>>(7-((position+i)&7)))&1);if(consume)position+=n;return 1;}
static void advance(unsigned n){assert(position+n<=available);position+=n;}
void ipu_idct(int16_t block[64])
{if(!calls)memcpy(captured,block,sizeof captured);calls++;memset(block,0,128);}
static const uint8_t normal[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
static const uint8_t alternate[64]={0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63};
static int floor_div(int n,int d){int q=n/d;return q-(n<0&&n%d!=0);}
static void one(unsigned mp1,unsigned intra,unsigned alt,unsigned run,int level,unsigned q)
{
 ipu_state_t s;memset(&s,0,sizeof s);memset(data,0,sizeof data);
 length=position=available=calls=0;memset(captured,0,sizeof captured);
 s.command=0x24000000u|(intra<<27)|(q<<16);s.ctrl=(mp1<<23)|(alt<<20)|(precision<<16);
 for(unsigned k=0;k<64;k++)s.iq[intra?0:1][k]=(k*7+11)%255+1;
 ipu_mpeg_begin(&s);
 s.ctrl=(s.ctrl&~0x30000u)|(next_precision<<16);
 if(intra)put(4,3);else put(10,4); /* luma DC size=0 or CBP=32 */
 put(1,6);put(run,6);
 if(mp1){
  if(level>=128){put(0,8);put(level,8);}
  else if(level<=-128){put(128,8);put((unsigned)(level+256),8);}
  else put((unsigned)level&255,8);
 }else put((unsigned)level&4095,12);
 put(2,2);
 if(intra)for(unsigned b=1;b<6;b++){put(b<4?4:0,b<4?3:2);put(2,2);}
 while(length&127)put(0,1);length+=32;
 int result=0;
 for(unsigned guard=0;guard<32&&!result;guard++){
  available+=8;if(available>length)available=length;
  result=ipu_mpeg_decode(&s,peek,advance);
  if(!result){ipu_state_t before=s;unsigned at=position;
   assert(ipu_mpeg_decode(&s,peek,advance)==0&&position==at&&!memcmp(&before,&s,sizeof s));}
 }
 unsigned index=run+(intra?1:0),j=(alt?alternate:normal)[index];
 /* Separable IDCT frequency order for both dimensions. */
 const unsigned order[8]={0,4,1,5,2,6,3,7};
 unsigned x=j%8,y=j/8,slot=order[y]*8+order[x];
 int scale=2*(int)q*(int)s.iq[intra?0:1][index],v;
 if(intra)v=floor_div(level*scale,16);
 else {int product=(2*level+(level<0?-1:1))*scale;v=mp1?product/32:floor_div(product,32);}
 if(mp1)v=(v+(v<0?0:-1))|1;
 if(v<-2048)v=-2048;if(v>2047)v=2047;
 int16_t expected[64]={0};if(intra)expected[0]=1024;expected[slot]=(int16_t)v;
 cases++;if(result!=1||calls!=(intra?6u:1u)||memcmp(expected,captured,128)){
  if(failures<8)printf("FAIL mp1=%u intra=%u scan=%u run=%u level=%d q=%u result=%d slot=%u actual=%d expected=%d\n",mp1,intra,alt,run,level,q,result,slot,captured[slot],v);failures++;
 }
}
int main(void)
{
 const int levels[]={-255,-128,-127,-1,1,127,128,255};
 for(unsigned mp=0;mp<2;mp++)for(unsigned in=0;in<2;in++)
 for(unsigned alt=0;alt<2;alt++)for(unsigned run=0;run<(in?63u:64u);run++)
 for(unsigned l=0;l<8;l++)for(unsigned q=1;q<=31;q+=15)one(mp,in,alt,run,levels[l],q);
 /* Decoder precision is latched, even if CTRL changes while starved. */
 for(precision=0;precision<3;precision++)for(next_precision=0;next_precision<3;next_precision++)
  one(0,1,0,0,1,7);
 precision=next_precision=0;
 /* Invalid CBP and an intra run beyond coefficient 63 must flag ECD
  * without IDCT/output. Repeated starvation above never consumed a token twice. */
 for(unsigned invalid=0;invalid<2;invalid++){
  ipu_state_t s;memset(&s,0,sizeof s);memset(data,0,sizeof data);
  length=position=calls=0;s.command=0x24010000u|(invalid<<27);ipu_mpeg_begin(&s);
  if(invalid){put(4,3);put(1,6);put(63,6);put(1,12);}
  available=128;
  assert(ipu_mpeg_decode(&s,peek,advance)==-1&&(s.ctrl&0x4000)&&!calls);
 }
 printf("MPEG1/2 escape, inverse-quantization, scan and starvation oracle: %u cases, %u failures\n",cases,failures);
 return failures?1:0;
}
