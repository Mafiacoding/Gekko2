#ifndef GEKKO2_GUEST_ERROR_PROBE_H
#define GEKKO2_GUEST_ERROR_PROBE_H
#include <stdint.h>
#include "core/ee/ee_core.h"
/* Read-only diagnostic for the owner's Crash E3 NuError stop.
 * The fingerprint ensures other titles cannot expose unrelated guest RAM.
 * Never alters CPU state, guest RAM, scheduling, or control flow. */
typedef struct {
    uint32_t pc, sp, caller;
    char heading[160], detail[256];
} gekko2_guest_error_t;
int gekko2_guest_error_probe(const ee_state_t *st, gekko2_guest_error_t *out);
#endif
