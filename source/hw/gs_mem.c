/* Shared GS VRAM with native color page/block/column addressing. */

#include "core/hw/gs_mem.h"
#include <string.h>

static uint8_t g_gs_mem[GS_MEM_SIZE];

static gs_mem_gpu_resolver g_gpu_resolver;
static void *g_gpu_opaque;
static uint32_t g_gpu_pending, g_sync_active, g_sync_failures;
static uint32_t g_protected_lo,g_protected_hi,g_read_guard;
int gs_mem_sync(void)
{
    if(g_sync_active)return 0;
    if(!g_gpu_pending)return 1;
    if(!g_gpu_resolver){g_sync_failures++;return 0;}
    g_sync_active=1;
    int ok=g_gpu_resolver(g_gpu_opaque,g_gs_mem,GS_MEM_SIZE);
    g_sync_active=0;
    if(!ok){g_sync_failures++;return 0;}
    g_gpu_pending=0;g_read_guard=0;return 1;
}
int gs_mem_gpu_bind(gs_mem_gpu_resolver resolver,void *opaque)
{
    if(!gs_mem_sync())return 0;
    g_gpu_resolver=resolver;g_gpu_opaque=opaque;g_read_guard=0;return 1;
}
int gs_mem_gpu_mark_pending(void)
{
    if(g_sync_active||!g_gpu_resolver)return 0;
    g_gpu_pending=1;return 1;
}
uint32_t gs_mem_gpu_pending(void){return g_gpu_pending;}
uint32_t gs_mem_sync_failures(void){return g_sync_failures;}
int gs_mem_gpu_protect_range(uint32_t lo,uint32_t hi)
{
 if(g_sync_active||!g_gpu_resolver||lo>=hi||hi>GS_MEM_SIZE)return 0;
 g_protected_lo=lo;g_protected_hi=hi;g_read_guard=1;return 1;
}
static inline int cpu_read_access(uint32_t off,uint32_t bytes)
{
 if(!g_gpu_pending)return 1;
 if(g_read_guard&&off<=GS_MEM_SIZE-bytes&&(off+bytes<=g_protected_lo||off>=g_protected_hi))return 1;
 return gs_mem_sync();
}
int gs_mem_hash_range(uint32_t off,uint32_t bytes,uint64_t *hash)
{
 if(!hash||off>GS_MEM_SIZE||bytes>GS_MEM_SIZE-off||!cpu_read_access(off,bytes))return 0;
 uint32_t a=2166136261u,b=0x9e3779b9u,i=0;
 for(;i+4u<=bytes;i+=4u) {
  uint32_t word;memcpy(&word,g_gs_mem+off+i,4);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  word=__builtin_bswap32(word);
#endif
  a=(a^word)*16777619u;b=((b<<5)|(b>>27))^word;b+=0x9e3779b9u;
 }
 for(;i<bytes;i++){a=(a^g_gs_mem[off+i])*16777619u;b=((b<<5)|(b>>27))^g_gs_mem[off+i];}
 *hash=((uint64_t)a<<32)|b;return 1;
}
static inline int cpu_access(void)
{ return !g_gpu_pending || gs_mem_sync(); }
void gs_mem_init(void)
{ if(cpu_access())memset(g_gs_mem,0,sizeof(g_gs_mem)); }
uint8_t *gs_mem_get(void)
{ return cpu_access()?g_gs_mem:NULL; }

/* Native GS page, block and column addressing. Primary table source:
 * PCSX2 developer tellowkrinkle's GS Memory Swizzle Visualizer:
 * https://gist.github.com/tellowkrinkle/bd6c6e1735cf5e03110ec57ddeea43a9
 * BP remains measured in 32-bit words at this API boundary. */
static inline uint32_t pixel_offset(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint32_t pages = bw / 64u; if (!pages) pages = 1;
    uint32_t bx = (x & 63u) >> 3, by = (y & 31u) >> 3;
    uint32_t block = (bx & 1u) | ((by & 1u) << 1) |
                     ((bx & 2u) << 1) | ((by & 2u) << 2) | ((bx & 4u) << 2);
    uint32_t word = (x & 1u) | ((y & 1u) << 1) |
                    ((x & 6u) << 1) | ((y & 6u) << 3);
    return bp * 4u + ((y / 32u) * pages + x / 64u) * 8192u + block * 256u + word * 4u;
}
static inline uint32_t pixel_offset16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, int is_s)
{
    uint32_t pages = bw / 64u; if (!pages) pages = 1;
    uint32_t bx = (x & 63u) >> 4, by = (y & 63u) >> 3;
    uint32_t block = is_s ?
        ((by & 1u) | ((bx & 1u) << 1) | ((by & 4u)) | ((by & 2u) << 2) | ((bx & 2u) << 3)) :
        ((by & 1u) | ((bx & 1u) << 1) | ((by & 2u) << 1) | ((bx & 2u) << 2) | ((by & 4u) << 2));
    uint32_t half = ((x & 8u) >> 3) | ((x & 1u) << 1) |
                    ((y & 1u) << 2) | ((x & 6u) << 2) | ((y & 6u) << 4);
    return bp * 4u + ((y / 64u) * pages + x / 64u) * 8192u + block * 256u + half * 2u;
}

uint32_t gs_mem_read_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint32_t off = pixel_offset(bp, bw, x, y);
    if (off > GS_MEM_SIZE - 4u) return 0;
    if(!cpu_read_access((uint32_t)off,4u))return 0;
    return (uint32_t)g_gs_mem[off] | ((uint32_t)g_gs_mem[off + 1] << 8) |
           ((uint32_t)g_gs_mem[off + 2] << 16) | ((uint32_t)g_gs_mem[off + 3] << 24);
}

/* R1284: horizontal PSMCT32 byte offsets within a 64-pixel page row.
 * Read-only addressing table, not cached guest pixels. */
static const uint16_t psmct32_x_offset[64] = {
    0,4,16,20,32,36,48,52,
    256,260,272,276,288,292,304,308,
    1024,1028,1040,1044,1056,1060,1072,1076,
    1280,1284,1296,1300,1312,1316,1328,1332,
    4096,4100,4112,4116,4128,4132,4144,4148,
    4352,4356,4368,4372,4384,4388,4400,4404,
    5120,5124,5136,5140,5152,5156,5168,5172,
    5376,5380,5392,5396,5408,5412,5424,5428,
};

/* R1282: contiguous scanout shares page-row and block/column Y work.
 * No persistent pixel cache: every call sees current shared GS VRAM. */
void gs_mem_read_psmct32_span(uint32_t *out,uint32_t bp,uint32_t bw,
                             uint32_t x,uint32_t y,uint32_t count)
{
    if(!out)return;
    uint32_t pages=bw/64u;if(!pages)pages=1u;
    uint32_t by=(y&31u)>>3;
    uint32_t row_base=bp*4u+(y/32u)*pages*8192u+
        ((by&1u)<<1)*256u+((by&2u)<<2)*256u+
        (((y&1u)<<1)|((y&6u)<<3))*4u;
    for(uint32_t n=0;n<count;n++,x++) {
        uint32_t off=row_base+(x/64u)*8192u+psmct32_x_offset[x&63u];
        uint32_t value=0;
        if(off<=GS_MEM_SIZE-4u) {
            if(!cpu_read_access(off,4u)){memset(out,0,(size_t)count*sizeof(*out));return;}
            memcpy(&value,g_gs_mem+off,4);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            value=__builtin_bswap32(value);
#endif
        }
        out[n]=value;
    }
}

/* R1285: pure PSMCT32 row fill, same addressing and bounds as scalar writes. */
void gs_mem_fill_psmct32_span(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                             uint32_t count,uint32_t rgba)
{
    if(!cpu_access())return;
    uint32_t pages=bw/64u;if(!pages)pages=1u;
    uint32_t by=(y&31u)>>3;
    uint32_t row_base=bp*4u+(y/32u)*pages*8192u+
        ((by&1u)<<1)*256u+((by&2u)<<2)*256u+
        (((y&1u)<<1)|((y&6u)<<3))*4u;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    rgba=__builtin_bswap32(rgba);
#endif
    for(uint32_t n=0;n<count;n++,x++) {
        uint32_t off=row_base+(x/64u)*8192u+psmct32_x_offset[x&63u];
        if(off<=GS_MEM_SIZE-4u)memcpy(g_gs_mem+off,&rgba,4);
    }
}

void gs_mem_write_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba)
{
    if(!cpu_access())return;
    uint32_t off = pixel_offset(bp, bw, x, y);
    if (off > GS_MEM_SIZE - 4u) return;
    for (unsigned i = 0; i < 4; i++) g_gs_mem[off + i] = (uint8_t)(rgba >> (i * 8u));
}

/* Compatibility wrapper for the historical page-unit API. */
uint32_t gs_mem_swizzle_addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    return pixel_offset(bp * 2048u, bw, x, y);
}

uint32_t gs_mem_read_psmct32_swizzled(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint32_t off = gs_mem_swizzle_addr32(bp, bw, x, y);
    if (off > GS_MEM_SIZE - 4u) return 0;
    if(!cpu_read_access((uint32_t)off,4u))return 0;
    return (uint32_t)g_gs_mem[off] | ((uint32_t)g_gs_mem[off + 1] << 8) |
           ((uint32_t)g_gs_mem[off + 2] << 16) | ((uint32_t)g_gs_mem[off + 3] << 24);
}

void gs_mem_write_psmct32_swizzled(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba)
{
    if(!cpu_access())return;
    uint32_t off = gs_mem_swizzle_addr32(bp, bw, x, y);
    if (off > GS_MEM_SIZE - 4u) return;
    for (unsigned i = 0; i < 4; i++) g_gs_mem[off + i] = (uint8_t)(rgba >> (i * 8u));
}

/* Native 16-bit view of the same VRAM: preserve the actual two-byte
 * texel size so adjacent texture allocations cannot overwrite each other. */
uint16_t gs_mem_read_psmct16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint64_t off = pixel_offset16(bp, bw, x, y, 0);
    if (off > GS_MEM_SIZE - 2u) return 0;
    if(!cpu_read_access((uint32_t)off,2u))return 0;
    return (uint16_t)(g_gs_mem[off] | ((uint16_t)g_gs_mem[off + 1u] << 8));
}
void gs_mem_write_psmct16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint16_t v)
{
    if(!cpu_access())return;
    uint64_t off = pixel_offset16(bp, bw, x, y, 0);
    if (off > GS_MEM_SIZE - 2u) return;
    g_gs_mem[off] = (uint8_t)v;
    g_gs_mem[off + 1u] = (uint8_t)(v >> 8);
}

uint16_t gs_mem_read_psmct16s(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint32_t off = pixel_offset16(bp, bw, x, y, 1);
    if (off > GS_MEM_SIZE - 2u) return 0;
    if(!cpu_read_access((uint32_t)off,2u))return 0;
    return (uint16_t)(g_gs_mem[off] | ((uint16_t)g_gs_mem[off + 1u] << 8));
}
void gs_mem_write_psmct16s(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint16_t v)
{
    if(!cpu_access())return;
    uint32_t off = pixel_offset16(bp, bw, x, y, 1);
    if (off > GS_MEM_SIZE - 2u) return;
    g_gs_mem[off] = (uint8_t)v;
    g_gs_mem[off + 1u] = (uint8_t)(v >> 8);
}

/* Z formats use the color column layout with a 0x18 block-address XOR. */
uint32_t gs_mem_z_offset(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm)
{
    if(psm!=0u&&psm!=1u&&psm!=2u&&psm!=10u)return UINT32_MAX;
    if(bp>=GS_MEM_SIZE/4u||bw>2048u||x>2047u||y>2047u)return UINT32_MAX;
    uint32_t off=((psm==2u||psm==10u)?pixel_offset16(bp,bw,x,y,psm==10u):pixel_offset(bp,bw,x,y))^6144u;
    unsigned bytes=(psm==2u||psm==10u)?2u:(psm==1u?3u:4u);
    return off<=GS_MEM_SIZE-bytes?off:UINT32_MAX;
}
uint32_t gs_mem_read_z(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t psm)
{
    uint32_t off = ((psm == 2 || psm == 10) ? pixel_offset16(bp,bw,x,y,psm == 10) : pixel_offset(bp,bw,x,y)) ^ 6144u;
    unsigned bytes = (psm == 2 || psm == 10) ? 2 : (psm == 1 ? 3 : 4);
    if (off > GS_MEM_SIZE - bytes) return 0;
    if(!cpu_read_access((uint32_t)off,bytes))return 0;
    uint32_t value = 0;
    for (unsigned i=0;i<bytes;i++) value |= (uint32_t)g_gs_mem[off+i] << (8*i);
    return value;
}
void gs_mem_write_z(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t psm, uint32_t value)
{
    if(!cpu_access())return;
    uint32_t off = ((psm == 2 || psm == 10) ? pixel_offset16(bp,bw,x,y,psm == 10) : pixel_offset(bp,bw,x,y)) ^ 6144u;
    unsigned bytes = (psm == 2 || psm == 10) ? 2 : (psm == 1 ? 3 : 4);
    if (off > GS_MEM_SIZE - bytes) return;
    for (unsigned i=0;i<bytes;i++) g_gs_mem[off+i] = (uint8_t)(value >> (8*i));
}

/* Indexed formats: native page/block/column permutations, not one index per word.
 * Mapping cross-checked with TellowKrinkle's GS swizzle visualizer. */
static uint32_t index_offset(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,int four)
{
    uint32_t pages=bw/128u;if(!pages)pages=1;
    uint32_t bx=(x&127u)>>(four?5:4),by=(y&(four?127u:63u))>>4;
    uint32_t block=four?
        ((by&1)|((bx&1)<<1)|((by&2)<<1)|((bx&2)<<2)|((by&4)<<2)):
        ((bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2));
    uint32_t column=four?
        (((x&1)<<3)|((x&2)<<4)|((((x>>2)^(y>>1)^(y>>2))&1)<<6)|((x&24)>>2)|((y&1)<<4)|((y&2)>>1)|((y&12)<<5)):
        (((x&1)<<2)|((x&2)<<3)|((((x>>2)^(y>>1)^(y>>2))&1)<<5)|((x&8)>>2)|((y&1)<<3)|((y&2)>>1)|((y&12)<<4));
    uint32_t base=bp*4u+((y/(four?128u:64u))*pages+x/128u)*8192u+block*256u;
    return four?base*2u+column:base+column;
}
uint32_t gs_mem_read_index(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm)
{
    if(psm==0x13){uint32_t off=index_offset(bp,bw,x,y,0);return off<GS_MEM_SIZE&&cpu_read_access(off,1u)?g_gs_mem[off]:0;}
    if(psm==0x14){uint32_t off=index_offset(bp,bw,x,y,1);return off/2<GS_MEM_SIZE&&cpu_read_access(off/2,1u)?(g_gs_mem[off/2]>>((off&1)*4))&15:0;}
    uint32_t value=gs_mem_read_psmct32(bp,bw,x,y);
    return psm==0x1b?value>>24:((value>>(psm==0x24?24:28))&15);
}
void gs_mem_write_index(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm,uint32_t value)
{
    if(!cpu_access())return;
    if(psm==0x13){uint32_t off=index_offset(bp,bw,x,y,0);if(off<GS_MEM_SIZE)g_gs_mem[off]=(uint8_t)value;return;}
    if(psm==0x14){uint32_t off=index_offset(bp,bw,x,y,1);if(off/2<GS_MEM_SIZE){unsigned sh=(off&1)*4;g_gs_mem[off/2]=(g_gs_mem[off/2]&~(15u<<sh))|((value&15)<<sh);}return;}
    unsigned sh=psm==0x2c?28:24;uint32_t mask=psm==0x1b?255:15;
    uint32_t prior=gs_mem_read_psmct32(bp,bw,x,y);
    gs_mem_write_psmct32(bp,bw,x,y,(prior&~(mask<<sh))|((value&mask)<<sh));
}
