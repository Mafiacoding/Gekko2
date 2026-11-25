/* GPL-3.0-or-later. Wii U codegen-area glue for the dynarec (GEKKO2_WIIU_JIT builds only; empty otherwise).
 * Observed in Cemu 2.6: area 0x02018000, 16 MiB, core 0 only; OSCodegenCopy and RW/R_X switching both
 * run generated code; code in the normal heap crashes. Real Wii U behaviour is NOT verified. */
#include "core/recompiler/native_ppc.h"
#ifdef GEKKO2_WIIU_JIT
#include <coreinit/cache.h>
#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/title.h>
#include <coreinit/memorymap.h>
#include <coreinit/memheap.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memfrmheap.h>
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
/* On by default; sd:/wiiu/gekko2/jit_cemu_safe.txt turns it off. The BIOS hang seen with build 13 (timer calibration
 * loop, EE pc 0x9fc41080) was Cemu's recompiler getting the carry out of a chained SUBFE wrong; the EE 64-bit
 * compare no longer uses that chain on the Wii U (ppc_dynarec.c emit_slt_core). */
int gekko2_wiiu_cemu_fast_jit;
int gekko2_wiiu_jit_exp; /* extra optimization bits from jit_exp.txt (measurements) */
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
/* RWX code memory (technique from PPSSPP for Wii U, jereif/PPSSPP_wiiu Common/MemoryUtil.cpp, GPL-2.0-or-later;
 * re-implemented here). Under Aroma the range 0x00800000-0x01000000 is mapped read/write/execute and holds the
 * loader's heaps. A block taken from the largest expanded heap there (or from a frame heap) can hold generated code
 * without OSSwitchSecCodeGenMode and is not tied to the codegen core. Real Wii U only: Cemu does not have this
 * memory. sd:/wiiu/gekko2/jit_codegen.txt keeps the official codegen area. Not verified on hardware yet. */
int gekko2_wiiu_rwx_jit;
static char rwx_info[160];
static void *rwx_acquire(uint32_t *out_size)
{
    const uintptr_t lo = 0x00800000u, hi = 0x01000000u;
    MEMHeapHeader *best_exp = NULL, *best_frm = NULL;
    uint32_t best_exp_avail = 0, best_frm_avail = 0;
    for (uintptr_t page = lo; page < hi; page += 0x1000u) {
        if (!OSIsAddressValid((uint32_t)page))
            continue;
        for (uintptr_t a = page; a < page + 0x1000u; a += 4u) {
            MEMHeapHeader *h = (MEMHeapHeader *)a;
            if (h->tag != MEM_EXPANDED_HEAP_TAG && h->tag != MEM_FRAME_HEAP_TAG)
                continue;
            const uintptr_t ds = (uintptr_t)h->dataStart, de = (uintptr_t)h->dataEnd;
            if (ds < a + sizeof(MEMHeapHeader) || ds >= hi || de <= ds || de > hi)
                continue;
            if (h->tag == MEM_EXPANDED_HEAP_TAG) {
                const uint32_t av = MEMGetAllocatableSizeForExpHeapEx(h, 0x100);
                if (av > best_exp_avail) { best_exp_avail = av; best_exp = h; }
            } else {
                const uint32_t av = MEMGetAllocatableSizeForFrmHeapEx(h, 0x100);
                if (av > best_frm_avail) { best_frm_avail = av; best_frm = h; }
            }
        }
    }
    const uint32_t keep = 64u * 1024u, cap = 6u * 1024u * 1024u + 64u;
    void *p = NULL;
    uint32_t sz = 0;
    if (best_exp && best_exp_avail >= 512u * 1024u + keep) {
        sz = (best_exp_avail - keep) & ~0xfffu;
        if (sz > cap) sz = cap & ~0xfffu;
        p = MEMAllocFromExpHeapEx(best_exp, sz, -0x1000); /* from the tail, 4 KiB aligned */
        snprintf(rwx_info, sizeof rwx_info, "RWX block from expanded heap %p: %p, %u KiB", (void *)best_exp, p, sz / 1024u);
    }
    if (!p && best_frm && best_frm_avail >= 512u * 1024u + keep) {
        sz = (best_frm_avail - keep) & ~0xfffu;
        if (sz > cap) sz = cap & ~0xfffu;
        p = MEMAllocFromFrmHeapEx(best_frm, sz, -0x100);
        snprintf(rwx_info, sizeof rwx_info, "RWX block from frame heap %p: %p, %u KiB", (void *)best_frm, p, sz / 1024u);
    }
    if (!p) {
        snprintf(rwx_info, sizeof rwx_info, "no RWX heap with free space (best expanded %u KiB, frame %u KiB)",
                 best_exp_avail / 1024u, best_frm_avail / 1024u);
        return NULL;
    }
    *out_size = sz;
    return p;
}
const char *gekko2_codegen_rwx_info(void) { return rwx_info; }
/* Generated code may run on any core (RWX heap, or Cemu which does not enforce the codegen core). */
int gekko2_codegen_any_core(void) { return cemu_mode; }
/* Front end, before the emulator thread layout is chosen: take the RWX heap now. 1 = RWX code memory in use; 0 = not
 * available (the official codegen area is tried later, on the codegen core). */
int gekko2_codegen_try_rwx(void)
{
    if (acquired)
        return cemu_mode && area != NULL && rwx_info[0] == 'R';
    if (!gekko2_wiiu_rwx_jit || (OSGetTitleID() >> 32) == 0xffffffffull)
        return 0;
    void *r = rwx_acquire(&area_size);
    if (!r)
        return 0;
    acquired = 1;
    area = r;
    cemu_mode = 1;
    in_rx = 0;
    status = "RWX heap (Aroma) ready: no mode switches, any core";
    return 1;
}
void *gekko2_codegen_area(uint32_t *size)
{
    if (!acquired) {
        acquired = 1;
        if (gekko2_wiiu_rwx_jit && (OSGetTitleID() >> 32) != 0xffffffffull) {
            void *r = rwx_acquire(&area_size);
            if (r) {
                area = r;
                cemu_mode = 1; /* same handling: RWX memory needs no mode switches */
                in_rx = 0;
                status = "RWX heap (Aroma) ready: no mode switches, any core";
                if (size)
                    *size = area_size;
                return area;
            }
        }
        OSGetCodegenVirtAddrRange(&area, &area_size);
        if (!area || !area_size) {
            area = NULL;
            status = "no codegen area for this app";
        } else if ((OSGetTitleID() >> 32) == 0xffffffffull && gekko2_wiiu_cemu_fast_jit) {
            /* Cemu does not tie generated code to the codegen core, so the emulator may run on the main core. */
            cemu_mode = 1;
            in_rx = 0;
            status = "codegen area ready (Cemu: no mode switches, Cemu recompiles the generated code)";
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
