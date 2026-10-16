#ifndef GEKKO2_EXPERIMENTAL_OPTIONS_H
#define GEKKO2_EXPERIMENTAL_OPTIONS_H
enum { GEKKO2_EXP_CHAIN, GEKKO2_EXP_PSM, GEKKO2_EXP_GX_RISK, GEKKO2_EXP_COUNT };
/* Independent of the already full 32-bit CPU/GX mask. All default OFF.
 * Request/load never changes a running session. Apply only after releasing
 * native owners and before initializing a new cold-boot session. */
unsigned gekko2_experimental_available(void);
int gekko2_experimental_requested(unsigned option);
int gekko2_experimental_enabled(unsigned option);
int gekko2_experimental_request(unsigned option,int enabled);
void gekko2_experimental_apply_cold_boot(void);
int gekko2_experimental_load(unsigned option,const char *path);
int gekko2_experimental_save(unsigned option,const char *path);
#endif
