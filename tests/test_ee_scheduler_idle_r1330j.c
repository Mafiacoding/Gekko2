/* Scheduler slots keep the other CPU running while EE retires nothing. */
#include <stdio.h>
#include <stdint.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"

static unsigned failures, callbacks;
#define CHECK(c,m) do { if(!(c)){printf("FAIL: %s\n",m);failures++;} } while(0)
static int wake_after_quantum(void)
{
    callbacks++;
    if(callbacks==1)ee_core_get_state()->idle=0;
    return 1;
}
static int count_quantum(void){callbacks++;return 1;}
static void start_idle(void)
{
    static bios_image_t bios;
    if(system_init(&bios,&bios))failures++;
    ee_state_t *ee=ee_core_get_state();
    ee->pc=0x80200000;ee->next_pc=ee->pc+4;
    ee->cop0[9]=100;ee->cop0[12]=0;ee->idle=1;
    iop_state_t *iop=iop_core_get_state();
    iop->idle=1;iop->cop0[12]=0;
}
int main(void)
{
    start_idle();
    ee_state_t *ee=ee_core_get_state();
    iop_state_t *iop=iop_core_get_state();
    uint64_t retired=ee->instructions_executed,phase=ee_core_display_clock_save();
    CHECK(ee_core_step_n(8)==0,"idle grant retires no fabricated instructions");
    CHECK(ee->cop0[9]==108,"idle grant runs exactly eight Count ticks");
    CHECK(ee_core_display_clock_save()==phase+8,"idle display clock advances by eight slots");
    CHECK(ee->instructions_executed==retired,"idle retirement counter stays unchanged");
    CHECK(system_run_interleaved(3)==0,"idle system grant returns to frontend");
    CHECK(iop->sched_ticks==3,"IOP continues while EE is parked");
    CHECK(ee->cop0[9]==132,"three pairs add exactly 24 EE park ticks");
    CHECK(ee->instructions_executed==retired,"system idle grants do not retire EE code");
    ee_core_shutdown();iop_core_shutdown();

    start_idle();ee=ee_core_get_state();callbacks=0;
    CHECK(ee_core_step_interleaved_n(3,count_quantum)==3,"native scheduler completes parked quanta");
    CHECK(callbacks==3&&ee->cop0[9]==124,"parked quantum callbacks stay at eight slots");
    CHECK(ee->instructions_executed==0,"parked native grants preserve zero retirements");
    ee_core_shutdown();iop_core_shutdown();

    start_idle();ee=ee_core_get_state();callbacks=0;
    CHECK(ee_core_step_interleaved_n(2,wake_after_quantum)==2,"IOP boundary can wake parked EE");
    CHECK(callbacks==2&&ee->cop0[9]==116,"wake preserves exact quantum timing");
    CHECK(ee->instructions_executed==8,"only awake quantum retires eight NOPs");
    ee_core_shutdown();iop_core_shutdown();

    start_idle();ee=ee_core_get_state();callbacks=0;ee->idle=0;
    /* Existing HLE returns 1 after handling syscall 100 but does not halt.
     * A partial EE quantum must retain its remaining seven slots. */
    ee->gpr[3].ud0=100;ee->ram[0x200000]=12;
    CHECK(ee_core_step_interleaved_n(2,count_quantum)==2,"HLE early yield completes requested quanta");
    CHECK(callbacks==2&&!ee->halted,"HLE early yield never discards IOP boundaries");
    CHECK(ee->instructions_executed==15,"handled HLE slot does not fabricate a retirement");
    CHECK(ee->pc==0x80200040,"HLE resumes exactly after the handled instruction");
    ee_core_shutdown();iop_core_shutdown();
    printf("EE idle scheduler: %u failures\n",failures);
    return failures?1:0;
}
