/* GPL-3.0-or-later. See wiiu_menu.h. */
#include "wiiu_menu.h"
#include "frontend.h" /* static ui_* page drawing shared with the Wii launcher */
#include "core/hw/frontend_font_data.h"
#include "core/hw/frontend_logo_data.h"
#include "core/hw/frontend_settings.h"
#include "core/recompiler/optimization.h"
#include <stdlib.h>
#include <string.h>

/* Page numbers follow the Wii launcher (source/main.c): 0 main, 1 settings, 2 about, 3 disc browser,
 * 4 developer switches, 5 experiments, 6 network logger (not available here). */
enum { PG_MAIN = 0, PG_SETTINGS = 1, PG_ABOUT = 2, PG_BROWSER = 3, PG_DEV = 4, PG_EXP = 5 };

struct wiiu_menu {
    int page, selected, settings_selected, opt_selected, experiment_selected;
    frontend_browser browser;
    int browser_ready, last_disc;
    unsigned hud, throughput, fps, gx;
    char root[256], disc[512], disc_notice[80], notice[96], opt_path[300], settings_path[300], bios_name[64];
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

/* The launcher logo (256x96 RGBA, alpha in the low byte) blended into the RGB buffer. */
static void menu_logo(int x, int y)
{
    for (unsigned j = 0; j < FRONTEND_LOGO_HEIGHT; j++)
        for (unsigned i = 0; i < FRONTEND_LOGO_WIDTH; i++) {
            const uint32_t p = frontend_logo_pixels[j * FRONTEND_LOGO_WIDTH + i];
            const unsigned a = p & 255u;
            if (!a) continue;
            const int px = x + (int)i, py = y + (int)j;
            if (px < 0 || py < 0 || px >= (int)WM_W || py >= (int)WM_H) continue;
            uint32_t *d = &g_fb[(size_t)py * WM_W + (size_t)px], o = *d;
            const unsigned r = (((o >> 16) & 255u) * (255u - a) + ((p >> 24) & 255u) * a + 127u) / 255u;
            const unsigned g = (((o >> 8) & 255u) * (255u - a) + ((p >> 16) & 255u) * a + 127u) / 255u;
            const unsigned b = ((o & 255u) * (255u - a) + ((p >> 8) & 255u) * a + 127u) / 255u;
            *d = (r << 16) | (g << 8) | b;
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

static int save_all(wiiu_menu *m)
{
    frontend_settings st = { m->hud, m->throughput, m->fps, m->gx };
    int rc = 0;
    if (m->opt_path[0] && gekko2_opt_save(m->opt_path)) rc = -1;
    if (m->settings_path[0] && frontend_settings_save(&st, m->settings_path)) rc = -1;
    return rc;
}

static void options_defaults(void)
{
    gekko2_opt_reset_requested();
    /* Software TLB translation cache: measured faster with an identical picture. On by default, switchable. */
    if (!(gekko2_opt_requested() & GEKKO2_OPT_BIT(GEKKO2_OPT_FASTMEM))) gekko2_opt_toggle(GEKKO2_OPT_FASTMEM);
}

wiiu_menu *wiiu_menu_create(const char *sd_root, const char *bios_name)
{
    wiiu_menu *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    snprintf(m->root, sizeof m->root, "%s", sd_root ? sd_root : "");
    snprintf(m->bios_name, sizeof m->bios_name, "%s", bios_name ? bios_name : "ps2bios.bin");
    snprintf(m->opt_path, sizeof m->opt_path, "%s/wiiu/gekko2/optimization.cfg", m->root);
    snprintf(m->settings_path, sizeof m->settings_path, "%s/wiiu/gekko2/settings.cfg", m->root);
    frontend_settings st;
    frontend_settings_defaults(&st);
    frontend_settings_load(&st, m->settings_path);
    m->hud = st.hud; m->throughput = st.throughput; m->fps = st.fps; m->gx = 0; /* no GX on the Wii U */
    options_defaults();
    gekko2_opt_load(m->opt_path); /* keeps the defaults when there is no saved file */
    snprintf(m->notice, sizeof m->notice, "SD: wiiu/gekko2/bios/ and games/");
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

void wiiu_menu_render(wiiu_menu *m, uint32_t *xrgb)
{
    g_fb = xrgb;
    ui_text_renderer = menu_text;
    ui_logo_renderer = menu_logo;
    switch (m->page) {
    case PG_DEV: ui_optimization_draw(menu_rect, m->opt_selected, gekko2_opt_requested(), wiiu_available(), m->notice); break;
    case PG_EXP: ui_experiments_draw(menu_rect, m->experiment_selected, m->notice); break;
    case PG_SETTINGS: ui_settings_draw(menu_rect, m->settings_selected, (int)m->hud, (int)m->throughput, (int)m->fps, (int)m->gx, m->notice); break;
    case PG_BROWSER: ui_browser_draw(menu_rect, &m->browser, m->notice); break;
    default: ui_draw(menu_rect, m->selected, m->page, 0, (int)m->hud, (int)m->throughput, (int)m->fps, (int)m->gx, gekko2_opt_requested(), m->notice); break;
    }
    g_fb = NULL;
}

static int wrap(int v, int delta, int count) { return (v + delta + count) % count; }

static void experiment_toggle(wiiu_menu *m, unsigned n)
{
    if (!(gekko2_experimental_available() & (1u << n))) { wiiu_menu_set_notice(m, "Not available in this build."); return; }
    gekko2_experimental_request(n, !gekko2_experimental_requested(n));
    wiiu_menu_set_notice(m, "Applied at the next cold boot.");
}

static int settings_change(wiiu_menu *m)
{
    int rc = 0;
    switch (m->settings_selected) {
    case UI_SET_RENDERER: wiiu_menu_set_notice(m, "GX is not available on the Wii U port (software video only)."); return 0;
    case UI_SET_SOUND: wiiu_menu_set_notice(m, "Sound output is not available on the Wii U port yet."); return 0;
    case UI_SET_HUD: m->hud = !m->hud; break;
    case UI_SET_UPDATE: m->throughput = !m->throughput; break;
    case UI_SET_FPS: m->fps = !m->fps; break;
    case UI_SET_PROFILE:
        gekko2_opt_request_profile(!strcmp(gekko2_opt_profile_name(), "WII BASE") ? GEKKO2_PROFILE_INTERPRETER : GEKKO2_PROFILE_BASE);
        break;
    case UI_SET_EXPERIMENTS: m->page = PG_EXP; return 0;
    case UI_SET_DEVELOPER: m->page = PG_DEV; return 0;
    case UI_SET_NETWORK: wiiu_menu_set_notice(m, "The network logger is not available on the Wii U port."); return 0;
    case UI_SET_DEFAULTS: {
        frontend_settings d;
        frontend_settings_defaults(&d);
        m->hud = d.hud; m->throughput = d.throughput; m->fps = d.fps; m->gx = 0;
        options_defaults();
        for (unsigned n = 0; n < GEKKO2_EXP_COUNT; n++) gekko2_experimental_request(n, 0);
        break;
    }
    }
    rc = save_all(m);
    wiiu_menu_set_notice(m, rc ? "Some settings were not saved. Check the SD card." : "Saved. Applies at the next start.");
    return 0;
}

int wiiu_menu_input(wiiu_menu *m, unsigned edges)
{
    if (!m || !edges) return WM_ACTION_NONE;
    if (edges & WM_START) { /* the Wii launcher resumes a paused session here; this port has none */
        wiiu_menu_set_notice(m, "No paused session. Choose BIOS or DISC.");
        return WM_ACTION_NONE;
    }
    switch (m->page) {
    case PG_BROWSER:
        if (edges & WM_B) {
            if (!strcmp(m->browser.path, m->browser.root)) { m->page = PG_MAIN; wiiu_menu_set_notice(m, m->disc[0] ? m->disc_notice : "No disc selected."); }
            else if (frontend_browser_up(&m->browser) < 0) wiiu_menu_set_notice(m, "Could not open parent folder.");
        } else if ((edges & WM_PAGE_UP) && m->browser.count) m->browser.selected = m->browser.selected >= 8 ? m->browser.selected - 8 : 0;
        else if ((edges & WM_PAGE_DOWN) && m->browser.count) m->browser.selected = m->browser.selected + 8 < m->browser.count ? m->browser.selected + 8 : m->browser.count - 1;
        else if ((edges & WM_UP) && m->browser.count) m->browser.selected = (m->browser.selected + m->browser.count - 1) % m->browser.count;
        else if ((edges & WM_DOWN) && m->browser.count) m->browser.selected = (m->browser.selected + 1) % m->browser.count;
        else if (edges & WM_A) {
            char path[512];
            int rc = frontend_browser_activate(&m->browser, path, sizeof path);
            if (rc == 1) {
                const char *name = strrchr(path, '/');
                snprintf(m->disc, sizeof m->disc, "%s", path);
                snprintf(m->disc_notice, sizeof m->disc_notice, "DISC: %.62s", name ? name + 1 : path);
                wiiu_menu_set_notice(m, m->disc_notice);
                m->page = PG_MAIN;
                m->selected = UI_DISC;
            } else wiiu_menu_set_notice(m, rc < 0 ? "Could not open selection." : "Choose an ISO / BIN / CHD file.");
        }
        return WM_ACTION_NONE;
    case PG_DEV:
        if (edges & WM_B) m->page = PG_SETTINGS;
        else if (edges & WM_UP) m->opt_selected = wrap(m->opt_selected, -1, GEKKO2_OPT_COUNT);
        else if (edges & WM_DOWN) m->opt_selected = wrap(m->opt_selected, 1, GEKKO2_OPT_COUNT);
        else if (edges & (WM_A | WM_LEFT | WM_RIGHT)) {
            if (!(wiiu_available() & GEKKO2_OPT_BIT((unsigned)m->opt_selected))) wiiu_menu_set_notice(m, "Not available on this build.");
            else {
                gekko2_opt_toggle((unsigned)m->opt_selected);
                wiiu_menu_set_notice(m, save_all(m) ? "Not saved. Check the SD card." : "Saved. Applies at the next start.");
            }
        }
        return WM_ACTION_NONE;
    case PG_EXP:
        if (edges & WM_B) m->page = PG_SETTINGS;
        else if (edges & WM_UP) m->experiment_selected = wrap(m->experiment_selected, -1, GEKKO2_EXP_COUNT);
        else if (edges & WM_DOWN) m->experiment_selected = wrap(m->experiment_selected, 1, GEKKO2_EXP_COUNT);
        else if (edges & (WM_A | WM_LEFT | WM_RIGHT)) experiment_toggle(m, (unsigned)m->experiment_selected);
        return WM_ACTION_NONE;
    case PG_SETTINGS:
        if (edges & WM_B) m->page = PG_MAIN;
        else if (edges & WM_UP) m->settings_selected = wrap(m->settings_selected, -1, UI_SET_COUNT);
        else if (edges & WM_DOWN) m->settings_selected = wrap(m->settings_selected, 1, UI_SET_COUNT);
        else if (edges & (WM_A | WM_LEFT | WM_RIGHT)) settings_change(m);
        return WM_ACTION_NONE;
    case PG_ABOUT:
        if (edges & (WM_B | WM_A)) m->page = PG_MAIN;
        return WM_ACTION_NONE;
    default: break;
    }
    /* main page */
    if (edges & (WM_UP | WM_LEFT)) m->selected = wrap(m->selected, -1, UI_MENU_COUNT);
    else if (edges & (WM_DOWN | WM_RIGHT)) m->selected = wrap(m->selected, 1, UI_MENU_COUNT);
    else if (edges & WM_A) {
        if (m->selected == UI_EXIT) return WM_ACTION_EXIT;
        if (m->selected == UI_SETTINGS || m->selected == UI_ABOUT) { m->page = m->selected == UI_SETTINGS ? PG_SETTINGS : PG_ABOUT; return WM_ACTION_NONE; }
        const int disc = m->selected == UI_COLD_BOOT ? m->last_disc : m->selected == UI_DISC;
        if (m->selected == UI_SELECT_DISC || (disc && !m->disc[0])) {
            if (open_browser(m) < 0) wiiu_menu_set_notice(m, "Could not open SD card.");
            else { m->page = PG_BROWSER; wiiu_menu_set_notice(m, "Choose an ISO / BIN / CHD file."); }
            return WM_ACTION_NONE;
        }
        m->last_disc = disc;
        gekko2_opt_apply();
        return disc ? WM_ACTION_START_DISC : WM_ACTION_START_BIOS;
    }
    return WM_ACTION_NONE;
}
