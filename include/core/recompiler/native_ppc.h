#ifndef GEKKO2_NATIVE_PPC_H
#define GEKKO2_NATIVE_PPC_H
/* GEKKO2_NATIVE_PPC: this build may execute generated PowerPC code. True on the Wii (GEKKO) and in the
 * Wii U JIT test build (GEKKO2_WIIU_JIT). On every other build (host tests, tools, the Wii U interpreter
 * build) the dynarec only emits bytes and never calls them. */
#if defined(GEKKO) || defined(GEKKO2_WIIU_JIT)
#define GEKKO2_NATIVE_PPC 1
#endif
#ifdef GEKKO2_WIIU_JIT
#include <stdint.h>
/* Wii U codegen area: readable and executable (R_X) while code runs, writable (RW) only while the code
 * cache or finalize() modify it. Only the codegen core (OSGetCodegenCore) may use it. NULL when the app has
 * no codegen area or this thread runs on another core; the dynarec then declines and the interpreter runs. */
void *gekko2_codegen_area(uint32_t *size);
void gekko2_codegen_rw_begin(void);
void gekko2_codegen_rw_end(void);
const char *gekko2_codegen_status(void);
#else
#define gekko2_codegen_rw_begin() ((void)0)
#define gekko2_codegen_rw_end() ((void)0)
#endif
#endif
