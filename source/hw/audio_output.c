/* SPDX-License-Identifier: GPL-3.0-or-later
 * libogc audio.h: aligned DMA pages, 48 kHz DSP output.
 * Producer and consumer never block. Decode stays on the emulation thread;
 * interrupt callback only copies queued PCM and pads underflow with silence. */
#include "core/hw/audio_output.h"
#include <string.h>
#ifdef GEKKO
#include <gccore.h>
#include <ogc/audio.h>
#include <ogc/machine/processor.h>
#endif
#define QUEUE_FRAMES 8192u
#define DMA_FRAMES 512u
static int16_t queue[QUEUE_FRAMES*2u];
static unsigned read_pos,write_pos;
static audio_output_info info;
#ifdef GEKKO
static int16_t pages[2][DMA_FRAMES*2u] __attribute__((aligned(32)));
static unsigned page;
static unsigned lock(void){u32 level;_CPU_ISR_Disable(level);return level;}
static void unlock(unsigned level){_CPU_ISR_Restore(level);}
#else
static unsigned lock(void){return 0;}
static void unlock(unsigned level){(void)level;}
#endif
void audio_output_stop(void)
{
 unsigned level=lock();
#ifdef GEKKO
 if(info.running){AUDIO_StopDMA();AUDIO_RegisterDMACallback(NULL);}
#endif
 info.running=0;info.queued=0;read_pos=write_pos=0;unlock(level);
}
void audio_output_reset(void){audio_output_stop();memset(&info,0,sizeof info);}
void audio_output_submit(const int16_t *lr,unsigned frames)
{
 if(!lr)return;
 /* Small chunks bound interrupt masking even for large native test input. */
 while(frames){unsigned n=frames>64u?64u:frames,level=lock();
  info.submitted+=n;
  for(unsigned i=0;i<n;i++){
   if(info.queued==QUEUE_FRAMES){info.dropped++;continue;}
   int16_t l=lr[i*2u],r=lr[i*2u+1u];
   queue[write_pos*2u]=l;queue[write_pos*2u+1u]=r;
   write_pos=(write_pos+1u)&(QUEUE_FRAMES-1u);info.queued++;
   if(l||r)info.nonzero++;
  }
  unlock(level);lr+=n*2u;frames-=n;
 }
}
void audio_output_consume(int16_t *lr,unsigned frames)
{
 if(!lr)return;unsigned level=lock();
 for(unsigned i=0;i<frames;i++){
  if(info.queued){lr[i*2u]=queue[read_pos*2u];lr[i*2u+1u]=queue[read_pos*2u+1u];
   read_pos=(read_pos+1u)&(QUEUE_FRAMES-1u);info.queued--;info.played++;
  }else{lr[i*2u]=lr[i*2u+1u]=0;info.underrun++;}
 }
 unlock(level);
}
#ifdef GEKKO
static void dma_done(void)
{
 page^=1u;audio_output_consume(pages[page],DMA_FRAMES);
 /* Wii DSP DMA stereo order is R,L, native big-endian signed 16-bit. */
 for(unsigned i=0;i<DMA_FRAMES;i++){int16_t l=pages[page][i*2u];pages[page][i*2u]=pages[page][i*2u+1u];pages[page][i*2u+1u]=l;}
 DCFlushRange(pages[page],sizeof pages[page]);
 AUDIO_InitDMA((u32)pages[page],sizeof pages[page]);AUDIO_StartDMA();
}
#endif
void audio_output_start(void)
{
 if(info.running)return;
#ifdef GEKKO
 AUDIO_Init(NULL);AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
 memset(pages,0,sizeof pages);DCFlushRange(pages,sizeof pages);page=0;
 info.running=1;AUDIO_RegisterDMACallback(dma_done);AUDIO_InitDMA((u32)pages[0],sizeof pages[0]);AUDIO_StartDMA();
#endif
 info.running=1;
}
void audio_output_get_info(audio_output_info *out){if(!out)return;unsigned level=lock();*out=info;unlock(level);}
