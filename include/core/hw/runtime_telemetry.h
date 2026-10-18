#ifndef GEKKO2_RUNTIME_TELEMETRY_H
#define GEKKO2_RUNTIME_TELEMETRY_H
#include <stddef.h>
#include <stdint.h>
#include "core/runtime_profile.h"
/* Fixed big-endian wire schema; no guest addresses, strings, memory or files. */
enum { RT_BOOT,RT_SEQ,RT_SESSION,RT_MS,RT_MASK,RT_GX,RT_EXPERIMENTS,RT_INTERVAL,
 RT_PRESENTS,RT_VBLANKS,RT_EE,RT_CORE_TB,RT_PRESENT_TB,RT_LOG_TB,RT_GX_TIMEOUTS,
 RT_BLOCK_RETIRED,RT_BUDGET_MISSES,RT_WAY_SCANS,RT_DROPPED,RT_ERRORS,RT_SENT,
 RT_EVENT,RT_EPOCH,RT_FIELDS };
#define RT_PACKET_BYTES (128u+8u*(RT_FIELDS+2u*(3u*GP_COUNT+2u)))
#define RT_QUEUE 4u
typedef struct { uint32_t ip;uint16_t port; } runtime_telemetry_config;
typedef struct { uint64_t value[RT_FIELDS];gekko2_profile cpu,present; } runtime_telemetry_sample;
int runtime_telemetry_parse(const char *,runtime_telemetry_config *);
size_t runtime_telemetry_encode(uint8_t *,size_t,const runtime_telemetry_sample *);
/* Host-testable bounded single-producer/single-consumer storage. Wii serializes
 * these operations with IRQ_Disable; no network IO while holding that guard. */
typedef struct { uint8_t data[RT_QUEUE][RT_PACKET_BYTES];unsigned head,count; } runtime_telemetry_queue;
int runtime_telemetry_push(runtime_telemetry_queue *,const uint8_t *);
int runtime_telemetry_pop(runtime_telemetry_queue *,uint8_t *);
void runtime_telemetry_start(const char *path);
int runtime_telemetry_state(void); /* 0 OFF, 1 starting, 2 ready, 3 failed */
void runtime_telemetry_submit(runtime_telemetry_sample *);
#endif
