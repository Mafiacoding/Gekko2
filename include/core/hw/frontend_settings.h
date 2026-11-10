#ifndef GEKKO2_FRONTEND_SETTINGS_H
#define GEKKO2_FRONTEND_SETTINGS_H
typedef struct {unsigned hud,throughput,fps,gx;} frontend_settings;
void frontend_settings_defaults(frontend_settings *settings);
/* Strict versioned format; missing/invalid input leaves settings unchanged. */
int frontend_settings_load(frontend_settings *settings,const char *path);
int frontend_settings_save(const frontend_settings *settings,const char *path);
#endif
