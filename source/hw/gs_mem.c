/* Shared GS VRAM with native color page/block/column addressing. */

#include "core/hw/gs_mem.h"
#include "core/recompiler/optimization.h"
#include <string.h>

static uint8_t g_gs_mem[GS_MEM_SIZE];
/* One bounded digest owner. Writes invalidate only when they overlap its
 * physical byte range. Escaping mutable raw pointers disable memoization
 * permanently: an old caller may write through them after any cold boot. */
static struct { uint32_t off,bytes,valid; uint64_t value; } g_hash_owner;
static uint32_t g_hash_raw_exposed;
static uint64_t g_hash_hits,g_hash_bytes_avoided;
uint64_t gs_mem_hash_cache_hits(void){return g_hash_hits;}
uint64_t gs_mem_hash_cache_bytes_avoided(void){return g_hash_bytes_avoided;}
static inline void hash_invalidate_range(uint32_t off,uint32_t bytes)
{
 if(g_hash_owner.valid&&bytes&&off<g_hash_owner.off+g_hash_owner.bytes&&
    g_hash_owner.off<off+bytes)g_hash_owner.valid=0;
}


static gs_mem_gpu_resolver g_gpu_resolver;
static void *g_gpu_opaque;
static uint32_t g_gpu_pending, g_sync_active, g_sync_failures;
static uint32_t g_protected_lo,g_protected_hi,g_read_guard;
/* Conservative physical byte envelopes: GS PSM variants alias the SAME
 * 4 MiB eDRAM, even where texture/depth/color page layouts differ. */
static uint64_t g_alias_queries,g_alias_resolves,g_alias_disjoint,g_alias_failures;
uint64_t gs_mem_alias_stat(unsigned index)
{
    switch(index){case 0:return g_alias_queries;case 1:return g_alias_resolves;
    case 2:return g_alias_disjoint;case 3:return g_alias_failures;default:return 0;}
}
int gs_mem_gpu_sync_alias_range(uint32_t lo,uint32_t hi)
{
    if(lo>hi||hi>GS_MEM_SIZE)return 0;
    g_alias_queries++;
    if(!g_gpu_pending)return 1;
    /* GPU resolver promises to touch only its protected physical interval.
     * Outside that interval CPU/GS source reads remain directly coherent. */
    if(g_read_guard&&(hi<=g_protected_lo||lo>=g_protected_hi)) {
        g_alias_disjoint++;return 1;
    }
    g_alias_resolves++;
    if(!gs_mem_sync()){g_alias_failures++;return 0;}
    return 1;
}
int gs_mem_sync(void)
{
    if(g_sync_active)return 0;
    if(!g_gpu_pending)return 1;
    if(!g_gpu_resolver){g_sync_failures++;return 0;}
    /* A protected resolver promises writes only inside its interval. A
     * disjoint decoded texture digest survives color/depth ownership work. */
    if(!g_read_guard)g_hash_owner.valid=0;
    else hash_invalidate_range(g_protected_lo,g_protected_hi-g_protected_lo);
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
    if(!g_read_guard)g_hash_owner.valid=0;
    else hash_invalidate_range(g_protected_lo,g_protected_hi-g_protected_lo);
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
const uint8_t *gs_mem_read_range(uint32_t off,uint32_t bytes)
{
    if(off>GS_MEM_SIZE||bytes>GS_MEM_SIZE-off||!cpu_read_access(off,bytes))return NULL;
    return g_gs_mem+off;
}
int gs_mem_hash_range(uint32_t off,uint32_t bytes,uint64_t *hash)
{
 if(!hash||off>GS_MEM_SIZE||bytes>GS_MEM_SIZE-off||!cpu_read_access(off,bytes))return 0;
 if(!g_hash_raw_exposed&&gekko2_opt_enabled(GEKKO2_OPT_GX_SOURCE_CACHE)&&
    g_hash_owner.valid&&g_hash_owner.off==off&&g_hash_owner.bytes==bytes) {
  *hash=g_hash_owner.value;g_hash_hits++;g_hash_bytes_avoided+=bytes;return 1;
 }
 uint32_t a=2166136261u,b=0x9e3779b9u,i=0;
 for(;i+4u<=bytes;i+=4u) {
  uint32_t word;memcpy(&word,g_gs_mem+off+i,4);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  word=__builtin_bswap32(word);
#endif
  a=(a^word)*16777619u;b=((b<<5)|(b>>27))^word;b+=0x9e3779b9u;
 }
 for(;i<bytes;i++){a=(a^g_gs_mem[off+i])*16777619u;b=((b<<5)|(b>>27))^g_gs_mem[off+i];}
 *hash=((uint64_t)a<<32)|b;
 if(!g_hash_raw_exposed&&gekko2_opt_enabled(GEKKO2_OPT_GX_SOURCE_CACHE)) {
  g_hash_owner.off=off;g_hash_owner.bytes=bytes;g_hash_owner.value=*hash;g_hash_owner.valid=1;
 }
 return 1;
}
static inline int cpu_access(void)
{ return !g_gpu_pending || gs_mem_sync(); }
static inline int cpu_write_access(uint32_t off,uint32_t bytes)
{
 if(!(gekko2_opt_enabled(GEKKO2_OPT_GX_DISJOINT_WRITES)?cpu_read_access(off,bytes):cpu_access()))return 0;
 hash_invalidate_range(off,bytes);return 1;
}
int gs_mem_write_range(uint32_t off,const void *source,uint32_t bytes)
{
 if((!source&&bytes)||off>GS_MEM_SIZE||bytes>GS_MEM_SIZE-off||!cpu_write_access(off,bytes))return 0;
 if(bytes)memmove(g_gs_mem+off,source,bytes);return 1;
}
void gs_mem_init(void)
{ if(cpu_access()){g_hash_owner.valid=0;g_hash_hits=g_hash_bytes_avoided=0;memset(g_gs_mem,0,sizeof(g_gs_mem));} }
void gs_mem_reset_sync_stats(void)
{ if(!g_gpu_pending&&!g_sync_active){g_sync_failures=0;g_read_guard=0;g_protected_lo=g_protected_hi=0;
  g_alias_queries=g_alias_resolves=g_alias_disjoint=g_alias_failures=0;} }
uint8_t *gs_mem_get(void)
{ if(!cpu_access())return NULL;g_hash_raw_exposed=1;g_hash_owner.valid=0;return g_gs_mem; }

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
    return (bp * 4u + ((y / 32u) * pages + x / 64u) * 8192u + block * 256u + word * 4u) & (GS_MEM_SIZE - 1u);
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
    return (bp * 4u + ((y / 64u) * pages + x / 64u) * 8192u + block * 256u + half * 2u) & (GS_MEM_SIZE - 1u);
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
        uint32_t off=(row_base+(x/64u)*8192u+psmct32_x_offset[x&63u])&(GS_MEM_SIZE-1u);
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
    uint32_t pages=bw/64u;if(!pages)pages=1u;
    uint32_t by=(y&31u)>>3;
    uint32_t row_base=bp*4u+(y/32u)*pages*8192u+
        ((by&1u)<<1)*256u+((by&2u)<<2)*256u+
        (((y&1u)<<1)|((y&6u)<<3))*4u;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    rgba=__builtin_bswap32(rgba);
#endif
    for(uint32_t n=0;n<count;n++,x++) {
        uint32_t off=(row_base+(x/64u)*8192u+psmct32_x_offset[x&63u])&(GS_MEM_SIZE-1u);
        if(off<=GS_MEM_SIZE-4u){if(!cpu_write_access(off,4u))return;memcpy(g_gs_mem+off,&rgba,4);}
    }
}

void gs_mem_write_psmct32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t rgba)
{
    uint32_t off = pixel_offset(bp, bw, x, y);
    if (off > GS_MEM_SIZE - 4u) return;
    if(!cpu_write_access(off,4u))return;
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
    uint32_t off = gs_mem_swizzle_addr32(bp, bw, x, y);
    if (off > GS_MEM_SIZE - 4u) return;
    if(!cpu_write_access(off,4u))return;
    for (unsigned i = 0; i < 4; i++) g_gs_mem[off + i] = (uint8_t)(rgba >> (i * 8u));
}

/* Native 16-bit view of the same VRAM: preserve the actual two-byte
 * texel size so adjacent texture allocations cannot overwrite each other. */
uint32_t gs_mem_framebuffer16_offset(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm)
{
    if(psm!=2u&&psm!=10u)return UINT32_MAX;
    return pixel_offset16(bp,bw,x,y,psm==10u);
}

uint16_t gs_mem_read_psmct16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint64_t off = pixel_offset16(bp, bw, x, y, 0);
    if (off > GS_MEM_SIZE - 2u) return 0;
    if(!cpu_read_access((uint32_t)off,2u))return 0;
    return (uint16_t)(g_gs_mem[off] | ((uint16_t)g_gs_mem[off + 1u] << 8));
}
void gs_mem_write_psmct16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint16_t v)
{
    uint64_t off = pixel_offset16(bp, bw, x, y, 0);
    if (off > GS_MEM_SIZE - 2u) return;
    if(!cpu_write_access((uint32_t)off,2u))return;
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
    uint32_t off = pixel_offset16(bp, bw, x, y, 1);
    if (off > GS_MEM_SIZE - 2u) return;
    if(!cpu_write_access(off,2u))return;
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
    uint32_t off = ((psm == 2 || psm == 10) ? pixel_offset16(bp,bw,x,y,psm == 10) : pixel_offset(bp,bw,x,y)) ^ 6144u;
    unsigned bytes = (psm == 2 || psm == 10) ? 2 : (psm == 1 ? 3 : 4);
    if (off > GS_MEM_SIZE - bytes) return;
    /* Resident RGB owns only its protected bytes. Nonalias CPU Z shadow
     * writes cannot change them; an alias still resolves and fails closed. */
    if(!cpu_read_access(off,bytes))return;
    hash_invalidate_range(off,bytes);
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
    return four?(base*2u+column)&(GS_MEM_SIZE*2u-1u):(base+column)&(GS_MEM_SIZE-1u);
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
    if(psm==0x13){uint32_t off=index_offset(bp,bw,x,y,0);if(off<GS_MEM_SIZE&&cpu_write_access(off,1u))g_gs_mem[off]=(uint8_t)value;return;}
    if(psm==0x14){uint32_t off=index_offset(bp,bw,x,y,1);if(off/2<GS_MEM_SIZE&&cpu_write_access(off/2,1u)){unsigned sh=(off&1)*4;g_gs_mem[off/2]=(g_gs_mem[off/2]&~(15u<<sh))|((value&15)<<sh);}return;}
    unsigned sh=psm==0x2c?28:24;uint32_t mask=psm==0x1b?255:15;
    uint32_t prior=gs_mem_read_psmct32(bp,bw,x,y);
    gs_mem_write_psmct32(bp,bw,x,y,(prior&~(mask<<sh))|((value&mask)<<sh));
}

/* BETA12.11: FORMAT AWARE GS local moves.
 * PCSX2 GSState::Move copies transfer payload of matching bit widths; it
 * does NOT generally perform RGB quantization across different widths.
 * For that reason the Wii hardware-facing GIF path uses strict mode.
 * Cross-bit-depth RGB/indices expansion is an explicitly opt-in emulator
 * extension, never silently enabled during a game. */
/* A Wii-safe bounded snapshot prevents overwriting source bytes when
 * identical physical GS pages are reinterpreted through different PSMs.
 * Larger alias moves fail closed rather than returning corrupted pixels. */
#define GS_LOCAL_ALIAS_SNAPSHOT_PIXELS 16384u
static uint32_t g_local_alias_snapshot[GS_LOCAL_ALIAS_SNAPSHOT_PIXELS];
static uint64_t g_local_alias_snapshots,g_local_alias_pixels,g_local_alias_declines;
uint64_t gs_mem_local_alias_stat(unsigned n)
{ return n==0?g_local_alias_snapshots:n==1?g_local_alias_pixels:n==2?g_local_alias_declines:0; }
static uint64_t g_gs_local_converted, g_gs_local_rejected;
static int g_gs_local_extended_conversion;
uint64_t gs_mem_local_converted_pixels(void){return g_gs_local_converted;}
uint64_t gs_mem_local_rejected_formats(void){return g_gs_local_rejected;}
void gs_mem_set_local_extended_conversion(int enable){g_gs_local_extended_conversion=!!enable;}
int gs_mem_get_local_extended_conversion(void){return g_gs_local_extended_conversion;}
void gs_mem_reset_local_copy_stats(void){g_gs_local_converted=0;g_gs_local_rejected=0;
 g_local_alias_snapshots=g_local_alias_pixels=g_local_alias_declines=0;}

static int local_psm_bits(uint32_t psm)
{
    switch(psm) {
    case 0u: case 0x30u: return 32;
    case 1u: case 0x31u: return 24;
    case 2u: case 10u: case 0x32u: case 0x3au: return 16;
    case 0x13u: case 0x1bu: return 8;
    case 0x14u: case 0x24u: case 0x2cu: return 4;
    default: return 0;
    }
}
static int local_psm_group(uint32_t psm)
{
    if(psm==0x13u || psm==0x1bu || psm==0x14u || psm==0x24u || psm==0x2cu) return 2;
    if(psm==0x30u || psm==0x31u || psm==0x32u || psm==0x3au) return 1;
    if(psm==0u || psm==1u || psm==2u || psm==10u) return 0;
    return -1;
}
/* Use the original pixel helpers for the real native swizzles, with CPU/GX
 * ownership already settled by gs_mem_sync() before entering this code. */
static uint32_t local_load_pixel(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm)
{
    const uint32_t base=bp*64u;
    switch(psm){
    case 0u: return gs_mem_read_psmct32(base,bw,x,y);
    case 1u: return gs_mem_read_psmct32(base,bw,x,y)&0x00ffffffu;
    case 2u: return gs_mem_read_psmct16(base,bw,x,y);
    case 10u: return gs_mem_read_psmct16s(base,bw,x,y);
    case 0x30u: return gs_mem_read_z(base,bw,x,y,0u);
    case 0x31u: return gs_mem_read_z(base,bw,x,y,1u);
    case 0x32u: return gs_mem_read_z(base,bw,x,y,2u);
    case 0x3au: return gs_mem_read_z(base,bw,x,y,10u);
    default: return gs_mem_read_index(base,bw,x,y,psm);
    }
}
static void local_store_pixel(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm,uint32_t value)
{
    const uint32_t base=bp*64u;
    switch(psm){
    case 0u: gs_mem_write_psmct32(base,bw,x,y,value);break;
    case 1u: {
        const uint32_t old=gs_mem_read_psmct32(base,bw,x,y);
        gs_mem_write_psmct32(base,bw,x,y,(old&0xff000000u)|(value&0x00ffffffu));
    }break;
    case 2u: gs_mem_write_psmct16(base,bw,x,y,(uint16_t)value);break;
    case 10u: gs_mem_write_psmct16s(base,bw,x,y,(uint16_t)value);break;
    case 0x30u: gs_mem_write_z(base,bw,x,y,0u,value);break;
    case 0x31u: gs_mem_write_z(base,bw,x,y,1u,value);break;
    case 0x32u: gs_mem_write_z(base,bw,x,y,2u,value);break;
    case 0x3au: gs_mem_write_z(base,bw,x,y,10u,value);break;
    default: gs_mem_write_index(base,bw,x,y,psm,value);break;
    }
}
/* Conservative native page range. Cross-layout moves touching the same
 * physical pages are rejected, rather than corrupting aliased source data.
 * Callers receive an ordinary failed move, never a partially guessed copy. */
static int local_page_range(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                            uint32_t w,uint32_t h,uint32_t psm,
                            uint64_t *lo,uint64_t *hi)
{
    const int bits=local_psm_bits(psm);
    /* PSMT8H/4HL/4HH share the 32-bit page geometry. */
    const int packed_h=(psm==0x1bu || psm==0x24u || psm==0x2cu);
    const uint32_t page_w=(!packed_h&&(bits==8||bits==4))?128u:64u;
    /* PSMCT16/16S and PSMZ16/16S pages are 64x64 (not 64x128).
     * PSMT4 is 128x128; PSMT8 128x64; upper-byte formats 64x32. */
    const uint32_t page_h=(packed_h||bits==32||bits==24)?32u:
                          (bits==16||bits==8)?64u:128u;
    uint64_t pages=bw/page_w;if(!pages)pages=1;
    const uint64_t first=(uint64_t)(y/page_h)*pages+x/page_w;
    const uint64_t last=(uint64_t)((y+h-1u)/page_h)*pages+(x+w-1u)/page_w;
    const uint64_t raw_lo=(uint64_t)bp*256u+first*8192u;
    const uint64_t raw_hi=(uint64_t)bp*256u+(last+1u)*8192u;
    /* eDRAM wraps modulo 4 MiB.  A wrapped page interval cannot be
     * expressed by the older [lo,hi) API; return a conservative full
     * memory envelope instead of treating legal GS addresses as invalid
     * or missing an alias across the physical zero boundary. */
    if(raw_lo>=GS_MEM_SIZE || raw_hi>GS_MEM_SIZE){
        *lo=0;*hi=GS_MEM_SIZE;return 1;
    }
    *lo=raw_lo;*hi=raw_hi;
    return *lo<*hi;
}
static uint32_t local_rgb16_to_32(uint32_t v)
{
    uint32_t r=v&31u,g=(v>>5)&31u,b=(v>>10)&31u;
    return (r<<3)|(r>>2)|((g<<3)|(g>>2))<<8|(((b<<3)|(b>>2))<<16)|((v&0x8000u)?0x80000000u:0u);
}
static uint32_t local_rgb32_to_16(uint32_t v)
{
    return ((v&0xffu)>>3)|(((v>>8)&0xffu)>>3)<<5|(((v>>16)&0xffu)>>3)<<10|
           (((v>>24)>=0x40u)?0x8000u:0u);
}
static uint32_t local_convert_bits(uint32_t v,int sbits,int dbits,int group)
{
    if(group==2)return dbits==4?(v&15u):v&255u;
    if(group==1)return dbits==16?(v&65535u):(dbits==24?v&0x00ffffffu:v);
    /* RGB conversion is a software extension; not asserted to be GS HW. */
    if(sbits==16)v=local_rgb16_to_32(v);
    else if(sbits==24)v=(v&0x00ffffffu)|0x80000000u;
    if(dbits==16)return local_rgb32_to_16(v);
    if(dbits==24)return v&0x00ffffffu;
    return v;
}

/* BETA12.9: synchronous GS local-to-local transfer, initiated by
 * GIFRegTRXDIR.XDIR=2. Never synthesize a front/back buffer flip.
 * Restrict conversions to equal supported PSMs; guesses about PSM
 * conversion are worse than a logged unsupported copy. */
int gs_mem_copy_local(const gs_local_copy_desc_t *d)
{
    if(!d||!d->rrw||!d->rrh||d->rrw>2048u||d->rrh>2048u||
       d->ssax>=2048u||d->ssay>=2048u||d->dsax>=2048u||d->dsay>=2048u||
       d->rrw>2048u-d->ssax||d->rrw>2048u-d->dsax||
       d->rrh>2048u-d->ssay||d->rrh>2048u-d->dsay||
       !d->sbw||!d->dbw||d->sbw>2048u||d->dbw>2048u||
       d->sbp>=GS_MEM_SIZE/256u||d->dbp>=GS_MEM_SIZE/256u)return 0;
    int sbits=local_psm_bits(d->spsm),dbits=local_psm_bits(d->dpsm);
    int sgroup=local_psm_group(d->spsm),dgroup=local_psm_group(d->dpsm);
    /* Matching transfer widths copy raw texel bits across swizzles;
     * different bit depths are an explicit, opt-in software extension. */
    if(!sbits||!dbits||sgroup<0||dgroup<0||
       (sbits!=dbits&&(!g_gs_local_extended_conversion||sgroup!=dgroup))) {
       g_gs_local_rejected++;return 0;
    }
    uint64_t slo,shi,dlo,dhi;
    if(!local_page_range(d->sbp,d->sbw,d->ssax,d->ssay,d->rrw,d->rrh,d->spsm,&slo,&shi)||
       !local_page_range(d->dbp,d->dbw,d->dsax,d->dsay,d->rrw,d->rrh,d->dpsm,&dlo,&dhi))return 0;
    /* Ensure pending GPU writes are visible before any reads. */
    if(!gs_mem_sync())return 0;
    int alias=(slo<dhi && dlo<shi);
    int same_layout=(d->spsm==d->dpsm&&d->sbp==d->dbp&&d->sbw==d->dbw);
    /* For a cross-layout physical alias, direction bits are NOT enough:
     * the same bytes may represent unrelated pixel coordinates. Snapshot
     * the entire source before writing the first destination pixel. */
    uint64_t count=(uint64_t)d->rrw*d->rrh;
    if(alias&&!same_layout) {
        if(count>GS_LOCAL_ALIAS_SNAPSHOT_PIXELS) {
            g_gs_local_rejected++;g_local_alias_declines++;return 0;
        }
        for(uint32_t y=0;y<d->rrh;y++)for(uint32_t x=0;x<d->rrw;x++)
            g_local_alias_snapshot[y*d->rrw+x]=local_load_pixel(
                 d->sbp,d->sbw,d->ssax+x,d->ssay+y,d->spsm);
        g_local_alias_snapshots++;g_local_alias_pixels+=count;
    }
    uint32_t dirx=d->dirx,diry=d->diry;
    if(same_layout&&
       d->ssax<d->dsax+d->rrw&&d->dsax<d->ssax+d->rrw&&
       d->ssay<d->dsay+d->rrh&&d->dsay<d->ssay+d->rrh){
       if(d->dsay>d->ssay)diry=1;else if(d->dsay<d->ssay)diry=0;
       if(d->dsax>d->ssax)dirx=1;else if(d->dsax<d->ssax)dirx=0;
    }
    int converting=(d->spsm!=d->dpsm);
    for(uint32_t iy=0;iy<d->rrh;iy++){
       uint32_t yy=diry?d->rrh-1u-iy:iy;
       for(uint32_t ix=0;ix<d->rrw;ix++){
          uint32_t xx=dirx?d->rrw-1u-ix:ix;
          uint32_t v=(alias&&!same_layout)?g_local_alias_snapshot[yy*d->rrw+xx]:
             local_load_pixel(d->sbp,d->sbw,d->ssax+xx,d->ssay+yy,d->spsm);
          if(sbits!=dbits)v=local_convert_bits(v,sbits,dbits,sgroup);
          local_store_pixel(d->dbp,d->dbw,d->dsax+xx,d->dsay+yy,d->dpsm,v);
       }
    }
    if(converting)g_gs_local_converted+=(uint64_t)d->rrw*d->rrh;
    return 1;
}

/* BETA12.12: GS Local-to-Host readback; native PSM packing in little-endian
 * host FIFO byte order, independent of Wii/PPC host endianness.  GPU writes
 * must be resolved before sampling eDRAM; no fake zero-filled success when
 * readback itself fails.  The final incomplete qword alone is zero-padded. */
uint32_t gs_mem_local_fifo_total_qwords(const gs_local_copy_desc_t *d)
{
    if(!d||!d->rrw||!d->rrh||d->rrw>2048u||d->rrh>2048u||
       !d->sbw||d->sbw>2048u||d->sbp>=GS_MEM_SIZE/256u||
       d->ssax>=2048u||d->ssay>=2048u||
       d->rrw>2048u-d->ssax||d->rrh>2048u-d->ssay)return 0;
    int bits=local_psm_bits(d->spsm);
    if(!bits)return 0;
    uint64_t lo,hi;
    if(!local_page_range(d->sbp,d->sbw,d->ssax,d->ssay,d->rrw,d->rrh,
                         d->spsm,&lo,&hi))return 0;
    uint64_t total_bits=(uint64_t)d->rrw*d->rrh*(uint32_t)bits;
    return (uint32_t)((total_bits+127u)/128u);
}
uint32_t gs_mem_read_local_qwords(const gs_local_copy_desc_t *d,
                                uint32_t start_qword,uint8_t *out,uint32_t qwc)
{
    if(!out||!qwc||qwc>128u)return 0;
    uint32_t total=gs_mem_local_fifo_total_qwords(d);
    if(!total||start_qword>=total||!gs_mem_sync())return 0;
    if(qwc>total-start_qword)qwc=total-start_qword;
    int bits=local_psm_bits(d->spsm);
    uint64_t count=(uint64_t)d->rrw*d->rrh;
    /* Each output byte consists of 2 indexed-4 pixels, or a native
     * byte of one 8/16/24/32-bit GS pixel. */
    for(uint32_t i=0;i<qwc*16u;i++){
        uint64_t pos=(uint64_t)start_qword*16u+i;
        uint64_t pixel=(pos*8u)/(uint32_t)bits;
        if(pixel>=count){out[i]=0;continue;}
        uint32_t x=(uint32_t)(pixel%d->rrw),y=(uint32_t)(pixel/d->rrw);
        if(d->dirx)x=d->rrw-1u-x;
        if(d->diry)y=d->rrh-1u-y;
        uint32_t v=local_load_pixel(d->sbp,d->sbw,d->ssax+x,d->ssay+y,d->spsm);
        if(bits==4){
            uint64_t next=pixel+1u;
            uint32_t hi=0;
            if(next<count){
                uint32_t nx=(uint32_t)(next%d->rrw),ny=(uint32_t)(next/d->rrw);
                if(d->dirx)nx=d->rrw-1u-nx;
                if(d->diry)ny=d->rrh-1u-ny;
                hi=local_load_pixel(d->sbp,d->sbw,d->ssax+nx,d->ssay+ny,d->spsm);
            }
            out[i]=(uint8_t)((v&15u)|((hi&15u)<<4));
        }else{
            unsigned shift=(unsigned)((pos % ((unsigned)bits/8u))*8u);
            out[i]=(uint8_t)(v>>shift);
        }
    }
    return qwc;
}
