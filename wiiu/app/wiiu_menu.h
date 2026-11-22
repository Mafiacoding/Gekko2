/* GPL-3.0-or-later. Gekko2 launcher menu for the Wii U test port. Platform neutral: it draws into a 640x480
 * 0x00RRGGBB buffer and takes button edges, so it is testable on the host. It reuses the real Gekko2 launcher
 * pages (include/frontend.h: browser, developer switches) and adds a Wii U main page. No GX2 needed. */
#ifndef GEKKO2_WIIU_MENU_H
#define GEKKO2_WIIU_MENU_H
#include <stdint.h>
#include <stddef.h>

enum { WM_UP = 1, WM_DOWN = 2, WM_LEFT = 4, WM_RIGHT = 8, WM_A = 16, WM_B = 32, WM_START = 64, WM_PAGE_UP = 128, WM_PAGE_DOWN = 256 };
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
#endif
