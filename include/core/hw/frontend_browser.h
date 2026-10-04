#ifndef FRONTEND_BROWSER_H
#define FRONTEND_BROWSER_H
#include <stddef.h>
#define FRONTEND_BROWSER_PATH 512
/* Bounded native SD directory model. File paths are never guest memory. */
typedef struct {char name[256];unsigned directory;} frontend_browser_entry;
typedef struct {
 char root[FRONTEND_BROWSER_PATH],path[FRONTEND_BROWSER_PATH];
 frontend_browser_entry *entries;
 unsigned count,selected,truncated,capacity;
} frontend_browser;
void frontend_browser_release(frontend_browser *b);
/* Release an initialized browser before reinitializing it. */
int frontend_browser_init(frontend_browser *b,const char *root,const char *start);
int frontend_browser_scan(frontend_browser *b,const char *path);
/* Returns 1 with selected ISO/BIN path, 0 after entering directory, -1 on error. */
int frontend_browser_activate(frontend_browser *b,char *selected,size_t capacity);
int frontend_browser_up(frontend_browser *b);
int frontend_browser_is_image(const char *name);
#endif
