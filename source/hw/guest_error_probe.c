#include "core/hw/guest_error_probe.h"
#include <string.h>

static int within(const ee_state_t *st, uint32_t addr, uint32_t size)
{
    return st->ram && addr <= st->ram_size && size <= st->ram_size - addr;
}
static uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void read_string(const ee_state_t *st, uint32_t addr, char *out, unsigned cap)
{
    unsigned i=0;
    if (!cap) return;
    if (!within(st,addr,1)) { out[0]=0; return; }
    for (; i+1<cap && within(st,addr+i,1); i++) {
        unsigned char c=st->ram[addr+i];
        if (!c) break;
        /* A guest string can be arbitrary bytes; don't let it inject new log lines. */
        out[i]=(c>=32u && c<=126u && c!='"')?(char)c:'?';
    }
    out[i]=0;
}
int gekko2_guest_error_probe(const ee_state_t *st, gekko2_guest_error_t *out)
{
    if (!st || !out || (st->pc != 0x00103d70u && st->pc != 0x00103d74u)) return 0;
    /* The exact program and fatal-loop fingerprint are mandatory: an arbitrary
     * other game with the same PC must never have its stack emitted. */
    if (!within(st,0x00103d70u,8u) || !within(st,0x00468800u,8u) ||
        load32(st->ram+0x00103d70u)!=0x1000ffffu ||
        load32(st->ram+0x00103d74u)!=0u ||
        memcmp(st->ram+0x00468800u,"nu2.err",8u)) return 0;
    memset(out,0,sizeof(*out));
    out->pc=st->pc;
    out->sp=(uint32_t)st->gpr[29].ud0 & 0x1fffffffu;
    if (out->sp > 0xffffffffu-0x1124u ||
        !within(st,out->sp,1u) || !within(st,out->sp+0x1120u,4u)) return 0;
    {
        read_string(st,out->sp+0x1000u,out->heading,sizeof(out->heading));
        read_string(st,out->sp,out->detail,sizeof(out->detail));
        if (within(st,out->sp+0x1120u,4u))
            out->caller=load32(st->ram+out->sp+0x1120u);
    }
    return 1;
}
