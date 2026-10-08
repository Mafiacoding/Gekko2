/* GPL-3.0+. Retail RSPU2 extension transport, scoped by ee_core to its
 * verified provider. This is synchronous HLE, not execution of an IRX. */
#ifndef GEKKO2_RSPU2_STREAM_H
#define GEKKO2_RSPU2_STREAM_H
#include <stdint.h>
typedef struct {
 uint32_t configured, initialized, file_lba, file_bytes;
 uint32_t film_bytes[3], audio_bytes[3];
 uint32_t resource_state, stream_state, transfer_done, error;
 uint32_t next_sector, remaining_sectors, destination, transferred_sectors;
 uint32_t init_buffer, calls, reads, failures, last_command;
 uint32_t audio_sectors_per_chunk,video_sectors_per_chunk,chunk_video_left;
 uint32_t video_remaining,audio_sectors_read,audio_prefix_size,audio_prefix_progress;
 uint32_t audio_parameters[48][6];
 uint8_t audio_prefix[16*2048]; /* bounded latest IOP-style staging window */
} rspu2_stream_state_t;
void rspu2_stream_reset(void);
void rspu2_stream_configure(uint32_t lba,uint32_t bytes,const uint32_t films[3],const uint32_t audio[3]);
/* Words are decoded guest little-endian. Returns 0 only for unknown commands. */
int rspu2_stream_rpc(uint32_t command,const uint32_t *words,unsigned count,
                     uint8_t *ee_ram,uint32_t ram_size,int32_t *reply);
uint32_t rspu2_stream_status(void);
const rspu2_stream_state_t *rspu2_stream_get_state(void);
void rspu2_stream_resource_result(int32_t result);
void rspu2_stream_checkpoint_save(rspu2_stream_state_t *out);
int rspu2_stream_state_valid(const rspu2_stream_state_t *state);
void rspu2_stream_checkpoint_load(const rspu2_stream_state_t *state);
#endif
