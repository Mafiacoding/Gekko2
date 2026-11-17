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
/* Wii U codegen area: writable (RW) by default so the code cache can allocate and release freely; switched to
 * readable+executable (R_X) only around calls into generated code (CG_EXEC). A write window (rw_begin/rw_end)
 * nested inside an execution window temporarily switches back to RW, e.g. a helper called from generated code. Only the codegen core (OSGetCodegenCore) may use it. NULL when the app has
 * no codegen area or this thread runs on another core; the dynarec then declines and the interpreter runs. */
void *gekko2_codegen_area(uint32_t *size);
void gekko2_codegen_rw_begin(void);
void gekko2_codegen_rw_end(void);
void gekko2_codegen_exec_begin(void);
void gekko2_codegen_exec_end(void);
#define CG_EXEC(expr) ({ gekko2_codegen_exec_begin(); __typeof__(expr) cg_r_ = (expr); gekko2_codegen_exec_end(); cg_r_; })
#define CG_EXEC_VOID(expr) do { gekko2_codegen_exec_begin(); expr; gekko2_codegen_exec_end(); } while (0)
const char *gekko2_codegen_status(void);
#else
#define gekko2_codegen_rw_begin() ((void)0)
#define gekko2_codegen_rw_end() ((void)0)
#define CG_EXEC(expr) (expr)
#define CG_EXEC_VOID(expr) expr
#endif
#endif
