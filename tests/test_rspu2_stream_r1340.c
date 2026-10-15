/* Synthetic disc/RAM contract tests; no copyrighted data or console access. */
#include "core/hw/rspu2_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[16384];static unsigned reads,writes;static int fail_sector=-1;
int iop_cdvd_disc_read_sector(uint32_t sector,uint8_t *out)
{reads++;if(sector==(uint32_t)fail_sector)return -1;for(unsigned k=0;k<2048;k++)out[k]=(uint8_t)(sector+k);return 0;}
void ee_jit_notify_physical_write(uint32_t address,uint32_t bytes)
{assert(address+bytes<=sizeof ram&&bytes==2048);writes++;}
static int32_t rpc(uint32_t cmd,uint32_t a,uint32_t b,uint32_t c,unsigned n)
{uint32_t args[7]={0,a,b,c,4,0,0};int32_t out=123;assert(rspu2_stream_rpc(cmd,args,n,ram,sizeof ram,&out));return out;}
int main(void)
{
 uint32_t films[3]={8192,4096,2048},audio[3]={2048,1024,512};rspu2_stream_state_t saved,bad;
 rspu2_stream_reset();assert(rpc(0x2058,0x4820,0,0,5)==-1&&reads==0);
 rspu2_stream_configure(100,65536,films,audio);
 assert(rpc(0x2000,0,0,0,3)==-1);
 assert(rpc(0x2000,0x2237,0x20004000,0,3)==0);
 assert(rpc(0x2030,0,0,0,1)==0x44);
 rspu2_stream_resource_result(0);assert(rspu2_stream_status()==0x40);
 assert(rpc(0x2058,0x4820,3,0,5)==-1&&reads==0);
 assert(rpc(0x2058,0x4820,0,0,5)==0&&rspu2_stream_status()==0);
 assert(rpc(0x2058,0x4820,0,0,5)==-1); /* already streaming */
 memset(ram,0xa5,sizeof ram);
 assert(rpc(0x2059,0x20003c00,2,0,3)==-1&&writes==0);
 assert(rpc(0x2059,0x20000080,2,0,3)==0&&writes==2);
 for(unsigned k=0;k<4096;k++)assert(ram[128+k]==(uint8_t)(100+k/2048+k%2048));
 assert(ram[127]==0xa5&&ram[4224]==0xa5&&rspu2_stream_status()==0x20);
 rspu2_stream_checkpoint_save(&saved);assert(rspu2_stream_state_valid(&saved));
 assert(saved.next_sector==2&&saved.remaining_sectors==2&&saved.transferred_sectors==2);
 bad=saved;bad.remaining_sectors=UINT32_MAX;assert(!rspu2_stream_state_valid(&bad));
 fail_sector=103;
 assert(rpc(0x2059,0x20002000,2,0,3)==-1&&writes==3&&rspu2_stream_status()==4);
 rspu2_stream_checkpoint_save(&bad);assert(bad.next_sector==3&&bad.remaining_sectors==1&&!bad.transfer_done);
 fail_sector=-1;assert(rpc(0x2059,0x20002800,1,0,3)==0&&writes==4);
 assert(rpc(0x2059,0x20002800,1,0,3)==-1&&writes==4); /* no invented EOF bytes */
 rspu2_stream_checkpoint_load(&saved);assert(rspu2_stream_status()==0x20);
 assert(rpc(0x205a,0,0,0,2)==0&&rspu2_stream_status()==0x40);
 assert(rpc(0x205a,1,0,0,2)==1);
 assert(rpc(0x205e,47,1,2,7)==0);
 assert(rpc(0x205e,48,1,2,7)==-1);
 /* Two groups of one audio + three video sectors: audio prefixes must
  * never enter the EE video stream, including a read fault and retry. */
 films[0]=8*2048; rspu2_stream_configure(100,65536,films,audio);
 assert(rpc(0x2000,0x2237,0,0,3)==0);rspu2_stream_resource_result(0);
 uint32_t layout[7]={0,0x4820,0,1,3,0,0};int32_t reply;
 assert(rspu2_stream_rpc(0x2058,layout,7,ram,sizeof ram,&reply)&&reply==0);
 fail_sector=102;
 assert(rpc(0x2059,0x20000080,5,0,3)==-1);
 rspu2_stream_checkpoint_save(&saved);assert(saved.audio_sectors_read==1&&saved.transferred_sectors==1&&saved.chunk_video_left==2);
 fail_sector=-1;
 assert(rpc(0x2059,0x20000880,4,0,3)==0);
 for(unsigned k=0;k<5;k++){unsigned sector=k<3?101+k:102+k;assert(ram[128+k*2048]==(uint8_t)sector);}
 rspu2_stream_checkpoint_save(&saved);assert(saved.audio_sectors_read==2&&saved.transferred_sectors==5&&saved.video_remaining==1);
 assert(saved.audio_prefix_size==2048&&saved.audio_prefix[0]==104&&rspu2_stream_state_valid(&saved));
 /* Exact observed 7/128 grouping, crossing the boundary in one call. */
 films[0]=270*2048; rspu2_stream_configure(100,1048576,films,audio);
 assert(rpc(0x2000,0x2237,0,0,3)==0);rspu2_stream_resource_result(0);
 layout[3]=7;layout[4]=128;
 assert(rspu2_stream_rpc(0x2058,layout,7,ram,sizeof ram,&reply)&&reply==0);
 for(unsigned k=0;k<18;k++)assert(rpc(0x2059,0x20000080,7,0,3)==0);
 assert(rpc(0x2059,0x20000080,4,0,3)==0);
 unsigned expected[4]={233,234,242,243};
 for(unsigned k=0;k<4;k++)assert(ram[128+k*2048]==(uint8_t)expected[k]);
 rspu2_stream_checkpoint_save(&saved);assert(saved.audio_sectors_read==14&&saved.transferred_sectors==130&&saved.video_remaining==126);
 rspu2_stream_checkpoint_load(&(rspu2_stream_state_t){0});assert(rspu2_stream_status()==1);
 int32_t out=77;assert(!rspu2_stream_rpc(0x7777,0,0,ram,sizeof ram,&out)&&out==77);
 puts("PASS R1340 streaming: lifecycle, state-derived status, real bytes, bus bounds, partial read failure, EOF, invalidation, checkpoint");return 0;
}
