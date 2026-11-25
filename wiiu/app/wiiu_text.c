/* GPL-3.0-or-later. See wiiu_text.h. */
#include "wiiu_text.h"
#include "core/hw/frontend_font_data.h"

void wiiu_text_draw(uint32_t *fb, unsigned w, unsigned h, int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b)
{
    for (; *s; s++, x += 8) {
        const unsigned c = (unsigned char)*s;
        if (c < 32 || c > 126) continue;
        for (unsigned py = 0; py < 14; py++)
            for (unsigned px = 0; px < 8; px++) {
                const unsigned p = py * 8 + px;
                const uint8_t v = frontend_font_0[c - 32][p / 2];
                const unsigned a = (p & 1) ? (v & 15u) : (v >> 4);
                const int dx = x + (int)px, dy = y + (int)py;
                if (!a || dx < 0 || dy < 0 || dx >= (int)w || dy >= (int)h) continue;
                uint32_t *d = &fb[(size_t)dy * w + (size_t)dx], o = *d;
                const unsigned rr = (((o >> 16) & 255u) * (15 - a) + r * a + 7) / 15;
                const unsigned gg = (((o >> 8) & 255u) * (15 - a) + g * a + 7) / 15;
                const unsigned bb = ((o & 255u) * (15 - a) + b * a + 7) / 15;
                *d = (rr << 16) | (gg << 8) | bb;
            }
    }
}

void wiiu_text_dim_rows(uint32_t *fb, unsigned w, unsigned y0, unsigned y1)
{
    for (unsigned y = y0; y < y1; y++)
        for (unsigned x = 0; x < w; x++) {
            uint32_t *d = &fb[(size_t)y * w + x];
            *d = (*d >> 2) & 0x003f3f3fu; /* quarter brightness */
        }
}

void wiiu_scale_nearest(uint32_t *dst, unsigned dw, unsigned dh, const uint32_t *src, unsigned sw, unsigned sh)
{
    for (unsigned y = 0; y < dh; y++) {
        const uint32_t *row = &src[(size_t)((unsigned long)y * sh / dh) * sw];
        for (unsigned x = 0; x < dw; x++)
            dst[(size_t)y * dw + x] = row[(unsigned long)x * sw / dw];
    }
}
