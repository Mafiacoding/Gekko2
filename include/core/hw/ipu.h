#ifndef PCSX2WII_IPU_H
#define PCSX2WII_IPU_H
#include <stdint.h>
/* R1335: real bounded input/output FIFOs, FDEC, SETIQ, SETVQ, SETTH
 * and CSC. MPEG IDEC/BDEC/VDEC and PACK remain unsupported, recorded
 * explicitly; they retain legacy acknowledgement until MPEG decoding exists.
 * Register/command semantics: PCSX2 IPU.cpp/IPU.h, see R1335 documentation. */
typedef struct {
 uint64_t commands[16],unimplemented_commands;
 uint64_t input_qwc,accepted_qwc,discarded_qwc;
 uint32_t fifo_count,last_command;
 uint64_t output_qwc,completed_commands,csc_macroblocks,input_stalls,output_stalls;
 uint32_t output_available,busy;
} ipu_profile_t;
typedef struct {
 uint32_t ctrl,data,top,command,busy,bp,fp;
 uint32_t in_head,in_count,out_head,out_count;
 uint32_t skip,pos,blocks,out_pos,out_size;
 uint16_t th0,th1;
 uint8_t input[8][16],internal[2][16],output[8][16];
 uint8_t iq[2][64],vq[32],block[384],converted[1024];
} ipu_state_t;
void ipu_init(void);
void ipu_get_profile(ipu_profile_t *out);
ipu_state_t *ipu_get_state(void);
int ipu_state_valid(const ipu_state_t *state);
void ipu_restore(const ipu_state_t *state);
int ipu_mmio_read32(uint32_t addr,uint32_t *out);
int ipu_mmio_write32(uint32_t addr,uint32_t value);
int ipu_mmio_read64(uint32_t addr,uint64_t *out);
int ipu_mmio_write64(uint32_t addr,uint64_t value);
int ipu_fifo_read128(uint32_t addr,uint8_t out[16]);
int ipu_fifo_write128(uint32_t addr,const uint8_t in[16]);
uint32_t ipu_input_write(const uint8_t *data,uint32_t qwc);
uint32_t ipu_output_read(uint8_t *data,uint32_t qwc);
/* A service pass advances only real queued data. No cycles/guest counters
 * are invented. FIFO starvation preserves BUSY and DMA STR. */
void ipu_service(void);
void ipu_process_quadwords(int channel,const uint8_t *data,uint32_t qwc);
void ipu_csc_convert(const uint8_t input[384],uint8_t *output,
                     uint32_t format16,uint32_t dither,uint16_t th0,uint16_t th1);
#endif
