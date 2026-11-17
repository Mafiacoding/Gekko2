/* GPL-3.0-or-later. Wii U JIT feasibility probe. Reports whether generated PowerPC code can run:
 *  1) codegen area (coreinit OSGetCodegenVirtAddrRange): address, size, allowed core
 *  2) code placed in a normal heap buffer, flushed with DCFlushRange + ICInvalidateRange, then called
 *  3) code placed in the codegen area via OSCodegenCopy, then called (only if the area exists)
 * Each step prints "about to run" BEFORE executing, so a crash leaves the last line on screen.
 * No Gekko2 code. No guarantee about real hardware: Cemu behaviour is not Wii U behaviour. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <coreinit/cache.h>
#include <coreinit/core.h>
#include <coreinit/codegen.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <unistd.h>

static const uint32_t code[2] = { 0x3860002Au /* li r3,42 */, 0x4E800020u /* blr */ };
typedef int (*fn_t)(void);

static FILE *logf;
static void say(const char *fmt, ...)
{
    char b[160];
    __builtin_va_list a;
    __builtin_va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    __builtin_va_end(a);
    WHBLogPrint(b);
    WHBLogConsoleDraw();
    if (logf) {
        fprintf(logf, "%s\n", b);
        fflush(logf);
        fsync(fileno(logf));
    }
}


/* Runs on the codegen core. Method A: OSCodegenCopy. Method B: switch the area to RW, write, switch to RX. */
static int codegen_thread(int argc, void *argv)
{
    (void)argc; (void)argv;
    void *cg = NULL;
    uint32_t cgsize = 0;
    OSGetCodegenVirtAddrRange(&cg, &cgsize);
    say("thread core: %u, mode: %d", (unsigned)OSGetCoreId(), (int)OSGetSecCodeGenMode());

    say("method A: about to OSCodegenCopy ...");
    OSCodegenCopy(cg, code, sizeof code);
    say("method A: copied; about to run it ...");
    int r = ((fn_t)cg)();
    say("method A result: %d (%s)", r, r == 42 ? "codegen code ran" : "unexpected value");

    uint32_t *p = (uint32_t *)((uint8_t *)cg + 64);
    say("method B: about to switch to RW ...");
    OSSwitchSecCodeGenMode(CODEGEN_RW_);
    p[0] = 0x38600063u; /* li r3,99 */
    p[1] = 0x4E800020u; /* blr */
    DCFlushRange(p, 8);
    ICInvalidateRange(p, 8);
    OSSwitchSecCodeGenMode(CODEGEN_R_X);
    say("method B: about to run it ...");
    r = ((fn_t)p)();
    say("method B result: %d (%s)", r, r == 99 ? "RW/RX switching works" : "unexpected value");
    return 0;
}

int main(void)
{
    WHBProcInit();
    WHBLogConsoleInit();
    if (WHBMountSdCard()) {
        char path[256];
        snprintf(path, sizeof path, "%s/wiiu/gekko2/jitprobe.log", WHBGetSdCardMountPath());
        logf = fopen(path, "w");
    }
    say("Gekko2 Wii U JIT probe (log file: sd:/wiiu/gekko2/jitprobe.log, %s)", logf ? "open" : "not available");
    void *cg = NULL;
    uint32_t cgsize = 0;
    OSGetCodegenVirtAddrRange(&cg, &cgsize);
    say("codegen area: addr=%08x size=%u core=%u", (unsigned)(uintptr_t)cg, (unsigned)cgsize,
        (unsigned)OSGetCodegenCore());
    say("current core: %u", (unsigned)OSGetCoreId());

    /* Heap execution crashed Cemu (probe v2), so it is no longer tried. Codegen must be used from core
     * OSGetCodegenCore(); run the test on a thread pinned to that core. */
    if (cg && cgsize) {
        static OSThread thread;
        const uint32_t stack_size = 64u * 1024u;
        void *stack = MEMAllocFromDefaultHeapEx(stack_size, 16);
        if (!stack) {
            say("no memory for the test thread stack");
        } else {
            uint32_t affinity = 1u << (unsigned)OSGetCodegenCore();
            OSCreateThread(&thread, codegen_thread, 0, NULL, (uint8_t *)stack + stack_size, stack_size, 16, affinity);
            OSSetThreadName(&thread, "codegen-probe");
            say("starting test thread on core %u ...", (unsigned)OSGetCodegenCore());
            OSResumeThread(&thread);
            int ret = 0;
            OSJoinThread(&thread, &ret);
            say("test thread finished");
        }
    } else {
        say("no codegen area for this app: a dynarec is not possible here");
    }
    say("done. Press HOME to leave.");
    while (WHBProcIsRunning()) {
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(100));
    }
    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}
