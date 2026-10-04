#ifndef GUEST_ENDIAN_H
#define GUEST_ENDIAN_H
#include <stdint.h>
static inline uint32_t guest_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
/* R1278: after the caller has mapped/validated all eight bytes.
 * PPC already optimizes LE32 into lwbrx; expose the two 32-bit halves
 * for aligned 64-bit loads. Keep the original unaligned byte loop. */
static inline uint64_t guest_read_le64(const uint8_t *p)
{
    if (((uintptr_t)p & 3u) == 0u)
        return (uint64_t)guest_read_le32(p) | ((uint64_t)guest_read_le32(p+4) << 32);
    uint64_t v=0;
    for(int i=7;i>=0;i--)v=(v<<8)|p[i];
    return v;
}
#endif
