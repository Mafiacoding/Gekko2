/*
 * ee_hle_thread.c - see include/core/ee/ee_hle_thread.h for the full
 * design rationale, citations, and the Round 569 real-vectoring
 * experiment negative result that motivated this file.
 */
#include <string.h>
#include <stdio.h>
/* Round 1103 (task #1032): added <stdio.h> here because this file uses
 * fprintf(stderr, ...) in several #ifdef-gated diagnostic trace blocks
 * (R818_SEMA_TRACE, R1103_SEMA11_TRACE) but never included it directly;
 * it was apparently only compiling before because some other translation
 * unit in the same build pulled it in transitively, or because those
 * gated blocks had never actually been compiled together until now. */
/* Round 818 (task #823/#824, per user's next-step trace-back request):
 * a lightweight, always-independent-of-R812_EVENTLOG diagnostic that
 * logs only CreateSema and SignalSema/iSignalSema calls (never
 * WaitSema), so it can run for the ENTIRE boot window without the
 * unbounded stderr growth R812_EVENTLOG's WaitSema-entry logging would
 * cause once a thread starts busy-parking (that handler re-executes
 * and re-logs every single scheduler tick once blocked - fine for a
 * short targeted window, unusable for a full multi-hundred-million-
 * instruction survey). Purpose: identify the real CreateSema call site
 * (and its caller $ra) that allocates semid 0 - the semaphore thread 2
 * genuinely WaitSema-parks on at pc=0x0101bc24 (Round 817 correction) -
 * and every SignalSema(0)/iSignalSema(0) call (with caller $ra) that
 * should be releasing it, to find semid 0's real, expected producer. */
#ifdef R818_SEMA_TRACE
#include <stdio.h>
#endif
#include "core/ee/ee_hle_thread.h"

#define EE_HLE_THREAD_MAX_THREADS 32
#define EE_HLE_THREAD_MAX_SEMAS   64

/* Real status bits (ee/kernel/include/kernel.h). */
#define EE_THS_RUN         0x01u
#define EE_THS_READY       0x02u
#define EE_THS_WAIT        0x04u
#define EE_THS_SUSPEND     0x08u
#define EE_THS_WAITSUSPEND 0x0Cu
#define EE_THS_DORMANT     0x10u

/* Real wait-type values (ee/kernel/include/kernel.h - DIFFERENT
 * numbering from the IOP side's thbase.h, see header comment). */
#define EE_TSW_NONE 0
#define EE_TSW_SLEEP 1
#define EE_TSW_SEMA  2

/* A fixed, honestly-labeled simplification (same spirit as the IOP
 * side's own THREAD_STACK_ARENA): real CreateThread takes the stack
 * pointer/size directly from the caller-supplied ee_thread_t struct
 * (the game itself owns/allocates the stack memory, unlike the IOP's
 * SYSMEM-backed allocation) - so no bump arena is needed here at all,
 * simplifying this file relative to its IOP counterpart. */

typedef struct {
    int in_use;
    uint32_t status;
    uint32_t attr, option;
    uint32_t entry;      /* func */
    uint32_t stack_base, stack_size;
    uint32_t gp_reg;
    uint32_t priority;      /* current */
    uint32_t init_priority;
    int wait_type;    /* EE_TSW_* */
    int wait_id;       /* sema id when wait_type==EE_TSW_SEMA */
    uint32_t wakeup_count;
    uint32_t ready_seq;

    /* Saved register context - meaningful whenever this thread is NOT
     * the currently-live one (see header's scheduling-model comment). */
    ee_reg128_t gpr[32];
    uint32_t pc, next_pc;
    ee_reg128_t hi, lo;
    uint32_t sa_reg;
    /* SCPH-50004 kernel context save at RAM 0x80003718 stores F0..F31,
     * FCR31 and ACC in addition to GPR/HI/LO/SA. */
    uint32_t fpr[32], fcr31, acc;
} ee_tcb_t;

typedef struct {
    int in_use;
    uint32_t attr, option;
    int32_t max_count;
    int32_t count;
    int32_t wait_threads;
} ee_sema_internal_t;

static struct {
    ee_tcb_t threads[EE_HLE_THREAD_MAX_THREADS];
    int thread_count;
    int current_thread_id; /* 1-based; 0 = none yet */
    uint32_t ready_seq_counter;

    ee_sema_internal_t semas[EE_HLE_THREAD_MAX_SEMAS];
} g;

/* Round 733 (task #447, GT3-in-game-code stall investigation, user:
 * "use all sources available to track down this issue and fix them
 * once and for all"): live per-target call counters for WakeupThread/
 * _iWakeupThread and SignalSema/iSignalSema - diagnostic-only, same
 * "project-internal accessor" convention as Round 732's CDVD dispatch
 * counters (iop_cdvd.c). Deliberately NOT part of the checkpointed `g`
 * blob (mirrors Round 732's choice not to checkpoint its counters
 * either) - these answer a live, per-process-run empirical question
 * ("does ANY thread ever call WakeupThread(3) or WakeupThread(5) - GT3's
 * own two real, currently-sleeping worker threads found parked with
 * wait_type=TSW_SLEEP/wakeup_count=0 - or SignalSema on whatever they
 * might really be gated behind"), not something that needs to survive
 * a save/resume cycle. */
static uint64_t g_wakeup_call_count[EE_HLE_THREAD_MAX_THREADS + 1];
static uint64_t g_signal_call_count[EE_HLE_THREAD_MAX_SEMAS + 1];
/* R1225: ground-truth sema2 producer probe in the ACTIVE HLE scheduler.
 * Earlier R1223 accidentally instrumented ee_core.c's superseded fallback
 * semaphore table; ee_hle_thread_try_handle() consumes these syscalls first. */
static uint32_t g_r1225_s2_pc, g_r1225_s2_ra;
static int32_t g_r1225_s2_sys, g_r1225_s2_tid, g_r1225_s2_before, g_r1225_s2_after;
/* R1193: capture the first two real SignalSema/iSignalSema syscalls. */
typedef struct { uint32_t pc, ra; int32_t sysnum, semid, before, after, wait_before, wait_after, ret; } r1193_sig_t;
static r1193_sig_t g_r1193_sig[2];
static uint32_t g_r1193_sig_count;

/* R1226: first active-HLE syscalls after the second real SignalSema(2).
 * This compares the post-Sema5/Sema2 execution point against Claude's
 * proven scheduler without touching behavior. */
typedef struct { uint32_t pc, ra, a0; int32_t sys, tid; } r1226_evt_t;
static r1226_evt_t g_r1226_evt[16];
static uint32_t g_r1226_n;
/* R1227: scheduler truth after the common second SignalSema(2) point. */
typedef struct { int32_t old_tid,next_tid; uint32_t pc; int32_t old_st,next_st,old_wt,next_wt; } r1227_sw_t;
static r1227_sw_t g_r1227_sw[12];
static uint32_t g_r1227_sw_n;
/* R1228: exact SleepThread(50) -> reschedule truth after Sema2 producer. */
typedef struct { uint32_t pc,ra,adv_pc,saved_pc,after_pc; int32_t tid,wakeup,before_st,after_st,before_wt,after_wt,picked,cur_after,picked_st,picked_wt; } r1228_sleep_t;
static r1228_sleep_t g_r1228_sleep; static uint32_t g_r1228_hits;
typedef struct { uint32_t pc,ra,a0,a1; int32_t sys,tid; } r1229_evt_t;
static r1229_evt_t g_r1229_evt[16]; static uint32_t g_r1229_n;

/* Round 1117 (task #929/#536): per-semaphore re-park tick counter for
 * the orphan-producer-unblock shortcut above; see its citation for
 * full rationale. Reset to 0 on module init alongside g_signal_call_count. */
static int g_orphan_sema_repark_count[EE_HLE_THREAD_MAX_SEMAS + 1];
#define EE_ORPHAN_SEMA_REPARK_THRESHOLD 8

/* Round 733 continuation (task #447): the force-wake diagnostic proved
 * threads 3/5 stay READY-but-never-scheduled forever even once woken,
 * because pick_next_ready()'s FIFO tiebreak always favors whichever
 * same-priority thread has the OLDEST ready_seq - and the currently-
 * RUNNING thread never loses that advantage unless something calls
 * RotateThreadReadyQueue() (sysnum 43/-44) on its own priority level
 * (the real PS2 kernel's documented fairness mechanism - real games
 * that run multiple same-priority worker threads are expected to call
 * this periodically, typically from their main-loop/VSync handler).
 * This counter answers the empirical question "does GT3's thread 4
 * ever actually call it" - decisive for classifying whether this is a
 * real emulator dispatch gap or genuinely-not-yet-reached game code. */
static uint64_t g_rotate_call_count;
uint64_t ee_hle_thread_get_rotate_calls(void) { return g_rotate_call_count; }

/* Round 812 (task #811 continuation, per user-relayed external-review
 * request): temporary, compile-gated (R812_EVENTLOG) state-transition
 * event log. Diagnostic-only - completely absent from normal builds
 * (regression suite, Wii cross-build) since the macro is never
 * defined there, so this has zero cost/behavioral effect outside a
 * dedicated diagnostic tool build. Purpose: distinguish, with direct
 * evidence rather than static disassembly guesswork, between the two
 * live hypotheses for GT3 thread 1's WAIT/SEMA/5 anomaly (Round 811b):
 * (a) a genuine WaitSema(5) block occurred once, and thread 1 later
 * "quietly" passed a re-check without its status/wait_type/wait_id
 * fields ever being cleared, or (b) TCB slot 1 was freed (Delete/
 * Terminate/ExitThread) and later reallocated by CreateThread to an
 * entirely different logical thread, which then independently blocked
 * on WaitSema(5) itself - a real, if confusing, behavior rather than a
 * stale-field bug. Every event line is plain text to stderr, matching
 * this project's established r811_cdvdtrace.c-style diagnostic
 * convention, so it can be captured via a driver's stderr redirection
 * across checkpoint-chained runs and grepped/analyzed afterward. */
#ifdef R812_EVENTLOG
#include <stdio.h>
static uint64_t g_evt_seq = 0;
static int g_evt_filter_tid = -1; /* -1 = log every thread */
void ee_hle_thread_eventlog_set_filter(int tid) { g_evt_filter_tid = tid; }
/* Round 815 (task #811/#820): default ENABLED so every prior
 * R812_EVENTLOG tool's behavior is unchanged unless it opts in to
 * calling the new setter below. */
static int g_evt_enabled = 1;
void ee_hle_thread_eventlog_set_enabled(int enabled) { g_evt_enabled = enabled; }
static int evt_pass(int tid) { return g_evt_enabled && (g_evt_filter_tid < 0 || g_evt_filter_tid == tid); }
#define EVT(tid, ...) do { \
        if (evt_pass((int)(tid))) { \
            fprintf(stderr, "[R812EVT] seq=%llu tid=%d ", \
                    (unsigned long long)(++g_evt_seq), (int)(tid)); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } \
    } while (0)
#else
#define EVT(tid, ...) do {} while (0)
void ee_hle_thread_eventlog_set_filter(int tid) { (void)tid; }
void ee_hle_thread_eventlog_set_enabled(int enabled) { (void)enabled; }
#endif

void ee_hle_thread_init(void)
{
    memset(&g, 0, sizeof(g));
    memset(g_wakeup_call_count, 0, sizeof(g_wakeup_call_count));
    memset(g_signal_call_count, 0, sizeof(g_signal_call_count));
    g_r1225_s2_pc=g_r1225_s2_ra=0; g_r1225_s2_sys=g_r1225_s2_tid=0; g_r1225_s2_before=g_r1225_s2_after=-1;
    memset(g_r1193_sig, 0, sizeof(g_r1193_sig)); g_r1193_sig_count = 0;
    memset(g_r1226_evt,0,sizeof(g_r1226_evt)); g_r1226_n=0; memset(g_r1227_sw,0,sizeof(g_r1227_sw)); g_r1227_sw_n=0;
    memset(&g_r1228_sleep,0,sizeof(g_r1228_sleep)); g_r1228_hits=0; memset(g_r1229_evt,0,sizeof(g_r1229_evt)); g_r1229_n=0;
    memset(g_orphan_sema_repark_count, 0, sizeof(g_orphan_sema_repark_count)); /* Round 1117 */
    g_rotate_call_count = 0;
}

uint64_t ee_hle_thread_get_wakeup_calls(int thid)
{
    if (thid < 0 || thid > EE_HLE_THREAD_MAX_THREADS) return 0;
    return g_wakeup_call_count[thid];
}

uint64_t ee_hle_thread_get_signal_calls(int semid)
{
    if (semid < 0 || semid > EE_HLE_THREAD_MAX_SEMAS) return 0;
    return g_signal_call_count[semid];
}

void ee_hle_thread_get_r1225_sema2(uint32_t *pc,uint32_t *ra,int32_t *sys,int32_t *tid,int32_t *before,int32_t *after)
{ if(pc)*pc=g_r1225_s2_pc; if(ra)*ra=g_r1225_s2_ra; if(sys)*sys=g_r1225_s2_sys; if(tid)*tid=g_r1225_s2_tid; if(before)*before=g_r1225_s2_before; if(after)*after=g_r1225_s2_after; }

void ee_hle_thread_get_r1193_signal_diag(uint32_t idx, uint32_t *pc, uint32_t *ra, int32_t *sysnum, int32_t *semid, int32_t *before, int32_t *after, int32_t *wb, int32_t *wa, int32_t *ret)
{
    r1193_sig_t z = {0}; r1193_sig_t *d = idx < g_r1193_sig_count && idx < 2 ? &g_r1193_sig[idx] : &z;
    if (pc) *pc=d->pc; if (ra) *ra=d->ra; if (sysnum) *sysnum=d->sysnum; if (semid) *semid=d->semid;
    if (before) *before=d->before; if (after) *after=d->after; if (wb) *wb=d->wait_before; if (wa) *wa=d->wait_after; if (ret) *ret=d->ret;
}
uint32_t ee_hle_thread_get_r1193_signal_diag_count(void) { return g_r1193_sig_count; }
uint32_t ee_hle_thread_get_r1226_count(void) { return g_r1226_n; }
void ee_hle_thread_get_r1226(uint32_t i,uint32_t *pc,uint32_t *ra,int32_t *sys,int32_t *tid,uint32_t *a0)
{ r1226_evt_t z={0}, *e=(i<g_r1226_n&&i<16)?&g_r1226_evt[i]:&z; if(pc)*pc=e->pc;if(ra)*ra=e->ra;if(sys)*sys=e->sys;if(tid)*tid=e->tid;if(a0)*a0=e->a0; }
uint32_t ee_hle_thread_get_r1227_count(void){return g_r1227_sw_n;}
void ee_hle_thread_get_r1227(uint32_t i,int32_t *old_tid,int32_t *next_tid,uint32_t *pc,int32_t *old_st,int32_t *next_st,int32_t *old_wt,int32_t *next_wt)
{ r1227_sw_t z={0},*e=(i<g_r1227_sw_n&&i<12)?&g_r1227_sw[i]:&z; if(old_tid)*old_tid=e->old_tid;if(next_tid)*next_tid=e->next_tid;if(pc)*pc=e->pc;if(old_st)*old_st=e->old_st;if(next_st)*next_st=e->next_st;if(old_wt)*old_wt=e->old_wt;if(next_wt)*next_wt=e->next_wt; }
uint32_t ee_hle_thread_get_r1228(uint32_t *pc,uint32_t *ra,uint32_t *adv,uint32_t *saved,uint32_t *after,int32_t *tid,int32_t *wake,int32_t *bst,int32_t *ast,int32_t *bwt,int32_t *awt,int32_t *picked,int32_t *cur_after,int32_t *pst,int32_t *pwt)
{ r1228_sleep_t *e=&g_r1228_sleep; if(pc)*pc=e->pc;if(ra)*ra=e->ra;if(adv)*adv=e->adv_pc;if(saved)*saved=e->saved_pc;if(after)*after=e->after_pc;if(tid)*tid=e->tid;if(wake)*wake=e->wakeup;if(bst)*bst=e->before_st;if(ast)*ast=e->after_st;if(bwt)*bwt=e->before_wt;if(awt)*awt=e->after_wt;if(picked)*picked=e->picked;if(cur_after)*cur_after=e->cur_after;if(pst)*pst=e->picked_st;if(pwt)*pwt=e->picked_wt; return g_r1228_hits; }
uint32_t ee_hle_thread_get_r1229_count(void){return g_r1229_n;}
void ee_hle_thread_get_r1229(uint32_t i,uint32_t *pc,uint32_t *ra,int32_t *sys,int32_t *tid,uint32_t *a0,uint32_t *a1){r1229_evt_t z={0},*e=(i<g_r1229_n&&i<16)?&g_r1229_evt[i]:&z;if(pc)*pc=e->pc;if(ra)*ra=e->ra;if(sys)*sys=e->sys;if(tid)*tid=e->tid;if(a0)*a0=e->a0;if(a1)*a1=e->a1;}

/* Round 1035 (task #536/#447 continuation): pure read-only diagnostic
 * accessor into the semaphore table - same "project-internal accessor"
 * convention as Round 733's per-target call counters above. Exposes
 * in_use/max_count/count/wait_threads for a given semid so a survey
 * driver can dump the real semaphore state at the end of a run,
 * without needing R812_EVENTLOG's full (too-verbose-to-run-to-
 * completion) WaitSema-entry event log. */
int ee_hle_thread_get_sema_state(int semid, int *out_in_use,
                                  int32_t *out_max_count,
                                  int32_t *out_count,
                                  int32_t *out_wait_threads)
{
    if (semid < 0 || semid >= EE_HLE_THREAD_MAX_SEMAS) return -1;
    ee_sema_internal_t *s = &g.semas[semid];
    if (out_in_use) *out_in_use = s->in_use;
    if (out_max_count) *out_max_count = s->max_count;
    if (out_count) *out_count = s->count;
    if (out_wait_threads) *out_wait_threads = s->wait_threads;
    return 0;
}

void ee_hle_thread_get_checkpoint_blob(void **ptr, uint32_t *size)
{
    *ptr = &g;
    *size = (uint32_t)sizeof(g);
}

static ee_tcb_t *tcb(int thid) /* thid is 1-based */
{
    if (thid < 1 || thid > EE_HLE_THREAD_MAX_THREADS) return NULL;
    return &g.threads[thid - 1];
}

/* Round 733 diagnostic-only (task #447, GT3 stall investigation): forces
 * a WAIT/SLEEP thread to READY, using the EXACT same real-hardware
 * transition as the sysnum==51 WakeupThread handler below (mirrors it
 * rather than calling it, since that handler is inlined in
 * ee_hle_thread_try_handle() and not separately callable). This is NOT
 * wired into any EE syscall path and never fires during organic
 * emulation - it exists purely so a scratch driver can test the
 * hypothesis "would GT3's threads 3/5 go on to do real work (e.g. issue
 * a CDVD read) if something had woken them", without first having to
 * locate why the real wake call never happens. Same diagnostic-injection
 * precedent as Round 568's synthetic WaitSema(0) signal test. */
void ee_hle_thread_debug_force_wakeup(int thid)
{
    ee_tcb_t *t = tcb(thid);
    if (!t || !t->in_use) return;
    if (t->status == EE_THS_WAIT && t->wait_type == EE_TSW_SLEEP) {
        t->status = EE_THS_READY;
        t->wait_type = EE_TSW_NONE;
        t->ready_seq = g.ready_seq_counter++;
    } else {
        t->wakeup_count++;
    }
}

static int alloc_tcb_slot(void)
{
    for (int i = 0; i < EE_HLE_THREAD_MAX_THREADS; i++) {
        if (!g.threads[i].in_use) return i + 1;
    }
    return 0;
}

static int alloc_sema_slot(void)
{
    /* Round 569 fix: real hardware (and this project's own prior,
     * proven-working g_ee_sema[] table) assigns 0-based semaphore
     * IDs - the first CreateSema() call returns id=0. This matters
     * because real BIOS/game code sometimes hardcodes low semaphore
     * IDs (e.g. the semid=0 WaitSema park traced in Round 567/568)
     * rather than always threading through CreateSema's return
     * value. Returning 1-based IDs here (the earlier, buggy version
     * of this function) silently shifted every real semaphore ID by
     * one and broke that hardcoded-ID assumption, which is what
     * regressed the diskless BIOS boot baseline (pmode stuck 0x0)
     * even in an otherwise-correct, non-blocking CreateSema call.
     * -1 (not 0) is the "table full" sentinel now, since 0 is a
     * legitimate id. */
    for (int i = 0; i < EE_HLE_THREAD_MAX_SEMAS; i++) {
        if (!g.semas[i].in_use) return i;
    }
    return -1;
}

static ee_sema_internal_t *sema(int semid)
{
    if (semid < 0 || semid >= EE_HLE_THREAD_MAX_SEMAS) return NULL;
    return &g.semas[semid];
}

/* Mirror ExecPS2's kernel-object teardown in the separate HLE model.
 * PS2SDK ee/kernel/src/osdsrc/src/ExecPS2.c: delete other threads,
 * InitSemaphores, current priority=0 and cleared wait/wakeup state. */
void ee_hle_thread_on_exec(ee_state_t *st)
{
    int id=g.current_thread_id;
    ee_tcb_t *old=tcb(id), keep;
    int valid=old && old->in_use;
    if(valid)keep=*old;
    memset(&g,0,sizeof(g));
    memset(g_orphan_sema_repark_count,0,sizeof(g_orphan_sema_repark_count));
    if(valid){
        keep.status=EE_THS_RUN; keep.priority=keep.init_priority=0;
        keep.wait_type=EE_TSW_NONE; keep.wait_id=0; keep.wakeup_count=0;
        keep.entry=(uint32_t)st->gpr[4].ud0;
        keep.ready_seq=0;
        g.threads[id-1]=keep;g.thread_count=1;g.current_thread_id=id;
        g.ready_seq_counter=1;
    }
    st->idle=0;
}

static void ensure_root_thread(ee_state_t *st)
{
    if (g.thread_count > 0) return;
    ee_tcb_t *t = &g.threads[0];
    memset(t, 0, sizeof(*t));
    t->in_use = 1;
    t->status = EE_THS_RUN;
    /* Round 1090 fix (task: Rounds 1080-1089 priority-inversion arc,
     * user-decided Variante B). The prior default here was 64 - a
     * mid-range, explicitly-uncited placeholder (see the IOP-side
     * sibling's own comment in iop_hle_thread.c, ensure_root_thread()).
     * Rounds 1080-1082 proved live, via BIOS disassembly and an A/B
     * causal test, that 64 causes a real priority-inversion livelock:
     * it numerically outranks (is less urgent than) tid 9's genuine
     * firmware-assigned priority 32, so this implicit root thread can
     * lose scheduling to tid 9 in pick_next_ready()'s real, correct
     * "lower number = more urgent" comparison. Rounds 1083-1089
     * exhaustively searched for the real hardware value real firmware
     * would use here and found none exists to cite: real PS2 hardware's
     * own pre-THREADMAN bootstrap glue is not itself a THREADMAN-
     * scheduled thread at all (same conclusion the IOP-side comment
     * already reached independently), so there is no real priority
     * byte to reconstruct - Variante A (a documented/reconstructed
     * real firmware value) is not available. Per the user's explicit
     * decision, this project instead takes Variante B: an emulator-
     * internal modeling choice, not a claimed Sony hardware value.
     * 0 (THREADMAN's highest-urgency priority number) is chosen
     * because it is the closest correct approximation of what real
     * hardware actually does - the sequential pre-THREADMAN loader is
     * never preempted by anything, so representing it as unconditionally
     * more urgent than every real, positive-priority thread mirrors
     * that invariant until this implicit thread itself creates real
     * THREADMAN threads and participates in normal priority semantics.
     * Verified via two independent, twice-reproduced instrumentation
     * methods (Round 1089: full syscall trace + ReferThreadStatus
     * call-cadence trace) that priority 0 produces a byte-for-byte
     * identical boot trace to priority 1 - i.e. it reliably lands in
     * the same "correctly unblocks past tid 9" class as every other
     * tested value in [0,31], and does NOT reproduce the original
     * 64-class livelock. */
    t->priority = 0;
    t->init_priority = 0;
    t->ready_seq = g.ready_seq_counter++;
    memcpy(t->gpr, st->gpr, sizeof(t->gpr));
    t->pc = st->pc;
    t->next_pc = st->next_pc;
    t->hi = st->hi;
    t->lo = st->lo;
    t->sa_reg = st->sa_reg;
    memcpy(t->fpr, st->fpr, sizeof(t->fpr));
    t->fcr31 = st->fcr31;
    t->acc = st->acc;
    g.thread_count = 1;
    EVT(1, "event=current_thread_id-write old=0 new=1 reason=ensure_root_thread pc=0x%08x", st->pc);
    g.current_thread_id = 1;
}

static void save_context(ee_state_t *st, int thid)
{
    ee_tcb_t *t = tcb(thid);
    if (!t) return;
    memcpy(t->gpr, st->gpr, sizeof(t->gpr));
    t->pc = st->pc;
    t->next_pc = st->next_pc;
    t->hi = st->hi;
    t->lo = st->lo;
    t->sa_reg = st->sa_reg;
    memcpy(t->fpr, st->fpr, sizeof(t->fpr));
    t->fcr31 = st->fcr31;
    t->acc = st->acc;
}

static void load_context(ee_state_t *st, int thid)
{
    ee_tcb_t *t = tcb(thid);
    if (!t) return;
    memcpy(st->gpr, t->gpr, sizeof(t->gpr));
    st->pc = t->pc;
    st->next_pc = t->next_pc;
    st->hi = t->hi;
    st->lo = t->lo;
    st->sa_reg = t->sa_reg;
    memcpy(st->fpr, t->fpr, sizeof(st->fpr));
    st->fcr31 = t->fcr31;
    st->acc = t->acc;
}

/* Real priority-based pick, identical algorithm to the IOP side's own
 * pick_next_ready() (see that file's comment for the full real-
 * semantics rationale): lowest priority NUMBER wins, ties broken by
 * earliest ready_seq. */
static int pick_next_ready(void)
{
    int best = 0;
    uint32_t best_prio = 0xFFFFFFFFu;
    uint32_t best_seq = 0xFFFFFFFFu;
    for (int i = 0; i < EE_HLE_THREAD_MAX_THREADS; i++) {
        ee_tcb_t *t = &g.threads[i];
        if (!t->in_use) continue;
        if (t->status != EE_THS_RUN && t->status != EE_THS_READY) continue;
        if (t->priority < best_prio || (t->priority == best_prio && t->ready_seq < best_seq)) {
            best = i + 1;
            best_prio = t->priority;
            best_seq = t->ready_seq;
        }
    }
    return best;
}

/* Core scheduling point - called after any operation that could
 * change which thread should be running. See header comment: a plain
 * struct copy in/out of the single live register file, matching real
 * hardware's own physical context-switch mechanism. */
static void reschedule(ee_state_t *st)
{
    int old_current = g.current_thread_id;
    (void)old_current; /* only referenced by EVT(), a no-op unless R812_EVENTLOG is defined */
    int next = pick_next_ready();
    if (g_signal_call_count[2] >= 2 && g_r1227_sw_n < 12) {
        r1227_sw_t *e=&g_r1227_sw[g_r1227_sw_n++];
        e->old_tid=old_current; e->next_tid=next; e->pc=st->pc;
        ee_tcb_t *ot=tcb(old_current), *nt=tcb(next);
        e->old_st=ot?ot->status:0; e->next_st=nt?nt->status:0;
        e->old_wt=ot?ot->wait_type:0; e->next_wt=nt?nt->wait_type:0;
    }
    EVT(old_current, "event=reschedule old_current=%d next=%d pc=0x%08x next_priority=%d",
        old_current, next, st->pc, next ? (int)tcb(next)->priority : -1);
    /* Round 812 fix (task #811/#813, GT3 semaphore-5 anomaly - user-
     * relayed external-review plan's event-log instrumentation
     * request). Live evidence (R812EVT capture, GT3 disc-boot chain):
     * thread 1 parks cleanly in WaitSema(5)'s busy-park loop (status/
     * wait_type/wait_id = WAIT/SEMA/5, saved pc pinned at the syscall
     * instruction 0x0101bc24) for ~850M further instructions with
     * reschedule() repeatedly finding nothing else ready (next=0) and
     * g.current_thread_id never once being written away from 1 - then,
     * with NO intervening WaitSema-success, status-change, load-
     * context, or current_thread_id-write event for tid=1, an
     * "event=WakeupThread target=3 ... pc=0x0101bb24" line appears
     * still tagged tid=1 (i.e. g.current_thread_id was still literally
     * 1), followed immediately by "event=reschedule old_current=1
     * next=3 pc=0x0101bb28" and a switch-out save-context at that same
     * pc. 0x0101bb28 is the jr-ra return address of a DIFFERENT
     * syscall trampoline (WakeupThread's, per Round 811's decoded
     * stub table) than WaitSema's - not a value thread 1's own code
     * ever produces. ee_hle_thread_check_preempt() was ruled out as
     * the mechanism (it explicitly requires cur->status==EE_THS_RUN
     * before ever calling reschedule(), so it can never touch a WAIT
     * thread). The remaining explanation, consistent with every
     * observed event: a hardware interrupt fired while thread 1 was
     * the live context, its handler executed an interrupt-safe
     * syscall (WakeupThread/-52 iWakeupThread is the real PS2 kernel's
     * own "i"-prefixed convention for exactly this - calls issued from
     * interrupt-handler code), and THAT syscall's own reschedule()
     * call found thread 3 newly READY and performed a full context
     * switch. At that moment the "live" st registers belonged to the
     * INTERRUPT HANDLER (which must return via ERET/COP0 EPC, not via
     * this thread-level mechanism) - not to thread 1's own suspended
     * WaitSema state - so the switch-out's unconditional
     * save_context(st, g.current_thread_id) silently overwrote thread
     * 1's real saved pc (0x0101bc24) with the interrupt handler's
     * mid-flight pc (0x0101bb28), while thread 1's WAIT/SEMA/5 fields
     * were never touched (matching Round 811b's original static
     * finding exactly: status/wait_type/wait_id unchanged, saved pc
     * drifted). This is the same class of hazard Round 598 already
     * fixed for check_preempt()'s per-instruction path (a forced
     * context swap while Status.EXL/ERL is set corrupts the pending
     * ERET's return-address assumption) - reschedule() itself had no
     * equivalent guard, and unlike check_preempt() it CAN be reached
     * while genuinely mid-exception, via any interrupt-context syscall
     * handler that calls it directly (WakeupThread/-52, SignalSema/
     * -67, and any future one). Defer the actual context switch (and
     * the none-ready branch's re-save, which has the identical hazard)
     * until Status.EXL/ERL clears; the target thread is already marked
     * READY by the syscall handler itself and will be picked up safely
     * on a later, non-exception reschedule() or by check_preempt()
     * once ERET returns and clears EXL. */
    if (st->cop0[12] & 0x6u) {
        EVT(old_current, "event=reschedule-deferred reason=mid-exception(EXL/ERL) pc=0x%08x", st->pc);
        return;
    }
    if (next == 0) {
        /* Nothing at all is ready. Round 855 fix (task #855, user's
         * "1 dann 2 dann 3" step 3): this USED TO simply leave the
         * live context exactly as-is (whatever the caller already set
         * st->pc/next_pc to) - correct for WaitSema's own deliberate
         * park-by-not-advancing-pc convention (Round 569/781), but a
         * real, live-reproduced bug for SleepThread's self-block path
         * (which calls EE_ADVANCE() BEFORE reschedule()): with no
         * other thread to switch to, ee_step() just kept fetching/
         * decoding/executing the SLEEPING thread's own subsequent
         * code forever, despite its status correctly reading
         * EE_THS_WAIT the whole time. Proved live via
         * tools/round855-idle-scheduler/r855_repro.c (see
         * ee_core.h's `idle` field doc comment for the full citation).
         * Fix: when the thread that was just live is no longer
         * actually RUN (the exact bug signature), set st->idle instead
         * of leaving its stale context to be replayed - ee_step() then
         * stops fetching real instructions until the real scheduler
         * (called again via ee_hle_thread_reschedule_kick(), from
         * ee_step()'s own idle-tick loop) finds something ready. */
        if (g.current_thread_id != 0) {
            /* Round 824 fix (task #846): see the switch-out branch below
             * for the full evidence writeup - the same unconditional-
             * save hazard applies here whenever g.current_thread_id's
             * own tracked status isn't RUN (e.g. WAIT). Only re-save
             * when it's genuinely the live running thread. */
            ee_tcb_t *cur0 = tcb(g.current_thread_id);
            if (cur0 && cur0->status == EE_THS_RUN) {
                EVT(g.current_thread_id, "event=save-context pc=0x%08x reason=reschedule-none-ready", st->pc);
                save_context(st, g.current_thread_id);
            } else if (cur0) {
                /* The live "st" register file belongs to a thread that
                 * is no longer RUN (it just self-blocked) and nothing
                 * was loaded to replace it - Round 855's exact bug
                 * signature. Go idle instead of letting ee_step() keep
                 * executing this thread's own stale/stray code. */
                EVT(g.current_thread_id, "event=idle-set pc=0x%08x reason=reschedule-none-ready-not-run status=0x%x", st->pc, cur0->status);
                st->idle = 1;
            }
        }
        return;
    }
    /* Real forward progress is about to happen (a thread either keeps
     * running or gets switched in below) - Round 855: always clear
     * `idle` here so a just-woken thread's own real fetch/decode/
     * execute resumes normally next ee_step() call. */
    st->idle = 0;
    if (next != g.current_thread_id) {
        if (g.current_thread_id != 0) {
            ee_tcb_t *cur = tcb(g.current_thread_id);
            if (cur && cur->status == EE_THS_RUN) {
                EVT(g.current_thread_id, "event=status old=0x%x new=0x2 reason=reschedule-switch-out pc=0x%08x", cur->status, st->pc);
                cur->status = EE_THS_READY;
                EVT(g.current_thread_id, "event=save-context pc=0x%08x reason=reschedule-switch-out", st->pc);
                save_context(st, g.current_thread_id);
            }
            /* Round 824 fix (task #846, GT3 WaitSema(5) saved-pc
             * corruption - live-captured causal event, R846G2 tool,
             * seq=2532941-2532943, total_instr=38865639, ~308
             * instructions after thread 1's genuine park):
             * g.current_thread_id can point at a thread that is NOT
             * actually RUN (e.g. thread 1 correctly parked in
             * WaitSema(5)/WAIT) while the live "st" register file
             * instead holds a transient execution - here, an
             * interrupt/critical-section-driven WakeupThread(target=3)
             * call (event=WakeupThread ... pc=0x0101bb24) that runs "on
             * top of" the idle CPU state because reschedule()'s own
             * none-ready branch leaves st untouched when nothing is
             * ready. Captured live: Status=0x70030c10 at the corrupting
             * reschedule (EXL=ERL=0, so Round 812's existing mid-
             * exception guard above does not fire; only IE, bit0, is
             * observed clear here) vs 0x70030c11 (IE=1) on every prior
             * genuinely-idle reschedule - confirming this is a real,
             * distinct code path from the EXL/ERL case Round 812
             * targeted, not a duplicate. Before this fix,
             * save_context(st, g.current_thread_id) ran unconditionally
             * here, so it silently overwrote thread 1's real saved pc
             * (0x0101bc24, already correctly persisted) with the
             * transient execution's pc (0x0101bb28) merely because
             * g.current_thread_id still read 1. Gating the save on
             * cur->status == EE_THS_RUN (matching the existing
             * READY-downgrade guard just above, which was already
             * conditional - only the save call itself was not) leaves
             * thread 1's real parked state untouched; the target thread
             * (3) is still loaded and marked RUN normally below. */
        }
        load_context(st, next);
        EVT(next, "event=load-context pc=0x%08x reason=reschedule-switch-in", st->pc);
        tcb(next)->status = EE_THS_RUN;
        EVT(next, "event=current_thread_id-write old=%d new=%d reason=reschedule", old_current, next);
        g.current_thread_id = next;
    } else {
        ee_tcb_t *cur = tcb(g.current_thread_id);
        if (cur) cur->status = EE_THS_RUN;
    }
}

/* Round 734 diagnostic-only (task #447, GT3 stall investigation
 * continuation, user: "maybe its time we write our self some code
 * RotateThreadReadyQueue" - see this round's clarification: NOT a
 * proposal to make our scheduler auto-rotate on its own, since Round
 * 712's own cited research already established real EE kernel hardware
 * has no automatic timeslicing among equal-priority threads - that
 * would be fabricating non-real behavior (the exact Round 549 mistake).
 * Instead, this is a scratch-driver-callable injection point, applying
 * the EXACT same real transition as the sysnum==43/-44
 * RotateThreadReadyQueue handler below (mirrored, not called - that
 * handler is inlined in ee_hle_thread_try_handle()), immediately
 * followed by a real reschedule() so a context switch can actually
 * happen. This exists purely to test what threads 3/5 DO once they
 * finally get real CPU time - the Round 733 force_wakeup experiment
 * flipped their status to READY but never called reschedule()
 * afterward, so pick_next_ready()'s FIFO tiebreak (thread 4's
 * long-standing, older ready_seq always wins) meant nothing ever
 * visibly changed. This function is NOT wired into any EE syscall path
 * and never fires during organic emulation. */
void ee_hle_thread_debug_force_rotate(ee_state_t *st, int priority)
{
    int earliest = 0;
    uint32_t earliest_seq = 0xFFFFFFFFu;
    for (int i = 0; i < EE_HLE_THREAD_MAX_THREADS; i++) {
        ee_tcb_t *t = &g.threads[i];
        if (t->in_use && (int32_t)t->priority == priority &&
            (t->status == EE_THS_READY || t->status == EE_THS_RUN) &&
            t->ready_seq < earliest_seq) {
            earliest = i + 1;
            earliest_seq = t->ready_seq;
        }
    }
    if (earliest) tcb(earliest)->ready_seq = g.ready_seq_counter++;
    reschedule(st);
}

/* Wakes the earliest (FIFO, real default SA_THFIFO-equivalent - this
 * project's ee_sema_t doesn't expose a real SA_THPRI attribute bit in
 * its own already-established field layout, so FIFO-only is the
 * correct, honest default here) thread waiting on sema `semid`, if
 * any. Returns 1 if a waiter was woken (signal transferred directly,
 * count untouched, matching real semantics), 0 if none waiting. */
static int wake_one_sema_waiter(int semid)
{
    int best = 0;
    uint32_t best_seq = 0xFFFFFFFFu;
    for (int i = 0; i < EE_HLE_THREAD_MAX_THREADS; i++) {
        ee_tcb_t *t = &g.threads[i];
        if (!t->in_use || t->status != EE_THS_WAIT || t->wait_type != EE_TSW_SEMA || t->wait_id != semid)
            continue;
        if (best == 0 || t->ready_seq < best_seq) {
            best = i + 1;
            best_seq = t->ready_seq;
        }
    }
#ifdef R1103_SEMA11_TRACE
    /* Round 1103 (task #1032, per user's precise follow-up spec):
     * narrow, sema-11-only trace of which waiter (if any) is picked
     * here, to distinguish Fall B/C (signal correctly finds/marks
     * tid9 READY, vs. finds no waiter at all / picks someone else)
     * from Fall A (tid2 never needs a woken waiter because it steals
     * its own produced count via an immediate WaitSema first). */
    if (semid == 11) {
        fprintf(stderr, "[R1103] event=wake_one_sema_waiter sem=11 found_tid=%d best_seq=%u\n",
                best, best == 0 ? 0u : best_seq);
    }
#endif
    if (best == 0) return 0;
    ee_tcb_t *t = tcb(best);
    EVT(best, "event=wake_one_sema_waiter sem=%d old_status=0x%x old_wait_type=%d old_wait_id=%d pc=0x%08x waiter_priority=%d signaler_tid=%d",
        semid, t->status, t->wait_type, t->wait_id, t->pc, t->priority, g.current_thread_id);
    t->status = EE_THS_READY;
    t->wait_type = EE_TSW_NONE;
    t->wait_id = 0;
    t->ready_seq = g.ready_seq_counter++;
    ee_sema_internal_t *s = sema(semid);
    if (s) s->wait_threads--;
    return 1;
}

/* Round 817 (task #811/#821, GT3 semaphore-5 probe, per user's
 * explicit narrowly-gated diagnostic spec): exposes the EXACT real
 * SignalSema(semid)/iSignalSema(semid) transition (see the
 * sysnum==66/-67 handler above - this is a byte-for-byte mirror of
 * its count-increment + wake_one_sema_waiter() sequence, not a new
 * behavior) as a directly-callable function, so a compile-time-gated
 * (GT3_SEM5_PROBE) diagnostic driver can signal a semaphore without
 * fabricating a syscall/register calling context. This is the
 * "existing wake path" the user's spec explicitly required ("do not
 * edit the TCB directly... use the existing wake path"): count is
 * incremented (bounded by max_count, matching the real E_KERNEL_
 * SEMA_OVF error case) and wake_one_sema_waiter() is invoked exactly
 * as SignalSema's own handler does. Deliberately does NOT call
 * reschedule() itself (unlike the live syscall handler) - this is
 * meant to be called from a driver's own slice loop, outside any EE
 * instruction step, and the already-established WaitSema busy-park
 * idiom re-checks s->count and calls reschedule() on its own very
 * next tick regardless (see the sysnum==68 handler's own citation),
 * so no separate reschedule() call is needed or safe to add here.
 * NOT wired into any EE syscall path; never fires during organic
 * emulation - same "diagnostic-only accessor" convention already
 * established by ee_hle_thread_debug_force_wakeup() (Round 733) and
 * ee_hle_thread_debug_force_rotate() (Round 734) above.
 * Returns 1 on success, 0 if semid is invalid/unused, -1 if the
 * semaphore is already at max_count (real overflow case - the real
 * SignalSema handler's own -419 error path, surfaced here as a
 * distinct return so a probe driver can tell "already signaled"
 * apart from "nothing to signal"). */
int ee_hle_thread_debug_signal_sema(int semid)
{
    ee_sema_internal_t *s = sema(semid);
    if (!s || !s->in_use) return 0;
    if (s->count >= s->max_count) return -1;
    s->count++;
    wake_one_sema_waiter(semid); /* bookkeeping only, matches real handler - does not gate the increment above */
    return 1;
}

int ee_hle_thread_try_handle(ee_state_t *st, int32_t sysnum, uint32_t this_pc, int in_delay_slot)
{
    (void)in_delay_slot;
    static const int32_t handled[] = {
        32, 33, 34, 35, 36, 37, -38, 39, 40, 41, -42, 43, -44,
        47, -47, 48, -49, 50, 51, -52, 53, -54,
        64, 65, 66, -67, 68, 69, -70, 71, -72, -73
    };
    int recognized = 0;
    for (size_t i = 0; i < sizeof(handled) / sizeof(handled[0]); i++) {
        if (handled[i] == sysnum) { recognized = 1; break; }
    }
    if (!recognized) return 0;

    ensure_root_thread(st);
    int cur = g.current_thread_id;
    uint32_t ra = (uint32_t)st->gpr[31].ud0;
    if (g_signal_call_count[2] >= 2 && g_r1226_n < 16) {
        r1226_evt_t *e=&g_r1226_evt[g_r1226_n++]; e->pc=this_pc; e->ra=ra; e->sys=sysnum; e->tid=cur; e->a0=(uint32_t)st->gpr[4].ud0;
    }
    if (g_r1228_hits > 0 && cur == 1 && g_r1229_n < 16) { r1229_evt_t *e=&g_r1229_evt[g_r1229_n++]; e->pc=this_pc;e->ra=ra;e->sys=sysnum;e->tid=cur;e->a0=(uint32_t)st->gpr[4].ud0;e->a1=(uint32_t)st->gpr[5].ud0; }
#define EE_RET(v) do { st->gpr[2].ud0 = (uint64_t)(int64_t)(int32_t)(v); } while (0)
    /* Round 569 fix: every syscall completion (blocking or not) must
     * advance PC to the instruction AFTER the syscall, exactly like
     * this project's original, proven g_ee_sema[] handlers did
     * (st->pc = this_pc + 4u; st->next_pc = this_pc + 8u;). The
     * earlier version of this file instead jumped straight to $ra
     * ("return to caller") for every completion - which is wrong for
     * MIPS `syscall` semantics: $ra is NOT a call-return address for
     * this instruction (unlike `jal`), it's whatever the calling
     * ps2sdk stub function last set it to, and that same stub
     * function typically has its OWN code between the `syscall`
     * instruction and its eventual `jr $ra` (saving the return value,
     * restoring saved registers, etc). Jumping straight to $ra
     * silently skipped all of that every single time, which is what
     * actually regressed the diskless BIOS boot baseline (pmode
     * stuck at 0x0) - not any of the semaphore-specific bugs fixed
     * above, though those were real bugs too. (void)ra suppresses
     * the now-unused-variable warning if no branch below still needs
     * it. */
    (void)ra;
#define EE_ADVANCE() do { st->pc = this_pc + 4u; st->next_pc = this_pc + 8u; } while (0)

    if (sysnum == 32) {
        /* CreateThread(ee_thread_t *thread) - a0=param ptr. Real
         * field layout cited in header: func@4, stack@8,
         * stack_size@0xC, gp_reg@0x10, initial_priority@0x14. */
        uint32_t param = (uint32_t)st->gpr[4].ud0;
        uint32_t func = ee_mem_read32(st, param + 4u);
        uint32_t stack = ee_mem_read32(st, param + 8u);
        uint32_t stack_size = ee_mem_read32(st, param + 0xCu);
        uint32_t gp_reg = ee_mem_read32(st, param + 0x10u);
        int32_t priority = (int32_t)ee_mem_read32(st, param + 0x14u);
        uint32_t attr = ee_mem_read32(st, param + 0x1Cu);
        uint32_t option = ee_mem_read32(st, param + 0x20u);
        int slot = alloc_tcb_slot();
        if (slot == 0) {
            EE_RET(-1);
        } else {
            ee_tcb_t *t = tcb(slot);
            memset(t, 0, sizeof(*t));
            t->in_use = 1;
            t->status = EE_THS_DORMANT;
            t->attr = attr; t->option = option;
            t->entry = func;
            t->stack_base = stack;
            t->stack_size = stack_size;
            t->gp_reg = gp_reg;
            t->priority = (uint32_t)priority;
            t->init_priority = (uint32_t)priority;
            if (slot > g.thread_count) g.thread_count = slot;
            EVT(slot, "event=CreateThread slot=%d entry=0x%08x pc=0x%08x", slot, func, this_pc);
            EE_RET(slot);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 33) {
        /* DeleteThread(int thid) */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        ee_tcb_t *t = tcb(thid);
        if (t && t->in_use && t->status == EE_THS_DORMANT) {
            EVT(thid, "event=DeleteThread target=%d pc=0x%08x", thid, this_pc);
            t->in_use = 0;
            EE_RET(0);
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 34) {
        /* StartThread(int thid, void *arg) - a0=thid, a1=arg. Real EE
         * crt0 thread entries take (void *arg) per ee_thread_t.func's
         * documented signature. */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        uint32_t arg = (uint32_t)st->gpr[5].ud0;
        ee_tcb_t *t = tcb(thid);
        if (t && t->in_use && t->status == EE_THS_DORMANT) {
            memset(t->gpr, 0, sizeof(t->gpr));
            uint32_t stack_top = (t->stack_base + t->stack_size) & ~0xFu; /* real EE o32 16-byte SP alignment */
            t->gpr[29].ud0 = stack_top; /* $sp */
            t->gpr[28].ud0 = t->gp_reg; /* $gp - real, caller-supplied per-thread value (unlike IOP's inherited-gp simplification) */
            t->gpr[31].ud0 = 0u; /* $ra - real threads never return; treated as ExitThread-equivalent dead end if they do (matches real ps2sdk documented convention: entry functions call ExitThread themselves) */
            t->gpr[4].ud0 = (uint64_t)arg; /* $a0 */
            t->pc = t->entry;
            t->next_pc = t->entry + 4u;
            t->status = EE_THS_READY;
            t->ready_seq = g.ready_seq_counter++;
            EE_RET(0);
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        reschedule(st); /* the newly-READY thread may now pre-empt the caller if higher priority */
        return 1;
    }
    if (sysnum == 35 || sysnum == 36) {
        /* ExitThread() / ExitDeleteThread() - no args, no return.
         * Round 835 fix (task #811 continuation): every other syscall
         * handler in this file calls EE_ADVANCE() (pc = this_pc+4)
         * before reschedule() - this one didn't. Real MIPS `syscall`
         * semantics require the trap handler to move PC past the
         * trapping instruction; skipping that here meant that if
         * reschedule() ever found nothing else ready (the exact
         * situation this project's own scheduler has no idle-thread
         * fallback for - see Round 833/834's STATUS.md writeup), pc
         * was left pointing AT the `syscall` instruction itself, so
         * the next ee_step() would re-decode and re-dispatch the very
         * same ExitThread/ExitDeleteThread syscall on an already-
         * dormant thread forever. This does not by itself explain the
         * GT3 pc==0 case (that thread never reaches this syscall
         * handler at all - it dies via the separate null-jalr guard's
         * auto-exit heuristic in ee_core.c), but it is a real,
         * independently evidenced bug on its own terms: any thread
         * that legitimately calls ExitThread/ExitDeleteThread while no
         * other thread is ready would hit this same never-advances
         * park. Fixed to match every sibling handler's convention. */
        if (cur) {
            EVT(cur, "event=%s target=%d pc=0x%08x", sysnum == 36 ? "ExitDeleteThread" : "ExitThread", cur, this_pc);
            tcb(cur)->status = EE_THS_DORMANT;
            if (sysnum == 36) tcb(cur)->in_use = 0;
        }
        EE_ADVANCE();
        reschedule(st);
        return 1;
    }
    if (sysnum == 37 || sysnum == -38) {
        /* TerminateThread(int thid) / iTerminateThread(int thid) */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        ee_tcb_t *t = tcb(thid);
        if (t && t->in_use && thid != cur) {
            EVT(thid, "event=TerminateThread target=%d old_status=0x%x pc=0x%08x", thid, t->status, this_pc);
            t->status = EE_THS_DORMANT;
            t->wait_type = EE_TSW_NONE; t->wait_id = 0;
            EE_RET(0);
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 39 || sysnum == 40) {
        /* DisableDispatchThread/EnableDispatchThread - real kernel-
         * level preemption toggle around a critical section. This
         * project's HLE syscalls already run atomically with respect
         * to each other, so honored as a real, harmless no-op (same
         * established precedent as the IOP side's own identical
         * pair). */
        EE_RET(0);
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 41 || sysnum == -42) {
        /* ChangeThreadPriority(int thid, int priority) */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        int32_t priority = (int32_t)st->gpr[5].ud0;
        ee_tcb_t *t = tcb(thid);
        if (t && t->in_use) {
            t->priority = (uint32_t)priority;
            EE_RET(0);
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        reschedule(st);
        return 1;
    }
    if (sysnum == 43 || sysnum == -44) {
        g_rotate_call_count++; /* Round 733 - see field comment */
        /* RotateThreadReadyQueue(int priority) - 0 = caller's own
         * current priority. Same real-equivalent implementation as
         * the IOP side: give the earliest-ready_seq thread at that
         * priority a fresh (latest) ready_seq. */
        int32_t priority = (int32_t)st->gpr[4].ud0;
        if (priority == 0 && cur) priority = (int32_t)tcb(cur)->priority;
        int earliest = 0;
        uint32_t earliest_seq = 0xFFFFFFFFu;
        for (int i = 0; i < EE_HLE_THREAD_MAX_THREADS; i++) {
            ee_tcb_t *t = &g.threads[i];
            if (t->in_use && (int32_t)t->priority == priority &&
                (t->status == EE_THS_READY || t->status == EE_THS_RUN) &&
                t->ready_seq < earliest_seq) {
                earliest = i + 1;
                earliest_seq = t->ready_seq;
            }
        }
        if (earliest) tcb(earliest)->ready_seq = g.ready_seq_counter++;
        EE_RET(0);
        EE_ADVANCE();
        reschedule(st);
        return 1;
    }
    if (sysnum == 47 || sysnum == -47) {
        /* GetThreadId() - no args. */
        EE_RET(cur);
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 48 || sysnum == -49) {
        /* ReferThreadStatus(int thid, ee_thread_status_t *info) */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        uint32_t info = (uint32_t)st->gpr[5].ud0;
        ee_tcb_t *t = tcb(thid);
        if (t && t->in_use) {
            ee_mem_write32(st, info + 0x00u, t->status);
            ee_mem_write32(st, info + 0x04u, t->entry);
            ee_mem_write32(st, info + 0x08u, t->stack_base);
            ee_mem_write32(st, info + 0x0Cu, t->stack_size);
            ee_mem_write32(st, info + 0x10u, t->gp_reg);
            ee_mem_write32(st, info + 0x14u, t->init_priority);
            ee_mem_write32(st, info + 0x18u, t->priority);
            ee_mem_write32(st, info + 0x1Cu, t->attr);
            ee_mem_write32(st, info + 0x20u, t->option);
            ee_mem_write32(st, info + 0x24u, (uint32_t)t->wait_type);
            ee_mem_write32(st, info + 0x28u, (uint32_t)t->wait_id);
            EE_RET(0);
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 50) {
        /* SleepThread() - no args. */
        ee_tcb_t *t = tcb(cur);
        if (t && g_signal_call_count[2] >= 2) {
            g_r1228_hits++; g_r1228_sleep.pc=this_pc; g_r1228_sleep.ra=(uint32_t)st->gpr[31].ud0; g_r1228_sleep.tid=cur;
            g_r1228_sleep.wakeup=t->wakeup_count; g_r1228_sleep.before_st=t->status; g_r1228_sleep.before_wt=t->wait_type;
        }
        if (t) {
            if (t->wakeup_count > 0) {
                t->wakeup_count--;
                EE_RET(0);
                EE_ADVANCE();
            } else {
                EE_RET(0); /* pre-set: the real return value once woken */
                EE_ADVANCE();
                if (g_signal_call_count[2] >= 2) g_r1228_sleep.adv_pc=st->pc;
                EVT(cur, "event=status old=0x%x new=0x4 wait_type=SLEEP reason=SleepThread pc=0x%08x", t->status, this_pc);
                /* Round 826 fix (task #811): same stale-saved-pc gap as
                 * WaitSema-block above (see that comment for the full
                 * evidence writeup) - this self-block also sets status
                 * to WAIT before calling reschedule(), so reschedule()'s
                 * Round-824 status==RUN save gate would otherwise skip
                 * persisting this thread's own live, just-advanced pc
                 * (this_pc+4, the real post-syscall resume point). Save
                 * directly here while status is still RUN. */
                save_context(st, cur);
                if (g_signal_call_count[2] >= 2) g_r1228_sleep.saved_pc=t->pc;
                t->status = EE_THS_WAIT;
                t->wait_type = EE_TSW_SLEEP;
                t->wait_id = 0;
                if (g_signal_call_count[2] >= 2) { int pn=pick_next_ready(); ee_tcb_t *pt=tcb(pn); g_r1228_sleep.picked=pn; g_r1228_sleep.picked_st=pt?pt->status:0; g_r1228_sleep.picked_wt=pt?pt->wait_type:0; }
                reschedule(st);
                if (g_signal_call_count[2] >= 2) { g_r1228_sleep.after_st=t->status; g_r1228_sleep.after_wt=t->wait_type; g_r1228_sleep.cur_after=g.current_thread_id; g_r1228_sleep.after_pc=st->pc; }
            }
        } else {
            EE_ADVANCE();
        }
        return 1;
    }
    if (sysnum == 51 || sysnum == -52) {
        /* WakeupThread(int thid) / _iWakeupThread(int thid) */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        if (thid >= 0 && thid <= EE_HLE_THREAD_MAX_THREADS) g_wakeup_call_count[thid]++; /* Round 733 - see field comment */
        ee_tcb_t *t = tcb(thid);
        EVT(cur, "event=WakeupThread target=%d target_status=0x%x target_wait_type=%d pc=0x%08x",
            thid, t ? t->status : 0, t ? t->wait_type : 0, this_pc);
        if (t && t->in_use) {
            if (t->status == EE_THS_WAIT && t->wait_type == EE_TSW_SLEEP) {
                EVT(thid, "event=status old=0x%x new=0x2 reason=WakeupThread pc=0x%08x", t->status, this_pc);
                t->status = EE_THS_READY;
                t->wait_type = EE_TSW_NONE;
                t->ready_seq = g.ready_seq_counter++;
            } else {
                t->wakeup_count++;
            }
            EE_RET(0);
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        reschedule(st);
        return 1;
    }
    if (sysnum == 53 || sysnum == -54) {
        /* CancelWakeupThread(int thid) / iCancelWakeupThread(int thid) */
        int thid = (int)(int32_t)st->gpr[4].ud0;
        ee_tcb_t *t = tcb(thid);
        if (t && t->in_use) {
            EE_RET((int32_t)t->wakeup_count);
            t->wakeup_count = 0;
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 64) {
        /* CreateSema(ee_sema_t *sema) - same field offsets as this
         * project's pre-existing g_ee_sema-based implementation
         * (task #188), for consistency: max_count@4, init_count@8,
         * attr@0x10, option@0x14. */
        uint32_t param = (uint32_t)st->gpr[4].ud0;
        int32_t max_count = (int32_t)ee_mem_read32(st, param + 4u);
        int32_t init_count = (int32_t)ee_mem_read32(st, param + 8u);
        uint32_t attr = ee_mem_read32(st, param + 0x10u);
        uint32_t option = ee_mem_read32(st, param + 0x14u);
        int slot = alloc_sema_slot();
        if (slot < 0) {
            EE_RET(-1);
        } else {
            ee_sema_internal_t *s = sema(slot);
            memset(s, 0, sizeof(*s));
            s->in_use = 1;
            s->attr = attr; s->option = option;
            s->max_count = max_count;
            s->count = init_count;
#ifdef R818_SEMA_TRACE
            fprintf(stderr, "[R818SEMA] event=CreateSema tid=%d sem=%d init_count=%d max_count=%d ra=0x%08x pc=0x%08x\n",
                    cur, slot, init_count, max_count, (uint32_t)st->gpr[31].ud0, this_pc);
#endif
            EE_RET(slot);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 65) {
        /* DeleteSema(int semid). Round 569 fix: match the original
         * handler's real-error precedent - refuse to delete (real
         * E_KERNEL_SEMA_STAT-style error) while threads are still
         * waiting, rather than silently deleting out from under a
         * waiter. */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        ee_sema_internal_t *s = sema(semid);
        if (s && s->in_use) {
            if (s->wait_threads > 0) {
                EE_RET(-419); /* real error: threads still waiting */
            } else {
                s->in_use = 0;
                EE_RET(0);
#ifdef R818_SEMA_TRACE
                fprintf(stderr, "[R818SEMA] event=DeleteSema tid=%d sem=%d ra=0x%08x pc=0x%08x\n",
                        cur, semid, (uint32_t)st->gpr[31].ud0, this_pc);
#endif
            }
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 66 || sysnum == -67) {
        /* SignalSema(int semid) / iSignalSema(int semid).
         *
         * Round 569 fix: this project's WaitSema busy-park idiom
         * (see the sysnum==68 handler above) resumes a parked thread
         * by simply re-executing the SAME WaitSema syscall and
         * re-checking s->count - it has no other channel for
         * "you were specifically signaled". The earlier version of
         * this function called wake_one_sema_waiter() FIRST and only
         * incremented count if no tracked waiter was found - a
         * "direct transfer" optimization that looks correct for a
         * real preemptive scheduler, but is fatal here: it marks the
         * waiter READY without ever bumping count, so that thread's
         * next WaitSema recheck still sees count==0 and re-parks
         * forever. This exact mismatch is what kept the diskless BIOS
         * boot baseline stuck at pmode=0x0 for the module's entire
         * test run. Fix: match the original, proven g_ee_sema[]
         * handler's semantics exactly - SignalSema ALWAYS increments
         * count (bounded by max_count); wake_one_sema_waiter() is
         * still called to eagerly flip a tracked waiter's status to
         * READY (harmless bookkeeping/an optional latency
         * optimization for reschedule()), but it no longer gates
         * whether count is incremented. */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        if (semid >= 0 && semid <= EE_HLE_THREAD_MAX_SEMAS) g_signal_call_count[semid]++; /* Round 733 - see field comment */
        ee_sema_internal_t *s = sema(semid);
        if (semid == 2) {
            g_r1225_s2_pc=this_pc; g_r1225_s2_ra=(uint32_t)st->gpr[31].ud0;
            g_r1225_s2_sys=sysnum; g_r1225_s2_tid=cur;
            g_r1225_s2_before=(s&&s->in_use)?s->count:-1;
        }
        r1193_sig_t *r1193d = NULL;
        if (g_r1193_sig_count < 2) {
            r1193d = &g_r1193_sig[g_r1193_sig_count++];
            r1193d->pc=this_pc; r1193d->ra=(uint32_t)st->gpr[31].ud0; r1193d->sysnum=sysnum; r1193d->semid=semid;
            r1193d->before=s&&s->in_use?s->count:-1; r1193d->wait_before=s&&s->in_use?s->wait_threads:-1; r1193d->after=r1193d->before; r1193d->wait_after=r1193d->wait_before; r1193d->ret=-9999;
        }
#ifdef R1103_SEMA11_TRACE
        /* Round 1103 (task #1032): narrow sema-11-only SIGNAL trace
         * per the user's exact spec - tid, pc/ra, count BEFORE, and
         * (below, after the increment) count AFTER, so a signal by
         * tid2 can be lined up instruction-for-instruction against
         * any immediately-following WAIT trace from the SAME tid
         * (Fall A) vs. tid9 (Fall B/C). */
        int32_t r1103_count_before = s ? s->count : -1;
#endif
#ifdef R1127_SEMA7_TRACE
        int32_t r1127_count_before = s ? s->count : -1;
#endif
        if (s && s->in_use) {
            if (s->count < s->max_count) {
                s->count++;
                EVT(cur, "event=SignalSema sem=%d count=%d pc=0x%08x ra=0x%08x", semid, s->count, this_pc, (uint32_t)st->gpr[31].ud0);
#ifdef R818_SEMA_TRACE
                fprintf(stderr, "[R818SEMA] event=SignalSema tid=%d sem=%d count=%d ra=0x%08x pc=0x%08x sysnum=%d\n",
                        cur, semid, s->count, (uint32_t)st->gpr[31].ud0, this_pc, sysnum);
#endif
#ifdef R1103_SEMA11_TRACE
                if (semid == 11) {
                    fprintf(stderr, "[R1103] event=SIGNAL sem=11 tid=%d ra=0x%08x pc=0x%08x count_before=%d count_after=%d sysnum=%d\n",
                            cur, (uint32_t)st->gpr[31].ud0, this_pc, r1103_count_before, s->count, sysnum);
                }
#endif
#ifdef R1127_SEMA7_TRACE
                if (semid == 7) {
                    fprintf(stderr, "[R1127SIGNAL7] tid=%d caller_ra=0x%08x pc=0x%08x count_before=%d count_after=%d sysnum=%d\n",
                            cur, (uint32_t)st->gpr[31].ud0, this_pc, s->count - 1, s->count, sysnum);
                }
#endif
                wake_one_sema_waiter(semid); /* bookkeeping only - does not gate the increment above */
                EE_RET(0);
            } else {
#ifdef R1103_SEMA11_TRACE
                if (semid == 11) {
                    fprintf(stderr, "[R1103] event=SIGNAL sem=11 tid=%d ra=0x%08x pc=0x%08x count_before=%d RESULT=OVERFLOW(-419)\n",
                            cur, (uint32_t)st->gpr[31].ud0, this_pc, r1103_count_before);
                }
#endif
#ifdef R1127_SEMA7_TRACE
                if (semid == 7) {
                    fprintf(stderr, "[R1127SIGNAL7] tid=%d caller_ra=0x%08x pc=0x%08x count_before=%d RESULT=OVERFLOW(-419)\n",
                            cur, (uint32_t)st->gpr[31].ud0, this_pc, r1127_count_before);
                }
#endif
                EE_RET(-419); /* real E_KERNEL_SEMA_OVF-style error, matches original - EE_RET already sign-extends */
            }
        } else {
            EE_RET(-1);
        }
        if (semid == 2) g_r1225_s2_after=(s&&s->in_use)?s->count:-1;
        if (r1193d) { r1193d->after=s&&s->in_use?s->count:-1; r1193d->wait_after=s&&s->in_use?s->wait_threads:-1; r1193d->ret=(int32_t)st->gpr[2].ud0; }
        EE_ADVANCE();
        if (sysnum == 66) reschedule(st); /* iSignalSema: interrupt context, defer any switch */
        return 1;
    }
    if (sysnum == 68) {
        /* WaitSema(int semid) - the exact real primitive Round 567/
         * 568 identified as this project's central remaining EE
         * architectural gap. */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        ee_sema_internal_t *s = sema(semid);
        EVT(cur, "event=WaitSema-entry sem=%d count=%d pc=0x%08x ra=0x%08x", semid, s ? s->count : -1, this_pc, (uint32_t)st->gpr[31].ud0);
        /* Round 1117 (task #929/#536, per user's explicit instruction:
         * "Patch the Producer and Patch the scheduler see how it goes
         * if it goes wrong patch it back and after that keep using the
         * decompressor") - pragmatic, NOT proven-authentic producer-
         * unblock shortcut, mirroring ee_core.c's established
         * ee_check_boot_unblock_sbus_wait() honesty convention (Round
         * 177/178/262-264 citations there).
         *
         * Evidence: a direct experiment (scratch driver
         * /tmp/r1117_force_signal.c, checkpoints r1103_seq.ckpt persisted
         * at 267,354,194 instructions with tid1/5/6/7/9 all genuinely
         * WAIT-parked on sema 4/7/9/10/11, each confirmed via
         * g_signal_call_count[]==0 to have NEVER been signaled by any
         * real SignalSema/iSignalSema call across the entire organic
         * boot) force-signaling exactly those 5 semaphore IDs once, via
         * the pre-existing ee_hle_thread_debug_signal_sema() hook,
         * produced sustained, crash-free forward progress from
         * 267,354,194 to 564,067,707+ instructions across multiple
         * distinct, legitimate kernel/OSDSYS code regions (the real
         * 0x00257964 kernel WaitSema-resume dispatcher, then genuine
         * OSDSYS module code at 0x0021xxxx/0x0026xxxx), and the
         * previously 100%-zero OSDSYS module region (0x00200000-
         * 0x00260000, confirmed all-zero at two checkpoints 3.5B+
         * instructions apart via full 64KB-bucketed RAM sweep) went
         * from fully empty to ~96% populated with real code/data. This
         * satisfies the user's own pre-committed Fix-Kandidat-1
         * criterion (a real, never-signaled semaphore with an
         * identifiable missing producer) rather than Fix-Kandidat-2
         * (scheduler losing a real wake) - g_signal_call_count[]
         * staying at 0 the entire time directly rules out the latter
         * for these specific semaphore IDs.
         *
         * Scope/safety, matching the SBUS shortcut's own discipline:
         * (a) restricted to the exact 5 semaphore IDs this experiment
         * evidenced, not semaphores in general; (b) only ever fires
         * once g_signal_call_count[semid]==0 (no real producer has
         * EVER signaled this semaphore) AND the SAME thread has
         * already been confirmed re-parked here
         * EE_ORPHAN_SEMA_REPARK_THRESHOLD additional busy-park ticks in
         * a row - i.e. it never fires on a thread's first visit,
         * giving any real producer this project does implement a full
         * chance to fire first; (c) it only ever increments s->count
         * (bounded by max_count), the exact same primitive
         * SignalSema/ee_hle_thread_debug_signal_sema use - it can never
         * fight, duplicate, or race a real signal, and a real producer
         * firing at any point makes this permanently dead code for
         * that semaphore/thread (count already >0, or
         * g_signal_call_count[] no longer 0). Per the user's own
         * fallback instruction ("if it goes wrong patch it back"): if
         * future evidence shows this causes an incoherent boot state,
         * this whole block should be reverted first before any other
         * change. */
        if (s && s->in_use && s->count == 0 &&
            (semid == 4 || semid == 7 || semid == 9 || semid == 10 || semid == 11) &&
            semid >= 0 && semid <= EE_HLE_THREAD_MAX_SEMAS &&
            g_signal_call_count[semid] == 0) {
            ee_tcb_t *r1117_t = tcb(cur);
            int r1117_already_waiting = (r1117_t && r1117_t->status == EE_THS_WAIT &&
                                          r1117_t->wait_type == EE_TSW_SEMA &&
                                          r1117_t->wait_id == (uint32_t)semid);
            if (r1117_already_waiting) {
                if (++g_orphan_sema_repark_count[semid] >= EE_ORPHAN_SEMA_REPARK_THRESHOLD) {
                    if (s->count < s->max_count) {
                        s->count++;
                        EVT(cur, "event=OrphanSemaUnblock sem=%d count=%d pc=0x%08x", semid, s->count, this_pc);
                    }
                    g_orphan_sema_repark_count[semid] = 0;
                }
            }
        }
        if (!s || !s->in_use) {
            EVT(cur, "event=WaitSema-invalid sem=%d pc=0x%08x", semid, this_pc);
            EE_RET(-1);
            EE_ADVANCE();
            return 1;
        }
        if (s->count > 0) {
#ifdef R1103_SEMA11_TRACE
            /* Round 1103 (task #1032): narrow sema-11-only WAIT-
             * success trace per the user's exact spec - tid, pc/ra,
             * count BEFORE/AFTER, and whether this thread was already
             * parked here (a busy-park re-poll finally succeeding) or
             * this is a fresh, never-blocked WaitSema that happened to
             * find count>0 immediately (the Fall-A signature: the
             * SAME tid that just signaled, consuming its own count
             * before the real older waiter ever gets a turn). */
            if (semid == 11) {
                ee_tcb_t *r1103_t = tcb(cur);
                int r1103_was_waiting = (r1103_t && r1103_t->status == EE_THS_WAIT &&
                                          r1103_t->wait_type == EE_TSW_SEMA && r1103_t->wait_id == 11);
                fprintf(stderr, "[R1103] event=WAIT sem=11 tid=%d ra=0x%08x pc=0x%08x count_before=%d count_after=%d RESULT=SUCCESS was_already_waiting=%d\n",
                        cur, (uint32_t)st->gpr[31].ud0, this_pc, s->count, s->count - 1, r1103_was_waiting);
            }
#endif
            s->count--;
            EVT(cur, "event=WaitSema-success sem=%d count=%d pc=0x%08x ra=0x%08x", semid, s->count, this_pc, (uint32_t)st->gpr[31].ud0);
            EE_RET(0);
            EE_ADVANCE();
        } else {
#ifdef R1103_SEMA11_TRACE
            if (semid == 11) {
                ee_tcb_t *r1103_t = tcb(cur);
                int r1103_was_waiting = (r1103_t && r1103_t->status == EE_THS_WAIT &&
                                          r1103_t->wait_type == EE_TSW_SEMA && r1103_t->wait_id == 11);
                fprintf(stderr, "[R1103] event=WAIT sem=11 tid=%d ra=0x%08x pc=0x%08x count_before=0 RESULT=BLOCK was_already_waiting=%d\n",
                        cur, (uint32_t)st->gpr[31].ud0, this_pc, r1103_was_waiting);
            }
#endif
            /* Round 569 fix: must NOT call EE_ADVANCE() here.
             * This project's established park idiom (see ee_core.c's
             * original sysnum==68 handler, ~line 3179) is to leave
             * pc AT this_pc (re-execute the same syscall instruction
             * next step) rather than advancing to $ra - otherwise the
             * blocked thread's saved context resumes as if WaitSema
             * had already returned successfully, without the
             * semaphore ever actually being decremented. That earlier
             * (buggy) version of this function regressed the diskless
             * BIOS boot baseline (pmode stuck at 0x0) - this is the
             * fix, verified against that same baseline below. */
            ee_tcb_t *t = tcb(cur);
            EVT(cur, "event=WaitSema-block sem=%d count=%d pc=0x%08x", semid, s->count, this_pc);
            EE_RET(0); /* pre-set: the real return value once actually woken and re-dispatched */
            st->pc = this_pc;
            st->next_pc = this_pc + 4u;
            /* Round 1102 fix (task #1031, per user's follow-up request
             * after Round 1101: "in welchem Sema stecken wir fest?"):
             * this project's own busy-park idiom (Round 569/781 comments
             * above) re-executes this SAME WaitSema syscall every time
             * the blocked thread is rescheduled back onto the CPU while
             * count stays 0 - by design, since there is no other wakeup
             * channel. But the block below (save_context/status=WAIT/
             * wait_type/wait_id) and the s->wait_threads++ that used to
             * follow it were BOTH unconditional on every such re-entry,
             * not just the first time this thread transitions from
             * RUN/READY into WAIT. Evidence: a fresh, cleanly-booted
             * SCPH-50004 diskless survey (docs/STATUS.md Round 1100/
             * 1101 entry's park, re-verified this round) showed
             * sema(wait_id=11)'s reported waiters climbing from 40 at
             * 40,000,000 cumulative instructions to 105 at 80,000,000 -
             * i.e. scaling with elapsed re-dispatch ticks of the SAME
             * single real thread (tid=9), not with any real number of
             * distinct blocked threads (this boot has nowhere near 40-
             * 105 EE threads total). Root cause: nothing here checked
             * whether the thread was ALREADY parked on this exact
             * semaphore before repeating the state-transition side
             * effects. Fixed by computing `already_waiting` from the
             * tcb's pre-mutation state and gating both the
             * status/wait_type/wait_id/ready_seq assignment AND the
             * s->wait_threads++ below on it being false - so a thread
             * that is merely being re-confirmed as still blocked (the
             * normal busy-park tick) no longer inflates the waiter
             * count or churns g.ready_seq_counter on every single
             * re-entry. This does not change WaitSema's real blocking
             * semantics (EE_RET/pc/next_pc above are unaffected and
             * still re-arm the syscall every tick as before) - it only
             * corrects the wait_threads/ready_seq bookkeeping to match
             * what a real kernel's per-thread state transition would
             * actually count. */
            int already_waiting = (t && t->status == EE_THS_WAIT &&
                                    t->wait_type == EE_TSW_SEMA &&
                                    t->wait_id == (uint32_t)semid);
#ifdef R1124_WAIT_TRACE
            /* Round 1124 (task #929/#1123 follow-up, per user's exact
             * spec): ONE-SHOT capture of the specific WaitSema(X) call
             * that genuinely blocks at the new real frontier
             * pc=0x00257964 (Round 1005/task #983, re-confirmed via
             * Round 1123's fresh cold-boot LOADFILE trace). Fires
             * exactly once, at the FIRST real RUN-to-WAIT transition
             * at this exact pc (guarded by !already_waiting so a busy-
             * park re-poll never re-fires it), and prints the exact
             * register/sema state the user asked for - pc/tid/ra/v1/
             * a0-a3 plus the target semaphore's own count/wait_threads/
             * max_count/attr - establishing X's identity directly from
             * live state. Also flags whether this specific semid is
             * one of the 5 IDs (4/7/9/10/11) Round 1117's orphan-sema-
             * producer-unblock shortcut targets, since that shortcut is
             * unconditionally compiled into this same file and could
             * be why this pc is reached at all - the user must know if
             * X falls in that set before treating this frontier as a
             * fully organic result. */
            if (this_pc == 0x00257964u) {
                static int r1124_fired = 0;
                if (!r1124_fired) {
                    r1124_fired = 1;
                    fprintf(stderr,
                        "[R1124_WAIT] FIRST GENUINE BLOCK pc=0x%08x tid=%d ra=0x%08x "
                        "v1=0x%08x a0=%d a1=0x%08x a2=0x%08x a3=0x%08x\n",
                        this_pc, cur, (uint32_t)st->gpr[31].ud0,
                        (uint32_t)st->gpr[3].ud0, semid,
                        (uint32_t)st->gpr[5].ud0, (uint32_t)st->gpr[6].ud0,
                        (uint32_t)st->gpr[7].ud0);
                    fprintf(stderr,
                        "[R1124_WAIT] sema[%d] in_use=%d count=%d max_count=%d "
                        "wait_threads=%d attr=0x%08x option=0x%08x signal_call_count=%d "
                        "is_round1117_target=%d\n",
                        semid, s->in_use, s->count, s->max_count, s->wait_threads,
                        s->attr, s->option,
                        (semid >= 0 && semid <= EE_HLE_THREAD_MAX_SEMAS) ? g_signal_call_count[semid] : -1,
                        (semid == 4 || semid == 7 || semid == 9 || semid == 10 || semid == 11) ? 1 : 0);
                }
            }
#endif
            if (t && !already_waiting) {
                EVT(cur, "event=status old=0x%x new=0x4 wait_type=SEMA wait_id=%d reason=WaitSema-block pc=0x%08x",
                    t->status, semid, this_pc);
                /* Round 826 fix (task #811, GT3 thread-1 saved-pc stale-
                 * value bug): explicitly persist this thread's OWN live
                 * context (st->pc==this_pc, the real busy-park address)
                 * into its tcb HERE, before flipping status to WAIT and
                 * calling reschedule(). Evidence: a fresh Round-824-fixed
                 * cold boot (checkpoint tool, total_instr=572,756,907)
                 * showed thread 1 persisted with saved_pc=0x0101ba08 (the
                 * jr-ra return address of the StartThread/sysnum-34
                 * syscall stub, per this round's disassembly of
                 * 0x0101B9C0-0x0101BA24) while wait_type/wait_id
                 * correctly read SEMA/5 - i.e. the tcb's saved pc was
                 * STALE (left over from an earlier, unrelated StartThread-
                 * triggered switch-out) even though wait_type/wait_id
                 * reflect a real, current WaitSema(5) block. Root cause:
                 * reschedule()'s switch-out save_context() call (Round
                 * 824, task #846) is gated on `cur->status ==
                 * EE_THS_RUN` - correct for that round's actual bug
                 * (an interrupt-context syscall corrupting an ALREADY-
                 * parked thread's saved pc), but this WaitSema-block path
                 * sets t->status = EE_THS_WAIT two lines below, BEFORE
                 * calling reschedule() just below - so by the time
                 * reschedule() reaches its save-context gate, this
                 * thread's own status already reads WAIT, not RUN, and
                 * the gate silently skips saving even though `st` right
                 * now unambiguously holds THIS thread's own live,
                 * synchronous (non-interrupt, EXL/ERL clear) context.
                 * Calling save_context() directly here, while status is
                 * still RUN, closes that gap without weakening Round
                 * 824's own fix (which remains necessary for the
                 * asynchronous/interrupt-context case reschedule() still
                 * guards against). */
                save_context(st, cur);
                t->status = EE_THS_WAIT;
                t->wait_type = EE_TSW_SEMA;
                t->wait_id = semid;
                t->ready_seq = g.ready_seq_counter++;
                s->wait_threads++; /* Round 1102: gated - see citation above; only counts a genuine RUN/READY-to-WAIT transition, not a busy-park re-poll tick */
            }
            reschedule(st);
            /* Round 781 (task #803, GT3 0x0101bc24 permanent-park
             * fix): this park re-executes the SAME WaitSema syscall
             * every step for as long as the semaphore stays at 0 -
             * exactly like the original (now-dead) ee_core.c handler
             * this scheduler superseded (Round 569). That old handler
             * called an equivalent tick sequence inline every parked
             * step specifically so real elapsed hardware time (COP0
             * Count, VBLANK, timers, DMAC/INTC interrupt delivery)
             * keeps flowing even though this thread's own PC/context
             * isn't moving - real hardware's clock does not stop just
             * because one thread is blocked. This scheduler never
             * ported that call, so entering this branch permanently
             * starved every interrupt source ee_step()'s normal
             * epilogue would otherwise drive (verified live: GT3's
             * checkpoint chain, parked here on `WaitSema(5)`, made
             * literally zero forward progress across 240M+ slices -
             * see docs/STATUS.md Round 780/781). Fix: call the same
             * real-time tick ee_core.c now exposes for exactly this
             * purpose (ee_core_park_tick(), Round 781/task #803) once
             * per parked step. */
            ee_core_park_tick(st);
        }
        return 1;
    }
    if (sysnum == 69) {
        /* PollSema(int semid) - non-blocking WaitSema.
         *
         * Round 569 fix: this project's own Round 301 live-hardware
         * finding (see ee_core.c's original sysnum==69 handler,
         * ~line 6296) proved real PollSema's success path returns the
         * SEMAPHORE ID ITSELF (v0=semid), not a flat 0 - real OSDSYS
         * code (the 0x0020D478/0x0020E830/0x002034D0 device-comm
         * helper family, which this exact module's diskless-boot test
         * run got stuck cycling through) does an equality check
         * against that specific value. The earlier version of this
         * function returned a flat 0 on success (copying WaitSema's
         * own, separately-verified convention), silently
         * reintroducing the exact bug Round 301 already fixed once in
         * the original g_ee_sema[]-based handler. */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        ee_sema_internal_t *s = sema(semid);
        if (s && s->in_use && s->count > 0) {
            s->count--;
            EE_RET(semid); /* real, live-traced: success returns the semaphore ID itself, not 0 */
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == -70) {
        /* iPollSema(int semid) - non-blocking WaitSema, interrupt-safe
         * variant. Byte-for-byte mirror of the sysnum==69 (PollSema)
         * block directly above: same sema()/g.semas[] state, same
         * "success returns the semaphore ID itself" convention.
         *
         * Round 1094 fix (task pending - see STATUS.md Round 1094):
         * before this, -70 was NOT in this function's handled[]
         * table, so every iPollSema call fell through this whole
         * dispatcher (returning 0) and was picked up instead by
         * ee_core.c's OWN old, separate g_ee_sema[]-based fallback
         * handler (the one Round 1093 patched). That fallback array
         * is genuine dead code for every OTHER semaphore syscall
         * (64/65/66/-67/68/69 are all claimed here, upstream, first)
         * because CreateSema (sysnum==64, handled above) allocates
         * and populates a slot in THIS file's g.semas[] array via
         * sema(), never in ee_core.c's g_ee_sema[]. So Round 1093's
         * iPollSema(-70) fix, while logically correct in isolation,
         * was checking a semaphore slot that real CreateSema never
         * writes to - explaining the Round 1093/1094-investigated
         * "g_ee_sema[5].in_use == 0 despite confirmed CreateSema for
         * ID 5" mystery: two separate semaphore state arrays, and
         * -70 was the one syscall number reading the wrong one.
         * Live-verified in a scratch tree against the real
         * SCPH-50004 BIOS: with -70 claimed here instead, signal_calls
         * for wid7/wid8 (previously permanently 0) become nonzero and
         * climb, and the EE program counter advances well past the
         * previously-permanent 0x00257964 resting point. */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        ee_sema_internal_t *s = sema(semid);
#ifdef R1127_SEMA7_TRACE
        {
            int32_t r1127_before = s ? s->count : -999;
            int32_t r1127_ret;
            if (s && s->in_use && s->count > 0) {
                s->count--;
                EE_RET(semid);
                r1127_ret = semid;
            } else {
                EE_RET(-1);
                r1127_ret = -1;
            }
            fprintf(stderr, "[R1127POLL] tid=%d caller_ra=0x%08x semid=%d count_before=%d ret=%d count_after=%d\n",
                    g.current_thread_id, (uint32_t)st->gpr[31].ud0, semid, r1127_before, r1127_ret,
                    s ? s->count : -999);
            EE_ADVANCE();
            return 1;
        }
#endif
        if (s && s->in_use && s->count > 0) {
            s->count--;
            EE_RET(semid); /* same real convention as PollSema(69) above */
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }

    if (sysnum == -73) {
        /* iDeleteSema(int semid) - interrupt-context fast form of the
         * already-real DeleteSema(65) above. Byte-for-byte mirror of
         * that block's semantics (real E_KERNEL_SEMA_STAT-style
         * refusal while threads still wait), operating on the SAME
         * sema()/g.semas[] state DeleteSema(65) already uses.
         *
         * Round 1097b (per user's explicit architectural-review
         * request, following Round 1093/1094's iPollSema(-70) fix):
         * before this, -73 raised a real MIPS Syscall exception (see
         * ee_core.c's sysnum==-72/-73 block) because this file's
         * handled[] table never claimed it - the exact same
         * "disconnected array" class of bug already found and fixed
         * for iPollSema(-70). Any real caller of -73 previously had
         * no citable real kernel handler this project could safely
         * reimplement (see ee_core.c's own long-standing rationale
         * for that exception-raising family) and was guaranteed to
         * either crash or take an unmodeled kernel path. Claiming it
         * here, using the same real g.semas[] state every other sema
         * syscall already shares, closes that gap with the same,
         * already-proven convention - not a new guess. */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        ee_sema_internal_t *s = sema(semid);
        if (s && s->in_use) {
            if (s->wait_threads > 0) {
                EE_RET(-419);
            } else {
                s->in_use = 0;
                EE_RET(0);
            }
        } else {
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }
    if (sysnum == 71 || sysnum == -72) {
        /* ReferSemaStatus(int semid, ee_sema_t *status) /
         * iReferSemaStatus(int semid, ee_sema_t *status) - real
         * ps2sdk signature (kernel.h). Writes the semaphore's live
         * kernel-resident status struct to the caller-supplied
         * buffer, byte-for-byte the same accessor pattern as
         * ReferThreadStatus (sysnum 48/-49) directly above. Real
         * ee_sema_t layout (matches this file's own CreateSema
         * input-struct offsets above): count@0, max_count@4,
         * init_count@8, wait_threads@0xC, attr@0x10, option@0x14.
         *
         * Round 1097b: same rationale as the -73 block above - closes
         * the last remaining semaphore-family syscall that previously
         * raised a real, unimplemented-kernel-state exception instead
         * of using this file's own real g.semas[] state (see
         * ee_core.c's sysnum==59/62/71/84/89/90/91/105 block, which
         * this syscall was bundled into before this fix).
         *
         * Known, deliberate fidelity gap (documented, not silently
         * guessed): ee_sema_internal_t does not separately retain a
         * semaphore's ORIGINAL init_count once count starts changing
         * (only max_count/count/wait_threads/attr/option are tracked -
         * see the struct definition above). Adding a new field would
         * grow sizeof(g) - the exact blob checkpoint.c compares
         * byte-for-byte against a persisted checkpoint's recorded
         * size before restoring EE-thread/sema state (see
         * checkpoint.c's `if (blob_cap == eeth_size)` load guard) -
         * silently zeroing the user's already-persisted GT3
         * checkpoint's sema/thread state on next load (a size
         * mismatch skips the restore entirely rather than erroring).
         * To preserve that checkpoint's integrity, this reports
         * init_count == max_count, matching every real CreateSema
         * call site this project has ever traced (every observed real
         * semaphore is created with init_count==max_count - binary or
         * counting semaphores started "full"). If a future round ever
         * traces a real semaphore created with init_count !=
         * max_count, this approximation must be revisited (and the
         * checkpoint-format break accepted deliberately, with a
         * version bump, at that point - not silently). */
        int semid = (int)(int32_t)st->gpr[4].ud0;
        uint32_t out_ptr = (uint32_t)st->gpr[5].ud0;
        ee_sema_internal_t *s = sema(semid);
        if (s && s->in_use) {
            ee_mem_write32(st, out_ptr + 0x00u, (uint32_t)s->count);
            ee_mem_write32(st, out_ptr + 0x04u, (uint32_t)s->max_count);
            ee_mem_write32(st, out_ptr + 0x08u, (uint32_t)s->max_count); /* init_count approximation - see comment above */
            ee_mem_write32(st, out_ptr + 0x0Cu, (uint32_t)s->wait_threads);
            ee_mem_write32(st, out_ptr + 0x10u, s->attr);
            ee_mem_write32(st, out_ptr + 0x14u, s->option);
#ifdef R1127_SEMA7_TRACE
            fprintf(stderr, "[R1127REFER] tid=%d caller_ra=0x%08x semid=%d out_ptr=0x%08x count=%d max_count=%d init_count=%d wait_threads=%d attr=0x%x option=0x%x\n",
                    g.current_thread_id, (uint32_t)st->gpr[31].ud0, semid, out_ptr,
                    s->count, s->max_count, s->max_count, s->wait_threads, s->attr, s->option);
#endif
            EE_RET(0);
        } else {
#ifdef R1127_SEMA7_TRACE
            fprintf(stderr, "[R1127REFER] tid=%d caller_ra=0x%08x semid=%d out_ptr=0x%08x FAIL-not-in-use\n",
                    g.current_thread_id, (uint32_t)st->gpr[31].ud0, semid, out_ptr);
#endif
            EE_RET(-1);
        }
        EE_ADVANCE();
        return 1;
    }

    /* Recognized in the membership table above but not yet given a
     * concrete body (should not happen - every entry has a matching
     * block above); fail safe rather than silently mis-dispatch. */
    return 0;
#undef EE_RET
#undef EE_ADVANCE
#undef EE_ADVANCE
}

/* Round 782 (task #805): see ee_hle_thread.h's declaration comment for
 * the full citation/rationale. Exact mirror of the live sysnum==36
 * (ExitDeleteThread) handler body above (lines ~470-478), exposed for
 * ee_core.c's null-jalr guard to call directly rather than duplicating
 * this logic. */
void ee_hle_thread_exit_current(ee_state_t *st)
{
    int cur = g.current_thread_id;
    if (cur) {
        tcb(cur)->status = EE_THS_DORMANT;
        tcb(cur)->in_use = 0;
    }
    reschedule(st);
}

int ee_hle_thread_get_thread_count(void) { return g.thread_count; }

/* Round 855 (task #855): public wrapper so ee_core.c's ee_step() can
 * re-invoke the real scheduler while st->idle is set (after running
 * a real hardware tick), without exposing reschedule()/pick_next_
 * ready()'s internals. Idempotent: if still nothing is ready, this
 * just re-sets st->idle (already 1, a no-op); if something became
 * ready (e.g. a real interrupt-context WakeupThread/SignalSema call
 * during the tick), it performs the real context switch and clears
 * st->idle itself - see reschedule()'s own comments for both paths. */
void ee_hle_thread_reschedule_kick(ee_state_t *st)
{
    reschedule(st);
}
int ee_hle_thread_get_current_thread_id(void) { return g.current_thread_id; }
uint32_t ee_hle_thread_get_status(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? t->status : 0u;
}
uint32_t ee_hle_thread_get_priority(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? t->priority : 0u;
}
/* Round 612 (task #536): see header comment - read-only accessors for
 * the TCB's own already-tracked wait_type/wait_id fields. */
uint32_t ee_hle_thread_get_wait_type(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? (uint32_t)t->wait_type : 0u;
}
uint32_t ee_hle_thread_get_wait_id(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? (uint32_t)t->wait_id : 0u;
}

/* Round 613 (task #536): see header comment - expose entry/pc/wakeup_count
 * for host-native diagnostic drivers, to identify which real BIOS
 * function each parked thread belongs to. */
uint32_t ee_hle_thread_get_entry(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? t->entry : 0u;
}
uint32_t ee_hle_thread_get_saved_pc(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? t->pc : 0u;
}
uint32_t ee_hle_thread_get_wakeup_count(int thid)
{
    ee_tcb_t *t = tcb(thid);
    return t ? t->wakeup_count : 0u;
}

/* Round 811 (task #811, backward-trace of GT3's WaitSema(5) call
 * site per user's explicit request): expose a parked thread's full
 * saved GPR context (this project's context-switch already stores
 * all 32 real EE GPRs per-thread - see save_context()/ee_tcb_t.gpr
 * above - there was just no accessor exposing it to host-native
 * diagnostic drivers, unlike entry/saved_pc/wakeup_count above).
 * reg is a raw EE register index (0=$zero..31=$ra); returns the
 * low 64 bits (ud0) of that register's saved 128-bit value, which is
 * sufficient for every real EE ABI use of $a0-$a3/$v0/$v1/$ra/$sp/$gp
 * (none of which use the upper 64 bits of a 128-bit GPR in normal
 * calling-convention code). */
uint64_t ee_hle_thread_get_gpr(int thid, int reg)
{
    ee_tcb_t *t = tcb(thid);
    if (!t || reg < 0 || reg > 31) return 0u;
    return t->gpr[reg].ud0;
}

/* Round 597 (task #447/#536, following Round 596's finding): forced
 * preemption. This project's reschedule() is otherwise only invoked
 * from specific HLE syscall handlers above (StartThread/WakeupThread/
 * SleepThread/ChangeThreadPriority/RotateThreadReadyQueue/thread-exit/
 * SignalSema/WaitSema) - so a thread that never itself calls one of
 * those specific syscalls can starve a higher-priority READY thread
 * indefinitely, even after that thread has been made READY and even
 * signaled via a real WakeupThread() call. Round 596 found exactly
 * this: OSDSYS's real disc-browser dispatcher thread (entry=
 * 0x00204308, identified by disassembly) sits READY with a real,
 * better kernel priority than the currently-RUNNING animation-loop
 * thread, already woken (wakeup_count=1 at the point of discovery),
 * but never actually scheduled because nothing re-checks the ready
 * queue between syscalls. Real EE hardware avoids this via kernel-
 * level forced preemption on interrupt return (the real kernel's
 * exception/interrupt-return path always re-checks the ready queue
 * before restoring context) - this project's own C-level HLE
 * scheduler (Round 569) never had an equivalent, since it only ever
 * reacts to the specific syscalls above.
 *
 * Called once per genuine instruction boundary from ee_core.c's
 * ee_step(), in the exact same `if (!st->branch_pending)` block and
 * calling convention already used for ee_check_timer_interrupt()/
 * ee_check_intc_interrupt()/ee_check_dmac_interrupt() - a cheap
 * O(thread_count) scan (thread_count capped at
 * EE_HLE_THREAD_MAX_THREADS==32) that is a no-op until this project's
 * own scheduler has been engaged at all (thread_count==0, matching
 * every other check function's existing no-op-until-armed
 * convention).
 *
 * Deliberately conservative: only switches when the best real ready
 * priority is STRICTLY better (numerically lower) than the currently-
 * RUNNING thread's own priority - never merely because a same-or-
 * lower-priority thread is ready, so FIFO ordering among equal-
 * priority threads (pick_next_ready()'s own tiebreak) is never
 * disturbed by this function. This mirrors a real priority-preemptive
 * kernel's exact behavior (strictly-higher-priority-preempts, ties
 * don't), just checked far more frequently than real hardware's own
 * timer-tick granularity - an intentional, honest simplification
 * given this project has no cycle-accurate timing model (same
 * established precedent as e.g. ee_step()'s own COP0 Count-advances-
 * by-1-per-instruction comment immediately above in ee_core.c). */
void ee_hle_thread_check_preempt(ee_state_t *st)
{
    /* Round 598 (task #447/#536 follow-up, real regression fix): a real,
     * confirmed-via-user-supplied-disc regression was found in Round 597's
     * original version of this function - it had no guard against
     * preempting while the CPU is genuinely mid-hardware-exception
     * (COP0 Status.EXL or Status.ERL set). ee_check_timer_interrupt()/
     * ee_check_intc_interrupt()/ee_check_dmac_interrupt() immediately
     * above this function's own call site in ee_core.c's ee_step() all
     * correctly refuse to raise a NEW interrupt while EXL/ERL is set
     * (real MIPS semantics - exceptions do not nest by default), but
     * this function had no equivalent check, so it could - and, per
     * Round 598's host-native evidence against the real Tekken Tag
     * Tournament (Europe) (Demo) disc image, DID - swap out the "current
     * thread"'s live register file (including pc) while that thread's
     * CPU state actually belonged to a real EE interrupt handler
     * (Status.EXL=1, COP0 EPC holding the real interrupted return
     * address). A real interrupt-return sequence (ERET) assumes it will
     * resume exactly the context that was live when the exception was
     * taken; this function silently substituting a DIFFERENT thread's
     * saved context in between corrupts that assumption - the newly-
     * loaded thread's code then runs with EXL still set (interrupts
     * effectively masked) and no correct path back to the original
     * caller, which is exactly the symptom Round 598 measured: the real
     * Tekken disc-boot animation thread (tid 3) permanently trapped
     * cycling through EE kernel interrupt-dispatch code
     * (0x8000CC00-0x8000FA00) with zero further VU1/GS activity for
     * over a billion further instructions, instead of baseline's
     * (pre-Round-597) behavior of continuing to render real frames.
     * The fix mirrors the exact same EXL/ERL gating convention already
     * used by this file's three sibling interrupt-check functions -
     * simply refuse to switch threads while genuinely inside a hardware
     * exception; the CPU will still be re-checked on every subsequent
     * instruction boundary once ERET clears EXL, so the disc-browser-
     * dispatcher-starvation fix Round 597 was written for is preserved
     * (see this file's own header/definition comments for that
     * original rationale), just no longer firing during the one window
     * where doing so is unsafe. */
    if (st->cop0[12] & 0x6u) return; /* Status.EXL (bit 1) or Status.ERL (bit 2) set - genuinely mid-exception, never preempt here */
    /* Round 625 (task #536/#607, following Round 624's live-hardware-
     * verified findings): never freeze a thread's context while its PC
     * sits inside the real, fixed generic ERET-glue thread-start
     * trampoline - the exact 5-instruction body this project has fully
     * disassembled and byte-verified twice, independently, against the
     * real BIOS ROM (Round 467's "0x00081FE0 is a fixed generic kernel
     * thread-start trampoline; real entry point travels through $v1,
     * not $a0" finding; Round 619's byte-exact confirmation:
     *   0x00081FE0: lui   $sp, 8
     *   0x00081FE4: jalr  $v1        ; delay slot below
     *   0x00081FE8: addiu $sp, $sp, 0x1fc0
     *   0x00081FEC: addiu $v1, $zero, -5
     *   0x00081FF0: syscall
     * ). $v1 here is a real MIPS calling-convention register: the
     * trampoline's ONLY caller-supplied argument, which must already
     * hold a valid target address by the time control reaches
     * 0x00081FE0 - the trampoline body itself performs no load, no
     * null-check, and no guard of any kind before `jalr $v1` (Round
     * 619's own conclusion, quoted in STATUS.md: "There is no branch,
     * no zero-check, no guard of any kind between the trampoline's
     * entry and the jalr $v1"). Real EE hardware never has a problem
     * with this: an interrupt landing here is always followed by a
     * full, faithful, immediate context restore on return, so $v1
     * (already set by the real caller a few instructions earlier)
     * survives untouched. This project's OWN software scheduler is
     * different in one specific, consequential way - once
     * save_context() freezes a thread here, nothing guarantees WHEN
     * (or whether, for a very long time) load_context() will resume
     * it, unlike real hardware's effectively-instantaneous interrupt
     * round-trip. Round 622's own live-instrumented conclusion
     * ("$ra=0x800027AC... a leftover register value from BEFORE the
     * 2,000,000-slice observation window... set by a real call more
     * than ~154,500,000 slices earlier and never overwritten since")
     * and Round 624's direct RAM comparison (TIMER3's real per-cause
     * handler-table slot is genuinely unregistered/count=0, and the
     * real 0x80001630 dispatcher's own intact blez-guard would never
     * reach this trampoline for it at all) together rule out "the real
     * BIOS dispatch code is buggy" as the explanation - the evidence
     * instead points at exactly this window: this project's own forced
     * preemption (introduced Round 597, moved to fire only on real
     * ERET in Round 598) can still freeze whichever thread happens to
     * be mid-handoff here, for however long its priority keeps it off
     * the ready queue, and later resume it with $v1 stale/zero. The
     * fix is narrow and conservative - defer preemption by the few
     * instructions it takes the CPU to clear this window on its own,
     * exactly the same "genuinely unsafe software-yield point, unlike
     * real hardware which needs no such carve-out" reasoning as the
     * Status.EXL/ERL check immediately above. */
    if (st->pc >= 0x00081FE0u && st->pc < 0x00081FF0u) return;
    if (g.thread_count == 0 || g.current_thread_id == 0) return;
    ee_tcb_t *cur = tcb(g.current_thread_id);
    if (!cur || cur->status != EE_THS_RUN) return;
    int best = pick_next_ready();
    if (best == 0 || best == g.current_thread_id) return;
    ee_tcb_t *bt = tcb(best);
    if (bt && bt->priority < cur->priority) reschedule(st);
}
