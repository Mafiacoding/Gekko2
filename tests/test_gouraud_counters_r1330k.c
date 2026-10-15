/* test_gouraud_counters_r1330k.c (derived from test_gif_gouraud.c) - Gouraud-shading
 * support on TRIANGLE primitives (GS round: task #78), driven by
 * PRIM's real IIP bit (bit 3, mask 0x8) - confirmed against PCSX2's
 * own GS/GSRegs.h GIFRegPRIM bitfield layout (see docs/STATUS.md).
 *
 * Triangle used throughout: (0,0)-(60,0)-(0,60), a right triangle with
 * area 3600. For any interior point (x,y), the barycentric weights
 * work out to a clean closed form:
 *   b0 = 1 - (x+y)/60   (vertex0 weight, dominant near (0,0))
 *   b1 = x/60           (vertex1 weight, dominant near (60,0))
 *   b2 = y/60           (vertex2 weight, dominant near (0,60))
 * so sample points near each vertex should read back close to that
 * vertex's color, and the centroid should read back an equal blend of
 * all three (since each color channel is nonzero in only one vertex's
 * color below).
 */
#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static void wle32(uint8_t *p, uint32_t v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF; }

static void append_ad(uint8_t *buf, int *off, uint32_t data_lo, uint32_t data_hi, uint32_t addr)
{
    if (addr == GS_REG_FRAME_1 || addr == GS_REG_FRAME_2) data_lo = (data_lo & 0x1ffu) | (((data_lo >> 9) & 0x3fu) << 16);
    /* Encode fixture pixel coordinates into the real 64-bit XYZ register. */
    if (addr == GS_REG_XYZ2 || addr == GS_REG_XYZ3 || addr == GS_REG_XYZF2 || addr == GS_REG_XYZF3) {
        data_lo = (data_lo & 0xffffu) | ((data_hi & 0xffffu) << 16); data_hi = 0u;
    }
    wle32(buf + *off, data_lo);
    wle32(buf + *off + 4, data_hi);
    wle32(buf + *off + 8, addr);
    wle32(buf + *off + 12, 0);
    *off += 16;
}

/* Builds and processes one A+D GIF packet: FRAME_1(fbp=0,fbw=640) +
 * XYOFFSET_1(0,0) + PRIM(prim_type) + per-vertex (RGBAQ(colors[i]) +
 * XYZ2(verts[i])) - i.e., each vertex can carry its own color, unlike
 * test_gif_triangle.c's single shared-color helper. */
static void run_packet_colored(uint32_t prim_type, const uint32_t *colors, const int32_t *verts, int n_verts)
{
    uint8_t buf[16 * (4 + 2 * 8)];
    memset(buf, 0, sizeof(buf));
    int off = 0;
    int nloop = 4 + 2 * n_verts;

    wle32(buf + off,     (uint32_t)nloop | (1u << 15));
    wle32(buf + off + 4, (0u << 26) | (1u << 28));
    wle32(buf + off + 8, GIF_REG_AD);
    wle32(buf + off + 12, 0);
    off += 16;

    append_ad(buf, &off, (10u << 9), 0, GS_REG_FRAME_1);
    append_ad(buf, &off, 0, 0, GS_REG_XYOFFSET_1);
    append_ad(buf, &off, 1u, 0, GS_REG_PRMODECONT); /* AC=1: attributes from PRIM */
    append_ad(buf, &off, prim_type, 0, GS_REG_PRIM);
    for (int i = 0; i < n_verts; i++) {
        uint32_t rgba = colors[i];
        uint32_t rgbaq_lo = (rgba & 0xFFu) | (((rgba >> 8) & 0xFFu) << 8) | (((rgba >> 16) & 0xFFu) << 16) | (((rgba >> 24) & 0xFFu) << 24);
        append_ad(buf, &off, rgbaq_lo, 0, GS_REG_RGBAQ);
        int32_t px = verts[i*2], py = verts[i*2+1];
        append_ad(buf, &off, (uint32_t)(px << 4), (uint32_t)(py << 4), GS_REG_XYZ2);
    }

    gif_process_quadwords(DMA_CHANNEL_GIF, buf, (uint32_t)(off / 16));
}


int main(void) {
    uint32_t c[3]={0xFF0000FFu,0xFF00FF00u,0xFFFF0000u};
    int32_t good[6]={0,0,60,0,0,60};
    int32_t degen[6]={5,5,5,5,5,5};      /* identical3 */
    int32_t collin[6]={0,0,10,10,20,20}; /* collinear */
    run_packet_colored(PRIM_TYPE_TRIANGLE|PRIM_IIP_MASK,c,good,3);   /* TRIANGLE | IIP */
    run_packet_colored(PRIM_TYPE_TRIANGLE|PRIM_IIP_MASK,c,degen,3);
    run_packet_colored(PRIM_TYPE_TRIANGLE|PRIM_IIP_MASK,c,collin,3);
    printf("prim=%x ac=%d flatsub=%llu drawn=%llu route9=%llu\n",(unsigned)g_gif.prim,(int)g_gif.prmodecont_ac,(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_FLAT_SUBMITTED),(unsigned long long)gif_get_state()->triangles_drawn,(unsigned long long)gif_get_render_work(9));
    uint64_t sub=gif_get_gouraud_stat(GIF_GOURAUD_SUBMITTED);
    uint64_t deg=gif_get_gouraud_stat(GIF_GOURAUD_DEGENERATE);
    uint64_t off=gif_get_gouraud_stat(GIF_GOURAUD_OFFSCREEN);
    uint64_t vis=gif_get_gouraud_stat(GIF_GOURAUD_VISIBLE);
    uint64_t gx=gif_get_gouraud_stat(GIF_GOURAUD_GX);
    uint64_t sw=gif_get_gouraud_stat(GIF_GOURAUD_SOFTWARE);
    printf("sub=%llu deg=%llu off=%llu vis=%llu gx=%llu sw=%llu\n",(unsigned long long)sub,(unsigned long long)deg,(unsigned long long)off,(unsigned long long)vis,(unsigned long long)gx,(unsigned long long)sw);
    CHECK(sub==3,"3 Gouraud triangles submitted");
    CHECK(deg==2,"2 degenerate");
    CHECK(gif_get_gouraud_stat(GIF_GOURAUD_DEG_IDENTICAL3)==1,"1 identical3");
    CHECK(gif_get_gouraud_stat(GIF_GOURAUD_DEG_COLLINEAR)==1,"1 collinear");
    CHECK(sub==deg+off+vis,"submitted == degenerate+offscreen+visible");
    CHECK(vis==gx+sw,"visible == gx+software");
    uint64_t fb=0;for(unsigned i=0;i<GIF_GFB_COUNT;i++)fb+=gif_get_gouraud_fallback(i);
    CHECK(fb==sw,"sum(fallback reasons) == software");
    CHECK(gif_get_gouraud_fallback(GIF_GFB_GX_INACTIVE)==sw,"host: reason is gx_inactive");
    CHECK(gif_get_state()->triangles_drawn==1,"only the non-degenerate triangle counts as drawn");
    printf("%d failures\n",failures);
    return failures?1:0;
}
