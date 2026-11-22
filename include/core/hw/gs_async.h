/* GPL-3.0-or-later. Wii U port only: optional GS/GIF worker thread.
 *
 * The emulation thread stays the only producer. GIF/GS state (g_gif, the GS register block and VRAM)
 * is owned by the worker while it is attached; the emulation thread calls gs_async_drain() before it
 * observes any of that state. Never compiled into the Wii build (guarded by GEKKO2_WIIU). */
#ifndef GEKKO2_GS_ASYNC_H
#define GEKKO2_GS_ASYNC_H
#include <stdint.h>

#ifdef GEKKO2_WIIU
extern volatile int gs_async_on;                 /* 1 while a worker is attached */
/* idle() is called while a side waits (sleep/yield). Attach after system_init(), before the worker runs. */
void gs_async_attach(void (*idle)(void));
int gs_async_enable(void);                       /* after the worker thread started: 0 = GIF work now goes to the worker */
void gs_async_worker_main(void);                 /* run on the worker thread; returns after gs_async_detach() */
void gs_async_detach(void);                      /* emulation thread: drain, then ask the worker to leave */
void gs_async_drain(void);                       /* wait until everything submitted so far has been processed (or is stalled) */
void gs_async_pump(void);                        /* deliver deferred GS interrupts; cheap, call once per scheduler grant */
void gs_async_lock(void);
void gs_async_unlock(void);
void gs_async_touch(void);                       /* EE-side state changed: stalled data must be retried */
void gs_async_stats(uint64_t out[8]);            /* [0]entries [1]qwords [2]drains that waited [3]fallbacks [4]ring-full waits [5]emulation ns waiting in drains [6]worker busy ns */
#else
#define gs_async_on 0
static inline void gs_async_drain(void) {}
static inline void gs_async_pump(void) {}
static inline void gs_async_lock(void) {}
static inline void gs_async_unlock(void) {}
static inline void gs_async_touch(void) {}
#endif
#endif
