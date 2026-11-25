/* GPL-3.0-or-later. Host test of wiiu_menu: builds a fake SD tree, drives the menu with button edges and
 * writes each page to a PPM so the pages can be looked at. Not Wii U code. */
#include "wiiu_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static uint32_t fb[WM_W * WM_H];
static void dump(wiiu_menu *m, const char *dir, const char *name)
{
    char p[300]; snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    wiiu_menu_render(m, fb);
    FILE *f = fopen(p, "wb"); fprintf(f, "P6\n%u %u\n255\n", WM_W, WM_H);
    for (unsigned i = 0; i < WM_W * WM_H; i++) { fputc(fb[i] >> 16, f); fputc(fb[i] >> 8, f); fputc(fb[i], f); }
    fclose(f);
}
static void touch(const char *p) { FILE *f = fopen(p, "wb"); if (f) { fputs("x", f); fclose(f); } }
int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    char root[300], d[400];
    snprintf(root, sizeof root, "%s/sd", out); mkdir(root, 0755);
    snprintf(d, sizeof d, "%s/wiiu", root); mkdir(d, 0755);
    snprintf(d, sizeof d, "%s/wiiu/gekko2", root); mkdir(d, 0755);
    snprintf(d, sizeof d, "%s/wiiu/gekko2/games", root); mkdir(d, 0755);
    snprintf(d, sizeof d, "%s/wiiu/gekko2/games/Demo Disc A.iso", root); touch(d);
    snprintf(d, sizeof d, "%s/wiiu/gekko2/games/Second Game.chd", root); touch(d);
    snprintf(d, sizeof d, "%s/wiiu/gekko2/games/readme.txt", root); touch(d);
    wiiu_menu *m = wiiu_menu_create(root, "ps2bios.bin");
    int ok = m != NULL, act;
    dump(m, out, "01_main");
    for (int i = 0; i < 3; i++) wiiu_menu_input(m, WM_DOWN);          /* SELECT DISC */
    ok = ok && wiiu_menu_input(m, WM_A) == WM_ACTION_NONE;
    dump(m, out, "02_browser");
    wiiu_menu_input(m, WM_DOWN);                                      /* skip the ".." row */
    ok = ok && wiiu_menu_input(m, WM_A) == WM_ACTION_NONE;            /* picks the first image */
    ok = ok && wiiu_menu_disc(m) && strstr(wiiu_menu_disc(m), "Demo Disc A.iso");
    dump(m, out, "03_main_after_select");
    act = wiiu_menu_input(m, WM_A);                                   /* START DISC */
    ok = ok && act == WM_ACTION_START_DISC;
    for (int i = 0; i < 3; i++) wiiu_menu_input(m, WM_DOWN);          /* SETTINGS */
    wiiu_menu_input(m, WM_A);
    dump(m, out, "04_settings");
    for (int i = 0; i < 7; i++) wiiu_menu_input(m, WM_DOWN);          /* DEVELOPER SWITCHES */
    wiiu_menu_input(m, WM_A);
    dump(m, out, "05_developer");
    wiiu_menu_input(m, WM_B);
    wiiu_menu_input(m, WM_B);                                         /* back on the main page */
    wiiu_menu_input(m, WM_DOWN);                                      /* ABOUT */
    wiiu_menu_input(m, WM_A);
    dump(m, out, "06_about");
    wiiu_menu_input(m, WM_B);
    ok = ok && wiiu_menu_input(m, WM_START) == WM_ACTION_NONE;        /* START only resumes: no action */
    printf("disc=%s\n", wiiu_menu_disc(m) ? wiiu_menu_disc(m) : "(none)");
    wiiu_menu_destroy(m);
    puts(ok ? "PASS wiiu_menu host smoke" : "FAIL wiiu_menu host smoke");
    return ok ? 0 : 1;
}
