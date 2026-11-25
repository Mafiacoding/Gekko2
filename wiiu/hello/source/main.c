/* GPL-3.0-or-later. Wii U feasibility probe (step 1): prints text and the CPU endianness/word size
 * on the TV via WHB. No Gekko2 code. Build with wiiu/Makefile; run in Cemu or on a Wii U. */
#include <stdint.h>
#include <stdio.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>
#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

int main(void)
{
    WHBProcInit();
    WHBLogConsoleInit();
    const uint32_t probe = 0x01020304u;
    WHBLogPrintf("Gekko2 Wii U probe (step 1)");
    WHBLogPrintf("big-endian: %s, sizeof(void*)=%u", *(const uint8_t *)&probe == 1u ? "yes" : "no", (unsigned)sizeof(void *));
    WHBLogPrintf("time base: %llu ticks/s", (unsigned long long)OSGetSystemInfo()->busClockSpeed / 4ull);
    WHBLogConsoleDraw();
    while (WHBProcIsRunning()) {
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(100));
    }
    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}
