/* GPL-3.0-or-later. Wii U port only: optional IOP worker thread (see source/core/system.c).
 *
 * Ownership rule while the worker is attached:
 *  - IOP-private state is touched only by the worker while a catch-up job runs, and by the emulation (EE) thread only
 *    after G2_EE_CALLS_IOP() has waited for that job.
 *  - EE-side state reached from IOP code (SIF, SIF0 DMA, EE INTC, CDVD config, memory card) is touched by the worker only
 *    after G2_IOP_CALLS_EE() has waited until the EE thread is parked at a fixed point (a catch-up or a sync).
 * Both waits happen at points fixed by the EE instruction stream, so the result does not depend on thread timing.
 * In the Wii build the macros are empty. */
#ifndef GEKKO2_IOP_ASYNC_H
#define GEKKO2_IOP_ASYNC_H
#ifdef GEKKO2_WIIU
extern volatile int g_iop_async;
void system_ee_iop_sync(void);
void system_iop_worker_shared(void);
#define G2_EE_CALLS_IOP() do { if (g_iop_async) system_ee_iop_sync(); } while (0)
#define G2_IOP_CALLS_EE() do { if (g_iop_async) system_iop_worker_shared(); } while (0)
int gekko2_wiiu_event_sched_on(void);
void system_iop_async_attach(void (*idle)(void), void *stack_lo, void *stack_hi);
int system_iop_async_enable(void);
void system_iop_worker_main(void);
void system_iop_async_detach(void);
void system_iop_async_stats(unsigned long long out[4]);
extern int gekko2_wiiu_event_sched;
extern unsigned gekko2_wiiu_event_width;
#else
#define G2_EE_CALLS_IOP() do { } while (0)
#define G2_IOP_CALLS_EE() do { } while (0)
#endif
#endif
