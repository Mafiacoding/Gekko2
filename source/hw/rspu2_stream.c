/* GPL-3.0+. Derived protocol, no retail module bytes or tables included.
 * Dispatch +0x5b4/698/6d8/708/730/780, initialization +0x890,
 * status +0x2ed0 and streaming +0x2610/+0x2f58 in the whole verified
 * 66,077-byte provider. The synchronous HLE consumes real disc bytes;
 * no worker/semaphore, guest PC or MPEG progress is fabricated. */
#include "core/hw/rspu2_stream.h"
#include "core/hw/iop_cdvd.h"
#include "core/recompiler/ee_jit.h"
#include <string.h>
static rspu2_stream_state_t state;
const rspu2_stream_state_t *rspu2_stream_get_state(void){return &state;}
void rspu2_stream_reset(void){memset(&state,0,sizeof state);}
void rspu2_stream_configure(uint32_t lba,uint32_t bytes,const uint32_t films[3],const uint32_t audio[3])
{
 rspu2_stream_reset();state.configured=1;state.file_lba=lba;state.file_bytes=bytes;
 memcpy(state.film_bytes,films,sizeof state.film_bytes);
 memcpy(state.audio_bytes,audio,sizeof state.audio_bytes);
}
uint32_t rspu2_stream_status(void)
{
 if(!state.initialized)return 1u;
 return (state.resource_state!=1u?1u:0u)|(state.error?4u:0u)|
        (state.stream_state==1u?0x40u:0u)|
        (state.stream_state==3u&&state.transfer_done?0x20u:0u);
}
void rspu2_stream_resource_result(int32_t result)
{
 if(!state.initialized)return;
 state.resource_state=1;state.error=result<0;state.reads++;
 if(result<0)state.failures++;
}
static int failure(int32_t *out)
{state.error=1;state.failures++;*out=-1;return 1;}
int rspu2_stream_rpc(uint32_t cmd,const uint32_t *a,unsigned count,
                     uint8_t *ram,uint32_t size,int32_t *out)
{
 if(cmd!=0x2000u&&cmd!=0x2030u&&cmd!=0x2050u&&cmd!=0x2051u&&
    cmd!=0x2058u&&cmd!=0x2059u&&cmd!=0x205au&&cmd!=0x205eu)return 0;
 if(!out)return 0;
 state.calls++;state.last_command=cmd;*out=-1;
 if(!state.configured)return failure(out);
 if(cmd==0x2000u){
  if(!a||count<3||a[1]!=0x2237u)return failure(out);
  if(!state.initialized){
   state.initialized=1;state.resource_state=state.stream_state=1;
   state.init_buffer=a[2];state.error=1; /* actual power-on read-error flag */
  }
  *out=0;return 1;
 }
 if(cmd==0x2030u){*out=(int32_t)rspu2_stream_status();return 1;}
 if(!state.initialized)return failure(out);
 if(cmd==0x2050u||cmd==0x2051u){*out=0;return 1;} /* synchronous resource service is idle */
 if(cmd==0x205eu){
  if(!a||count<7||a[1]>=48u)return failure(out);
  memcpy(state.audio_parameters[a[1]],a+1,6u*sizeof(uint32_t));*out=0;return 1;
 }
 if(!a||count<2)return failure(out);
 if(cmd==0x205au){
  /* Module stop(0) requests stop and returns zero. stop(1) returns one
   * when streaming and its associated sound operation are idle. */
  state.stream_state=1;state.remaining_sectors=state.video_remaining=0;state.transfer_done=0;
  *out=a[1]?1:0;return 1;
 }
 if(cmd==0x2058u){
  if(count<5||state.stream_state!=1u||a[2]>=3u||a[1]<0x4820u)return failure(out);
  if(a[3]>16u||!a[4]||a[4]>128u||a[3]+a[4]>144u)return failure(out);
  uint32_t sector=a[1]-0x4820u;
  uint32_t n=(state.film_bytes[a[2]]+2047u)/2048u;
  if(!n||(uint64_t)sector*2048u+(uint64_t)n*2048u>state.file_bytes)return failure(out);
  state.next_sector=sector;state.remaining_sectors=n;state.stream_state=3;
  state.transfer_done=0;state.error=0;state.transferred_sectors=0;
  state.audio_sectors_per_chunk=a[3];state.video_sectors_per_chunk=a[4];
  state.chunk_video_left=0;state.audio_sectors_read=state.audio_prefix_size=state.audio_prefix_progress=0;
  uint32_t group=a[3]+a[4],tail=n%group;
  state.video_remaining=(n/group)*a[4]+(tail>a[3]?tail-a[3]:0);
  *out=0;return 1;
 }
 /* +0x2f58 accepts an EE bus address and a sector count. Validate the
  * whole requested RAM span before touching it. Read faults expose only
  * genuine completed sectors, invalidate each written JIT source page,
  * set error, and retain the cursor for the unread sector. */
 if(count<3||state.stream_state!=3u||!ram||!a[1]||!a[2]||a[2]>0x7fffffffu/2048u)return failure(out);
 uint32_t dst=a[1]&0x1fffffffu;
 uint32_t n=a[2];
 if(n>state.video_remaining)return failure(out);
 uint32_t bytes=n*2048u;
 if(dst>=size||bytes>size-dst)return failure(out);
 state.destination=dst;state.transfer_done=0;state.reads++;
 for(uint32_t k=0;k<n;k++){
  if(!state.chunk_video_left) {
   /* Provider +0x27fc/+0x2c7c stages the audio prefix separately;
    * +0x293c..2974 advances the EE source past that prefix. The
    * initialized geometry is supplied by command 0x2058, not guessed.
    * Preserve the latest raw ADPCM prefix. Synthesis of these streaming
    * voices is a separate remaining path; this does not fake playback. */
   if(!state.audio_prefix_progress)state.audio_prefix_size=0;
   for(uint32_t q=state.audio_prefix_progress;q<state.audio_sectors_per_chunk;q++) {
    if(!state.remaining_sectors||iop_cdvd_disc_read_sector(state.file_lba+state.next_sector,state.audio_prefix+q*2048u))return failure(out);
    state.next_sector++;state.remaining_sectors--;state.audio_sectors_read++;
    state.audio_prefix_size+=2048u;state.audio_prefix_progress++;
   }
   state.chunk_video_left=state.video_sectors_per_chunk;
  }
  uint8_t sector[2048];
  if(iop_cdvd_disc_read_sector(state.file_lba+state.next_sector,sector))return failure(out);
  memcpy(ram+dst+k*2048u,sector,sizeof sector);
  ee_jit_notify_physical_write(dst+k*2048u,2048u);
  state.next_sector++;state.remaining_sectors--;state.video_remaining--;state.chunk_video_left--;state.transferred_sectors++;
  if(!state.chunk_video_left)state.audio_prefix_progress=0;
 }
 state.transfer_done=1;state.error=0;*out=0;return 1;
}
void rspu2_stream_checkpoint_save(rspu2_stream_state_t *out){if(out)*out=state;}
int rspu2_stream_state_valid(const rspu2_stream_state_t *s)
{
 if(!s||s->configured>1||s->initialized>1||s->transfer_done>1||s->error>1)return 0;
 if(!s->configured)return !s->initialized&&!s->remaining_sectors&&!s->stream_state;
 if(!s->file_bytes||s->file_lba>UINT32_MAX-s->file_bytes/2048u)return 0;
 if(s->initialized&&(s->resource_state!=1u||(s->stream_state!=1u&&s->stream_state!=3u)))return 0;
 if(s->audio_sectors_per_chunk>16u||s->video_sectors_per_chunk>128u||
    s->chunk_video_left>s->video_sectors_per_chunk||s->audio_prefix_size>sizeof s->audio_prefix||
    (s->audio_prefix_size&2047u)||s->audio_prefix_progress>s->audio_sectors_per_chunk||s->video_remaining>s->remaining_sectors)return 0;
 if(s->stream_state==3u&&(!s->video_sectors_per_chunk||s->audio_sectors_per_chunk+s->video_sectors_per_chunk>144u))return 0;
 if(s->remaining_sectors>s->file_bytes/2048u||s->next_sector>s->file_bytes/2048u||
    s->remaining_sectors>s->file_bytes/2048u-s->next_sector)return 0;
 for(unsigned i=0;i<3;i++)if(s->film_bytes[i]>s->file_bytes||s->audio_bytes[i]>s->file_bytes)return 0;
 return 1;
}
void rspu2_stream_checkpoint_load(const rspu2_stream_state_t *s){if(rspu2_stream_state_valid(s))state=*s;else rspu2_stream_reset();}
