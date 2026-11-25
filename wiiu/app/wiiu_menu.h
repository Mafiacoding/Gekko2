/* GPL-3.0-or-later. Gekko2 launcher menu for the Wii U test port. Platform neutral: it draws into a 640x480
 * 0x00RRGGBB buffer and takes button edges, so it is testable on the host. It reuses the real Gekko2 launcher
 * pages (include/frontend.h: browser, developer switches) and adds a Wii U main page. No GX2 needed. */
#ifndef GEKKO2_WIIU_MENU_H
#define GEKKO2_WIIU_MENU_H
#include <stdint.h>
#include <stddef.h>

enum { WM_UP = 1, WM_DOWN = 2, WM_LEFT = 4, WM_RIGHT = 8, WM_A = 16, WM_B = 32, WM_START = 64, WM_PAGE_UP = 128, WM_PAGE_DOWN = 256, WM_X = 512, WM_Y = 1024, WM_MINUS = 2048, WM_ZR = 4096 };
enum { WM_ACTION_NONE = 0, WM_ACTION_START_BIOS = 1, WM_ACTION_START_DISC = 2, WM_ACTION_EXIT = 3 };
#define WM_W 640u
#define WM_H 480u

typedef struct wiiu_menu wiiu_menu;
wiiu_menu *wiiu_menu_create(const char *sd_root, const char *bios_name);
void wiiu_menu_destroy(wiiu_menu *m);
/* edges = buttons newly pressed this frame (WM_* bits). Returns a WM_ACTION_*. A start action applies the chosen
 * developer switches (gekko2_opt_apply) before returning. */
int wiiu_menu_input(wiiu_menu *m, unsigned edges);
void wiiu_menu_render(wiiu_menu *m, uint32_t *xrgb);
/* Selected disc image path, or NULL. */
const char *wiiu_menu_disc(const wiiu_menu *m);
/* Short status text for the main page (e.g. why the last start failed). */
void wiiu_menu_set_notice(wiiu_menu *m, const char *text);
/* 1 when the tester switched the verbose SD log on (X on the main page; stored as wiiu/gekko2/verbose_log_on.txt). */
int wiiu_menu_verbose(const wiiu_menu *m);
/* GPU (GX2) GS renderer switch, toggled with - on the main page, saved as sd:/wiiu/gekko2/gx2_gs_on.txt. */
int wiiu_menu_gpu(const wiiu_menu *m);
/* With the GPU renderer: GS work on its own core (main thread, GX2 core), emulation on core 2. ZR toggles it,
 * saved as sd:/wiiu/gekko2/gx2_split_on.txt. */
int wiiu_menu_gpu_split(const wiiu_menu *m);
/* Controller mapping. Every Wii U controller (GamePad, Pro Controller, Classic Controller (Pro), Wii Remote with or
 * without Nunchuk) is first read as these unified buttons; the user's table then maps them to the PS2 pad. */
enum { G2B_A, G2B_B, G2B_X, G2B_Y, G2B_L, G2B_R, G2B_ZL, G2B_ZR, G2B_PLUS, G2B_MINUS, G2B_UP, G2B_DOWN, G2B_LEFT,
       G2B_RIGHT, G2B_L3, G2B_R3, G2B_COUNT };
/* PS2 pad bits (IOP_PAD_BTN_*) for the held unified buttons (bit n = G2B_n). */
uint16_t wiiu_menu_map_buttons(const wiiu_menu *m, uint32_t unified_hold);
/* Raw unified buttons pressed this frame; used by the Controls page to capture a new assignment. */
int wiiu_menu_raw_trigger(wiiu_menu *m, uint32_t unified_trigger); /* 1 = consumed */
/* Full-screen "Gekko2 Wii U Edition ALPHA - Loading" picture shown from the start of a game until its first frame. */
void wiiu_menu_render_loading(wiiu_menu *m, uint32_t *xrgb, const char *title);
#endif
