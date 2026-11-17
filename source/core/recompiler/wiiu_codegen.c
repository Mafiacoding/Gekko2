/* GPL-3.0-or-later. Wii U codegen-area glue for the dynarec (GEKKO2_WIIU_JIT builds only; empty otherwise).
 * Observed in Cemu 2.6: area 0x02018000, 16 MiB, core 0 only; OSCodegenCopy and RW/R_X switching both
 * run generated code; code in the normal heap crashes. Real Wii U behaviour is NOT verified. */
#include "core/recompiler/native_ppc.h"
#ifdef GEKKO2_WIIU_JIT
#include <coreinit/cache.h>
#include <coreinit/codegen.h>
#include <coreinit/core.h>
static void *area;
static uint32_t area_size;
static unsigned rw_depth, exec_depth;
static int in_rx;
static int acquired;
static const char *status = "not initialised";
/* Lazy mode switching: the area stays R_X after generated code ran and only a writer switches it back to RW.
 * Running code repeatedly therefore costs no mode switch; a switch happens only on a write/execute transition. */
static void to_rw(void)
{
    if (in_rx) {
        OSSwitchSecCodeGenMode(CODEGEN_RW_);
        in_rx = 0;
    }
}
static void to_rx(void)
{
    if (!in_rx) {
        OSSwitchSecCodeGenMode(CODEGEN_R_X);
        in_rx = 1;
    }
}
const char *gekko2_codegen_status(void) { return status; }
void *gekko2_codegen_area(uint32_t *size)
{
    if (!acquired) {
        acquired = 1;
        OSGetCodegenVirtAddrRange(&area, &area_size);
        if (!area || !area_size) {
            area = NULL;
            status = "no codegen area for this app";
        } else if (OSGetCoreId() != OSGetCodegenCore()) {
            area = NULL;
            status = "emulator thread is not on the codegen core";
        } else {
            OSSwitchSecCodeGenMode(CODEGEN_RW_); /* RW by default; R_X only while generated code runs */
            in_rx = 0;
            status = "codegen area ready";
        }
    }
    if (size)
        *size = area ? area_size : 0;
    return area;
}
void gekko2_codegen_rw_begin(void) { if (area) { rw_depth++; to_rw(); } }
void gekko2_codegen_rw_end(void)
{
    if (area && rw_depth && --rw_depth == 0 && exec_depth > 0)
        to_rx(); /* a write window inside generated code's call must hand an executable area back */
}
void gekko2_codegen_exec_begin(void)
{
    if (area) {
        exec_depth++;
        if (rw_depth == 0)
            to_rx();
    }
}
void gekko2_codegen_exec_end(void) { if (area && exec_depth) exec_depth--; }
#endif
