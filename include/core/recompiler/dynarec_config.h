#ifndef GEKKO2_DYNAREC_CONFIG_H
#define GEKKO2_DYNAREC_CONFIG_H
/* CPU execution and diagnostics must be independently selectable. */
#include "core/recompiler/optimization.h"
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
#define GEKKO2_EE_BLOCKS_ENABLED 1
#else
#define GEKKO2_EE_BLOCKS_ENABLED 0
#endif
#if GEKKO2_EE_BLOCKS_ENABLED && !defined(GEKKO2_LEGACY_SCHEDULER)
#define GEKKO2_SCHEDULER_QUANTA_ENABLED 1
#else
#define GEKKO2_SCHEDULER_QUANTA_ENABLED 0
#endif
#endif
