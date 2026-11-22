/* GPL-3.0-or-later. See wiiu_menu.h. */
#include "wiiu_menu.h"
#include "frontend.h" /* static ui_* page drawing shared with the Wii launcher */
#include "core/hw/frontend_font_data.h"
#include "core/recompiler/optimization.h"
#include <stdlib.h>
#include <string.h>

typedef enum { PG_MAIN, PG_BROWSER, PG_OPTIONS, PG_ABOUT } page_t;
enum { MI_BIOS, MI_DISC, MI_SELECT, MI_OPTIONS, MI_ABOUT, MI_EXIT, MI_COUNT };

struct wiiu_menu {
    page_t page;
    int selected, opt_selected;
    frontend_browser browser;
    int browser_ready;
    char root[256], disc[512], bios_name[64], notice[96];
};

static uint32_t *g_fb;
static void menu_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_fb) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)WM_W) w = (int)WM_W - x;
    if (y + h > (int)WM_H) h = (int)WM_H - y;
    const uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            g_fb[(size_t)(y + j) * WM_W + (size_t)(x + i)] = c;
}

/* Antialiased launcher font (same data as the Wii launcher): scale 1 = 8x14 cells, scale 3 = 24x42 cells.
 * Other scales fall back to the built-in bitmap font through rect(). */
static void blend(int x, int y, unsigned a, uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_fb || !a || x < 0 || y < 0 || x >= (int)WM_W || y >= (int)WM_H) return;
    uint32_t *p = &g_fb[(size_t)y * WM_W + (size_t)x], o = *p;
    unsigned or_ = (o >> 16) & 255, og = (o >> 8) & 255, ob = o & 255;
    or_ = (or_ * (15 - a) + r * a + 7) / 15;
    og = (og * (15 - a) + g * a + 7) / 15;
    ob = (ob * (15 - a) + b * a + 7) / 15;
    *p = (or_ << 16) | (og << 8) | ob;
}
static void menu_text(int x, int y, int scale, const char *s, int r, int g, int b)
{
    if (scale != 1 && scale != 3) {
        for (; *s; s++, x += 8 * scale) {
            unsigned c = (unsigned char)*s;
            if (c < 32 || c > 126) continue;
            for (int row = 0; row < 14; row++)
                for (int col = 0; col < 8; col++)
                    if (ui_font[c - 32][row] & (1 << col)) menu_rect(x + col * scale, y + row * scale, scale, scale, (uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
        return;
    }
    const unsigned cw = 8u * (unsigned)scale, ch = 14u * (unsigned)scale;
    for (; *s; s++, x += (int)cw) {
        unsigned c = (unsigned char)*s;
        if (c < 32 || c > 126) continue;
        for (unsigned py = 0; py < ch; py++)
            for (unsigned px = 0; px < cw; px++) {
                unsigned p = py * cw + px;
                uint8_t v = scale == 3 ? frontend_font_1[c - 32][p / 2] : frontend_font_0[c - 32][p / 2];
                blend(x + (int)px, y + (int)py, (p & 1) ? (v & 15u) : (v >> 4), (uint8_t)r, (uint8_t)g, (uint8_t)b);
            }
    }
}

/* Developer switches that mean something on the Wii U test port. The rest are shown as UNAVAILABLE. */
static uint32_t wiiu_available(void)
{
    const uint32_t not_here = GEKKO2_OPT_BIT(GEKKO2_OPT_GX_RESIDENT) | GEKKO2_OPT_BIT(GEKKO2_OPT_GX_SOURCE_CACHE) |
        GEKKO2_OPT_BIT(GEKKO2_OPT_GX_DISJOINT_WRITES) | GEKKO2_OPT_BIT(GEKKO2_OPT_GX_GOURAUD) |
        GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_WORKER) | GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_IOS222) | GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_CPU_RAM) |
        GEKKO2_OPT_BIT(GEKKO2_OPT_SOUND) | GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT) |
        GEKKO2_OPT_BIT(GEKKO2_OPT_WORD_ALLOCATION);
    return gekko2_opt_available() & ~not_here; /* Wii hardware features and the IOP/VU dynarecs are not wired up here */
}

wiiu_menu *wiiu_menu_create(const char *sd_root, const char *bios_name)
{
    wiiu_menu *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    snprintf(m->root, sizeof m->root, "%s", sd_root ? sd_root : "");
    snprintf(m->bios_name, sizeof m->bios_name, "%s", bios_name ? bios_name : "ps2bios.bin");
    gekko2_opt_reset_requested();
    /* Software TLB translation cache: measured faster, identical picture. On by default, switchable. */
    if (!(gekko2_opt_requested() & GEKKO2_OPT_BIT(GEKKO2_OPT_FASTMEM)))
        gekko2_opt_toggle(GEKKO2_OPT_FASTMEM);
    return m;
}

void wiiu_menu_destroy(wiiu_menu *m)
{
    if (!m) return;
    if (m->browser_ready) frontend_browser_release(&m->browser);
    free(m);
}

const char *wiiu_menu_disc(const wiiu_menu *m) { return m && m->disc[0] ? m->disc : NULL; }
void wiiu_menu_set_notice(wiiu_menu *m, const char *t) { if (m) snprintf(m->notice, sizeof m->notice, "%s", t ? t : ""); }

static int open_browser(wiiu_menu *m)
{
    char start[300];
    if (m->browser_ready) frontend_browser_release(&m->browser);
    m->browser_ready = 0;
    snprintf(start, sizeof start, "%s/wiiu/gekko2/games/", m->root);
    if (frontend_browser_init(&m->browser, m->root, start) < 0) return -1;
    m->browser_ready = 1;
    return 0;
}

static const char *base_name(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void draw_main(wiiu_menu *m)
{
    for (int y = 0; y < 480; y++) menu_rect(0, y, 640, 1, 4 + y / 120, 9 + y / 80, 22 + y / 25);
    for (int i = 0; i < 7; i++) { int x = 408 + i * 28; menu_rect(x, 76 + i * 11, 2, 240 - i * 17, 12, 35 + i * 5, 66 + i * 9); }
    menu_rect(36, 35, 4, 51, 67, 182, 255);
    ui_text(menu_rect, 54, 32, 3, "GEKKO2", 204, 232, 255);
    ui_text(menu_rect, 56, 84, 1, "WII U TEST PORT  /  " GEKKO2_RELEASE, 78, 163, 229);
    menu_rect(36, 110, 568, 2, 24, 61, 99);
    ui_text(menu_rect, 38, 129, 1, "PLAYSTATION 2 EMULATION", 100, 166, 215);
    const char *labels[MI_COUNT] = { "START BIOS", "START DISC", "SELECT DISC", "DEVELOPER SWITCHES", "ABOUT", "EXIT" };
    const char *desc[MI_COUNT] = { "Boot the PS2 BIOS (OSDSYS menu)", "Boot the selected ISO / BIN / CHD", "Browse the SD card for a disc image",
                                   "CPU options; apply at the next start", "Build and controls", "Back to the Wii U menu" };
    for (int i = 0; i < MI_COUNT; i++) {
        int y = 152 + i * 32, on = i == m->selected;
        const int dim = (i == MI_DISC && !m->disc[0]);
        menu_rect(36, y, 370, 31, on ? 16 : 9, on ? 43 : 22, on ? 76 : 41);
        menu_rect(36, y, 3, 31, on ? 65 : 20, on ? 183 : 49, on ? 255 : 76);
        ui_text(menu_rect, 51, y + 3, 1, labels[i], dim ? 90 : on ? 228 : 148, dim ? 100 : on ? 242 : 176, dim ? 120 : on ? 255 : 207);
        ui_text(menu_rect, 51, y + 17, 1, desc[i], on ? 115 : 75, on ? 188 : 116, on ? 235 : 157);
    }
    char line[100];
    ui_text(menu_rect, 433, 173, 1, "CPU NEXT START", 91, 144, 188);
    const uint32_t req = gekko2_opt_requested() & gekko2_opt_available();
    const uint32_t jit = GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT) | GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
    ui_text(menu_rect, 433, 194, 1, (req & jit) == jit ? "EE DYNAREC" : "INTERPRETER", 185, 221, 250);
    ui_text(menu_rect, 433, 240, 1, "BIOS", 91, 144, 188);
    snprintf(line, sizeof line, "%.24s", m->bios_name);
    ui_text(menu_rect, 433, 261, 1, line, 185, 221, 250);
    ui_text(menu_rect, 433, 297, 1, "DISC", 91, 144, 188);
    snprintf(line, sizeof line, "%.24s", m->disc[0] ? base_name(m->disc) : "(none)");
    ui_text(menu_rect, 433, 318, 1, line, 185, 221, 250);
    menu_rect(36, 381, 568, 1, 24, 61, 99);
    ui_text(menu_rect, 38, 397, 1, m->notice, 117, 186, 237);
    ui_text(menu_rect, 38, 437, 1, "UP/DOWN SELECT  A OPEN  + START BIOS  HOME EXIT", 209, 227, 246);
}

static void draw_about(void)
{
    for (int y = 0; y < 480; y++) menu_rect(0, y, 640, 1, 4 + y / 120, 9 + y / 80, 22 + y / 25);
    ui_text(menu_rect, 36, 28, 2, "ABOUT", 204, 232, 255);
    ui_text(menu_rect, 48, 90, 1, "Experimental PS2 emulator, Wii U test port.", 221, 237, 255);
    ui_text(menu_rect, 48, 126, 1, "Interpreter, software video through OSScreen.", 106, 171, 223);
    ui_text(menu_rect, 48, 150, 1, "BIOS: sd:/wiiu/gekko2/bios/ps2bios.bin (your dump).", 106, 171, 223);
    ui_text(menu_rect, 48, 174, 1, "Discs: sd:/wiiu/gekko2/games/  (.iso .bin .chd)", 106, 171, 223);
    ui_text(menu_rect, 48, 216, 1, "Pad: B Cross, A Circle, Y Square, X Triangle,", 106, 171, 223);
    ui_text(menu_rect, 48, 240, 1, "L/R = L1/R1, ZL/ZR = L2/R2, + Start, - Select.", 106, 171, 223);
    ui_text(menu_rect, 48, 282, 1, "Back to this menu: hold L+R+ZL+ZR and press +.", 106, 171, 223);
    ui_text(menu_rect, 48, 320, 1, "Not a release. No speed or compatibility claim.", 117, 186, 237);
    ui_text(menu_rect, 36, 437, 1, "B BACK", 209, 227, 246);
}

void wiiu_menu_render(wiiu_menu *m, uint32_t *xrgb)
{
    g_fb = xrgb;
    ui_text_renderer = menu_text;
    ui_logo_renderer = NULL;
    switch (m->page) {
    case PG_MAIN: draw_main(m); break;
    case PG_BROWSER: ui_browser_draw(menu_rect, &m->browser, m->notice); break;
    case PG_OPTIONS: ui_optimization_draw(menu_rect, m->opt_selected, gekko2_opt_requested(), wiiu_available(), m->notice); break;
    case PG_ABOUT: draw_about(); break;
    }
    g_fb = NULL;
}

static int step(int v, unsigned edges, int count)
{
    if (edges & WM_UP) v = v > 0 ? v - 1 : count - 1;
    if (edges & WM_DOWN) v = v + 1 < count ? v + 1 : 0;
    return v;
}

int wiiu_menu_input(wiiu_menu *m, unsigned edges)
{
    if (!m || !edges) return WM_ACTION_NONE;
    switch (m->page) {
    case PG_MAIN:
        if (edges & WM_START) { gekko2_opt_apply(); return WM_ACTION_START_BIOS; }
        m->selected = step(m->selected, edges, MI_COUNT);
        if (edges & WM_A) {
            m->notice[0] = 0;
            switch (m->selected) {
            case MI_BIOS: gekko2_opt_apply(); return WM_ACTION_START_BIOS;
            case MI_DISC:
                if (!m->disc[0]) { wiiu_menu_set_notice(m, "No disc selected: use SELECT DISC first."); break; }
                gekko2_opt_apply();
                return WM_ACTION_START_DISC;
            case MI_SELECT:
                if (open_browser(m) < 0) wiiu_menu_set_notice(m, "Could not open the SD card folder.");
                else m->page = PG_BROWSER;
                break;
            case MI_OPTIONS: m->page = PG_OPTIONS; break;
            case MI_ABOUT: m->page = PG_ABOUT; break;
            case MI_EXIT: return WM_ACTION_EXIT;
            }
        }
        break;
    case PG_BROWSER:
        if (edges & WM_B) {
            if (!strcmp(m->browser.path, m->browser.root)) { m->page = PG_MAIN; m->notice[0] = 0; }
            else if (frontend_browser_up(&m->browser) < 0) wiiu_menu_set_notice(m, "Could not open the parent folder.");
            break;
        }
        if (m->browser.count) {
            if (edges & WM_UP) m->browser.selected = m->browser.selected > 0 ? m->browser.selected - 1 : m->browser.count - 1;
            if (edges & WM_DOWN) m->browser.selected = m->browser.selected + 1 < m->browser.count ? m->browser.selected + 1 : 0;
            if (edges & WM_PAGE_UP) m->browser.selected = m->browser.selected > 8 ? m->browser.selected - 8 : 0;
            if (edges & WM_PAGE_DOWN) m->browser.selected = m->browser.selected + 8 < m->browser.count ? m->browser.selected + 8 : m->browser.count - 1;
        }
        if (edges & WM_A) {
            char path[512];
            int rc = frontend_browser_activate(&m->browser, path, sizeof path);
            if (rc == 1) {
                snprintf(m->disc, sizeof m->disc, "%s", path);
                m->page = PG_MAIN;
                m->selected = MI_DISC;
                wiiu_menu_set_notice(m, "Disc selected. START DISC to boot it.");
            } else if (rc < 0) wiiu_menu_set_notice(m, "Could not open that entry.");
        }
        break;
    case PG_OPTIONS: {
        if (edges & WM_B) { m->page = PG_MAIN; m->notice[0] = 0; break; }
        m->opt_selected = step(m->opt_selected, edges, GEKKO2_OPT_COUNT);
        if (edges & (WM_A | WM_LEFT | WM_RIGHT)) {
            if (!(wiiu_available() & GEKKO2_OPT_BIT((unsigned)m->opt_selected))) wiiu_menu_set_notice(m, "Not available on this build.");
            else { gekko2_opt_toggle((unsigned)m->opt_selected); m->notice[0] = 0; }
        }
        break;
    }
    case PG_ABOUT:
        if (edges & (WM_B | WM_A)) m->page = PG_MAIN;
        break;
    }
    return WM_ACTION_NONE;
}
