#ifndef GEKKO2_AUDIO_OUTPUT_H
#define GEKKO2_AUDIO_OUTPUT_H
#include <stdint.h>
/* Guest-produced 48 kHz stereo PCM only. The host never advances SPU2. */
typedef struct {uint32_t queued,submitted,played,dropped,underrun,nonzero,running;} audio_output_info;
void audio_output_reset(void);
void audio_output_start(void);
void audio_output_stop(void);
void audio_output_submit(const int16_t *lr,unsigned frames);
/* Portable consumer used by the Wii DMA callback and regression tests. */
void audio_output_consume(int16_t *lr,unsigned frames);
void audio_output_get_info(audio_output_info *out);
#endif
