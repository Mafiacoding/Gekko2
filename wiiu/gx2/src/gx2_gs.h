/* GPL-3.0-or-later. Gekko2 Wii U: GX2 drawing back end for GS primitives (step 2 of the hardware acceleration).
 * The GPU rasterises triangles/sprites into an off-screen linear RGBA8 colour buffer; the CPU reads the result
 * back (GS VRAM write-back) or hands it to the presenter. Needs WHBGfxInit() first (gx2_present_init does it).
 * Milestone 1: colour + texture MODULATE/DECAL-like shading and the common GS blend equations, no depth yet.
 * Not verified on Cemu or a real Wii U by the author; the GX2 probe's test C checks pixels and logs them. */
#ifndef GEKKO2_GX2_GS_H
#define GEKKO2_GX2_GS_H
#include <stdint.h>

typedef struct gx2_gs_vertex {
    float pos[4]; /* clip space x, y, z, w */
    float col[4]; /* r, g, b, a with GS 0x80 = 1.0 */
    float uv[4];  /* u, v normalised to the uploaded texture, 0, 1 */
} gx2_gs_vertex;

enum {
    GX2_GS_BLEND_OFF = 0,
    GX2_GS_BLEND_MIX_AS,   /* (Cs - Cd) * As + Cd */
    GX2_GS_BLEND_ADD_AS,   /* Cs * As + Cd */
    GX2_GS_BLEND_ADD_FIX,  /* Cs * FIX + Cd */
    GX2_GS_BLEND_SUB_AS,   /* Cd - Cs * As */
    GX2_GS_BLEND_MIX_FIX,  /* (Cs - Cd) * FIX + Cd */
    GX2_GS_BLEND_ADD,      /* Cs + Cd */
};
enum { GX2_GS_TRIANGLES = 0, GX2_GS_STRIP = 1, GX2_GS_FAN = 2, GX2_GS_QUADS = 3 };

/* gsh_color / gsh_modulate: assembled gs.vsh + gs_color.psh / gs_modulate.psh. Returns 1 on success. */
int gx2_gs_init(const void *gsh_color, const void *gsh_modulate, unsigned width, unsigned height);
void gx2_gs_shutdown(void);
int gx2_gs_ready(void);
/* Start drawing into the off-screen target; clear when clear != 0 (0xAARRGGBB, GS-style alpha 0x80 = 1.0). */
void gx2_gs_begin(int clear, uint32_t argb);
/* xrgb: 0x00RRGGBB or with alpha 0xAARRGGBB (alpha used when has_alpha). Max 1024x1024. */
int gx2_gs_set_texture(const uint32_t *argb, unsigned w, unsigned h, int has_alpha, int linear, int wrap);
void gx2_gs_set_blend(int mode, uint32_t fix);
int gx2_gs_draw(const gx2_gs_vertex *v, unsigned count, int prim, int textured);
/* Finish all work and return the target pixels (big-endian words 0xRRGGBBAA, alpha 1.0 = 0xff), row pitch in pixels. */
const uint32_t *gx2_gs_end(unsigned *pitch);
/* 0 draws, 1 vertices, 2 texture uploads, 3 waits for the GPU, 4 vertex buffer overflows. */
uint64_t gx2_gs_stat(unsigned index);
#endif
