#ifndef CONTROLLER_USB_MOCK_GCCORE
#define CONTROLLER_USB_MOCK_GCCORE
#include <stdint.h>
#include <stddef.h>
typedef int32_t s32;typedef uint32_t u32;typedef uint16_t u16;typedef uint8_t u8;
#define ATTRIBUTE_ALIGN(x) __attribute__((aligned(x)))
static inline u32 IRQ_Disable(void){return 0;}
static inline void IRQ_Restore(u32 v){(void)v;}
#endif
