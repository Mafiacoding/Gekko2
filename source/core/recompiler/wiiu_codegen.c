/* GPL-3.0-or-later. Wii U codegen-area glue for the dynarec (GEKKO2_WIIU_JIT builds only; empty otherwise).
 * Observed in Cemu 2.6: area 0x02018000, 16 MiB, core 0 only; OSCodegenCopy and RW/R_X switching both
 * run generated code; code in the normal heap crashes. Real Wii U behaviour is NOT verified. */
#include "core/recompiler/native_ppc.h"
#ifdef GEKKO2_WIIU_JIT
#include <coreinit/cache.h>
#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/title.h>
#include <stdio.h>
static void *area;
static uint32_t area_size;
static unsigned rw_depth, exec_depth;
static int in_rx;
static unsigned long long switch_count;
static int acquired;
static const char *status = "not initialised";
/* Cemu turns its own recompiler off for the whole codegen area as soon as OSSwitchSecCodeGenMode is called, so
 * generated code then runs in Cemu's slow interpreter. Cemu does not protect the area and tracks changes through
 * ICInvalidateRange, so on Cemu the area is written directly and never switched. Cemu is recognised by the
 * title ID it gives a directly loaded RPX (upper half ffffffff); a real Wii U never uses that range. */
static int cemu_mode;
/* Lazy mode switching: the area stays R_X after generated code ran and only a writer switches it back to RW.
 * Running code repeatedly therefore costs no mode switch; a switch happens only on a write/execute transition. */
static void to_rw(void)
{
    if (cemu_mode)
        return;
    if (in_rx) {
        OSSwitchSecCodeGenMode(CODEGEN_RW_);
        in_rx = 0;
        switch_count++;
    }
}
static void to_rx(void)
{
    if (cemu_mode)
        return;
    if (!in_rx) {
        OSSwitchSecCodeGenMode(CODEGEN_R_X);
        in_rx = 1;
        switch_count++;
    }
}
const char *gekko2_codegen_status(void) { return status; }
unsigned long long gekko2_codegen_switch_count(void) { return switch_count; }
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
        } else if ((OSGetTitleID() >> 32) == 0xffffffffull) {
            cemu_mode = 1;
            in_rx = 0;
            status = "codegen area ready (Cemu: no mode switches, Cemu recompiles the generated code)";
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
/* Tester diagnostics: which callers cause RW switches (return addresses, mapped to functions with the build's
 * symbol table). Up to 12 distinct callers. */
static struct { uintptr_t addr; unsigned long long n; } sw_site[12];
void gekko2_codegen_rw_begin(void)
{
    if (!area)
        return;
    rw_depth++;
    if (in_rx) {
        const uintptr_t a = (uintptr_t)__builtin_return_address(0);
        for (unsigned i = 0; i < 12u; i++)
            if (sw_site[i].addr == a || !sw_site[i].addr) {
                sw_site[i].addr = a;
                sw_site[i].n++;
                break;
            }
    }
    to_rw();
}
/* "addr:count addr:count ..." */
void gekko2_codegen_switch_sites(char *out, unsigned size)
{
    unsigned k = 0;
    if (!out || !size)
        return;
    out[0] = 0;
    /* reference point: the run-time address of this function, to undo the RPX load offset */
    k = (unsigned)snprintf(out, size, " ref=%08lx", (unsigned long)(uintptr_t)&gekko2_codegen_switch_sites);
    for (unsigned i = 0; i < 12u && sw_site[i].addr && k + 24u < size; i++)
        k += (unsigned)snprintf(out + k, size - k, " %08lx:%llu", (unsigned long)sw_site[i].addr, sw_site[i].n);
}
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
