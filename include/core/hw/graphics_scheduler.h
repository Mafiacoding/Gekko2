#ifndef GEKKO2_GRAPHICS_SCHEDULER_H
#define GEKKO2_GRAPHICS_SCHEDULER_H
#include "core/hw/dma.h"
#include "core/hw/vif.h"
#include "core/hw/gif.h"
#include "core/hw/vu.h"
/* Same live fields as the service's idle check. No activity cache can
 * become stale after a producer write, reset or checkpoint restore. */
static inline int dma_graphics_work_pending_fast(void) {
 const dma_state_t *d=dma_scheduler_view;
 const vif_state_t *v0=vif0_scheduler_view,*v1=vif1_scheduler_view;
 return ((d->chan[0].chcr|d->chan[1].chcr|d->chan[2].chcr)&0x100u)||
  v0->stall_words||v1->stall_words||gif_scheduler_view->fifo_count||
  (v1->direct_needed_words&&v1->direct_have_words==v1->direct_needed_words);
}
static inline int vu1_work_pending_fast(void) {
 return vu1_scheduler_view->running||vu1_scheduler_view->xgkick_pending;
}
#endif
