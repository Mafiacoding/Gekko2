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

    uint8_t *heap = MEMAllocFromDefaultHeapEx(4096, 4096);
    say("heap buffer: %08x", (unsigned)(uintptr_t)heap);
    if (heap) {
        memcpy(heap, code, sizeof code);
        DCFlushRange(heap, 64);
        ICInvalidateRange(heap, 64);
        say("step 2: about to run code in a heap buffer ...");
        int r = ((fn_t)heap)();
        say("step 2 result: %d (%s)", r, r == 42 ? "heap code ran" : "unexpected value");
    }
    if (cg && cgsize) {
        say("step 3: about to copy code to the codegen area ...");
        OSCodegenCopy(cg, code, sizeof code);
        say("step 3: about to run it ...");
        int r = ((fn_t)cg)();
        say("step 3 result: %d (%s)", r, r == 42 ? "codegen code ran" : "unexpected value");
    } else {
        say("step 3 skipped: no codegen area for this app");
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
