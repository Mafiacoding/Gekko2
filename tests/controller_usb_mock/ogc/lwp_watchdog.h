#include <stdint.h>
uint64_t gettime(void);
#define secs_to_ticks(x) ((uint64_t)(x)*1000)
#define ticks_to_millisecs(x) (x)
