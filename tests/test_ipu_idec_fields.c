/* Synthetic MPEG I-macroblocks. Header bits are assembled independently of
 * the decoder; expected samples use the cosine IDCT definition, not its code.
 * No BIOS/game data. See docs/IPU-ACCURACY-CONTINUATION.md for references. */
#include "core/hw/ipu.h"
#include "core/hw/dma.h"
#include "core/hw/ee_intc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures, checks, length;
static uint8_t stream[128];
#define CHECK(c) do { checks++; if (!(c)) { if (failures < 12) \
 printf("FAIL line %u: %s\n", __LINE__, #c); failures++; } } while (0)
static void put(unsigned v, unsigned n)
{ for (unsigned i=n;i;i--) { if ((v>>(i-1))&1) stream[length>>3]|=1u<<(7-(length&7)); length++; } }
static uint32_t rd(unsigned addr)
{ uint32_t v=0; CHECK(ipu_mmio_read32(addr,&v)); return v; }
static void reset(void)
{ dma_init(); ipu_init(); ee_intc_init(); length=0; memset(stream,0,sizeof stream); }
static void block(unsigned b)
{
 /* Luma predictors: 136,136,128,128. Chroma predictors: 128,128. */
 if (b==0 || b==2) { put(6,3); put(b==0?8:7,4); }
 else put(b<4?4:0,b<4?3:2);
 if (!b) { put(3,2); put(0,1); } /* run=0, level=+1 at AC(1,0) */
 put(2,2); /* end_of_block */
}
static unsigned csc_gray(unsigned y)
{ unsigned l=y>16?((y-16)*149)/64:0; l=(l+1)/2; return l>255?255:l; }
static void decode_case(unsigned q, unsigned nonlinear, unsigned quantized,
                        unsigned dtd, unsigned interlaced)
{
 static const unsigned scales[32]={0,1,2,3,4,5,6,7,8,10,12,14,16,18,20,22,24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112};
 reset();
 ipu_mmio_write32(0x10002010,(nonlinear<<22)|(3u<<24));
 memset(ipu_get_state()->iq[0],16,64);
 put(1,quantized?2:1); /* 01 = intra+quant, 1 = intra */
 if (dtd) put(interlaced,1); /* DCT type precedes quantizer_scale_code */
 if (quantized) put(q,5);
 for (unsigned b=0;b<6;b++) block(b);
 while (length&7) put(0,1);
 put(0x000001b3,32);
 while (length&127) put(0,1);
 unsigned expected_q=quantized?(nonlinear?scales[q]:2*q):q;
 unsigned actual_interlace=dtd&&interlaced;
 ipu_mmio_write32(0x10002000,0x10000000u|(q<<16)|(dtd<<24));
 CHECK(rd(0x10002034)==0x80000000u);
 CHECK(rd(0x10002004)==0); /* only TOP busy, not CMD.DATA */
 CHECK(((rd(0x10002010)>>24)&7)==1); /* IDEC selects I picture */
 unsigned sent=0,got=0; uint8_t out[1024];
 for (unsigned g=0;g<256;g++) {
  if (sent<length/128) sent+=ipu_input_write(stream+sent*16,1);
  ipu_service();
  if (g==1) { /* restore while the output FIFO is full */
   ipu_state_t saved=*ipu_get_state(); CHECK(ipu_state_valid(&saved)); ipu_restore(&saved);
  }
  if (got<64) got+=ipu_output_read(out+got*16,1);
  ipu_service();
  if (got==64&&!ipu_get_state()->busy) break;
 }
 CHECK(got==64&&!ipu_get_state()->busy);
 CHECK(ipu_get_state()->mpeg.quant==expected_q);
 CHECK(ipu_get_state()->mpeg.interlaced==actual_interlace);
 CHECK(rd(0x10002030)==0x000001b3);
 CHECK((rd(0x10002010)&0xc000u)==0x8000u);
 CHECK(rd(0x10002034)==0);
 CHECK(ee_intc_get_raise_count(8)==1);
 if (got!=64) return;
 for (unsigned y=0;y<16;y++) for (unsigned x=0;x<16;x++) {
  unsigned b=(x/8)+(actual_interlace?(y&1):(y/8))*2;
  double sample=b<2?136:128;
  if (!b) sample+=expected_q*cos((2*(x&7)+1)*3.14159265358979323846/16)/(4*sqrt(2.0));
  unsigned gray=csc_gray((unsigned)floor(sample+0.5));
  unsigned at=(y*16+x)*4;
  for (unsigned c=0;c<3;c++) CHECK(abs((int)out[at+c]-(int)gray)<=2);
  CHECK(out[at+3]==128);
 }
}
int main(void)
{
 /* Both decode commands must retain TOP.BUSY during input starvation. */
 for (unsigned cmd=1;cmd<=2;cmd++) {
  reset(); ipu_mmio_write32(0x10002000,cmd<<28);
  CHECK(rd(0x10002034)==0x80000000u);
  CHECK(rd(0x10002004)==0);
  uint64_t both=0; CHECK(ipu_mmio_read64(0x10002030,&both));
  CHECK(both==0x8000000000000000ULL);
  ipu_state_t saved=*ipu_get_state(); ipu_init(); ipu_restore(&saved);
  CHECK(rd(0x10002034)==0x80000000u);
  ipu_mmio_write32(0x10002010,0x40000000u);
  CHECK(rd(0x10002034)==0);
 }
 unsigned cases=0;
 for (unsigned q=0;q<32;q++) for (unsigned nl=0;nl<2;nl++)
 for (unsigned quant=0;quant<2;quant++) for (unsigned dtd=0;dtd<2;dtd++)
 for (unsigned il=0;il<(dtd?2u:1u);il++) {
  decode_case(q,nl,quant,dtd,il); cases++;
 }
 printf("IDEC field/cosine oracle: %u cases, %u checks, %u failures\n",cases,checks,failures);
 return failures?1:0;
}
