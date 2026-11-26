#include <time.h>
/* GPL-3.0-or-later. Included at the end of gif.c (GEKKO2_WIIU only): optional GS/GIF worker thread.
 *
 * Producers (VU1 XGKICK, VIF DIRECT, GIF DMA, CPU FIFO) copy their quadwords into one SPSC ring per GIF
 * path and return immediately. The worker runs the unchanged synchronous GIF code (the *_sync functions)
 * on the oldest submitted entry that can make progress, so path arbitration, SIGNAL back-pressure and the
 * parser keep their existing logic. The emulation thread drains the worker before it observes GS state. */
#undef gif_try_process_quadwords
#undef gif_process_quadwords
#undef gif_process_xgkick
#undef gif_begin_xgkick
#undef gif_end_xgkick
#undef gif_process_xgkick_fragment
#undef gif_set_path3_mask
#undef gif_fifo_service
#undef gif_fifo_write
#undef gif_mmio_read32
#undef gif_mmio_write32
#undef gif_read_local_fifo

enum { GA_TRY, GA_PROC, GA_FRAG, GA_BEGIN, GA_END, GA_MASK, GA_FIFOW };
#define GA_SLOTS 131072u /* 2 MiB per path (16-byte slots); power of two */
#define GA_MASK_IDX (GA_SLOTS - 1u)
#define GA_MAXE 16384u   /* longest single entry in quadwords */
#define GA_SPIN_LIMIT 400000u

typedef struct { volatile uint32_t w, r; } ga_ring_t;
typedef struct { int loaded; uint32_t seq, kind, qwc, arg, consumed; } ga_cur_t;

volatile int gs_async_on;
static uint8_t ga_mem[3][GA_SLOTS][16];
static uint8_t ga_buf[3][GA_MAXE * 16u];
static ga_ring_t ga_ring[3];
static ga_cur_t ga_cur[3];
static uint32_t ga_seq;                       /* producers: emulation thread (PATH2/3) and the VU1 worker (PATH1) */
static volatile uint32_t ga_push_cnt;         /* emulation thread */
static volatile uint32_t ga_done_cnt;         /* worker */
static volatile uint32_t ga_epoch, ga_stall_epoch;
static volatile int ga_stop, ga_lock_flag, ga_intc_pending, ga_running, ga_started;
static void (*ga_idle)(void);
static uint64_t ga_stat[8];
static volatile uint64_t ga_busy_ns;
static inline uint64_t ga_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;}

static inline void ga_wait(void) { if (ga_idle) ga_idle(); }

void gs_async_lock(void)
{
    while (__atomic_exchange_n(&ga_lock_flag, 1, __ATOMIC_ACQUIRE)) ga_wait();
}
void gs_async_unlock(void) { __atomic_store_n(&ga_lock_flag, 0, __ATOMIC_RELEASE); }
void gs_async_touch(void) { __atomic_add_fetch(&ga_epoch, 1u, __ATOMIC_ACQ_REL); }

void gs_async_pump(void)
{
    if (__atomic_load_n(&ga_intc_pending, __ATOMIC_ACQUIRE) && __atomic_exchange_n(&ga_intc_pending, 0, __ATOMIC_ACQ_REL))
        ee_intc_raise(0);
}

void gs_async_drain(void)
{
    extern volatile int gekko2_wiiu_exiting;
    if (!gs_async_on || gekko2_wiiu_exiting) return;
    if (__atomic_load_n(&ga_done_cnt, __ATOMIC_ACQUIRE) == ga_push_cnt) { gs_async_pump(); return; }
    ga_stat[2]++;
    const uint64_t t0 = ga_now();
    while (!gekko2_wiiu_exiting &&
           __atomic_load_n(&ga_done_cnt, __ATOMIC_ACQUIRE) != ga_push_cnt &&
           __atomic_load_n(&ga_stall_epoch, __ATOMIC_ACQUIRE) != __atomic_load_n(&ga_epoch, __ATOMIC_ACQUIRE))
        ga_wait();
    ga_stat[5] += ga_now() - t0;
    gs_async_pump();
}

/* ---- worker ---- */
static int ga_load(unsigned p)
{
    ga_cur_t *c = &ga_cur[p];
    if (c->loaded) return 1;
    ga_ring_t *r = &ga_ring[p];
    const uint32_t rd = r->r;
    if (__atomic_load_n(&r->w, __ATOMIC_ACQUIRE) == rd) return 0;
    uint32_t h[4];
    memcpy(h, ga_mem[p][rd & GA_MASK_IDX], 16);
    c->seq = h[0]; c->kind = h[1]; c->qwc = h[2]; c->arg = h[3]; c->consumed = 0;
    if (c->qwc) {
        const uint32_t first = (rd + 1u) & GA_MASK_IDX;
        const uint32_t n1 = c->qwc < GA_SLOTS - first ? c->qwc : GA_SLOTS - first;
        memcpy(ga_buf[p], ga_mem[p][first], n1 * 16u);
        if (n1 < c->qwc) memcpy(ga_buf[p] + n1 * 16u, ga_mem[p][0], (c->qwc - n1) * 16u);
    }
    c->loaded = 1;
    return 1;
}

static int ga_exec(unsigned p)
{
    ga_cur_t *c = &ga_cur[p];
    int finished = 1, progress = 1;
    switch (c->kind) {
    case GA_TRY: {
        const uint32_t done = gif_try_process_quadwords_sync((int)p, ga_buf[p] + c->consumed * 16u, c->qwc - c->consumed);
        c->consumed += done;
        progress = done != 0;
        finished = c->consumed == c->qwc;
    } break;
    case GA_PROC: gif_process_quadwords_sync((int)p, ga_buf[p], c->qwc); break;
    case GA_FRAG: gif_process_xgkick_fragment_sync(ga_buf[p], c->qwc); break;
    case GA_BEGIN: gif_begin_xgkick_sync(); break;
    case GA_END: gif_end_xgkick_sync(); break;
    case GA_MASK: gif_set_path3_mask_sync((int)c->arg); break;
    case GA_FIFOW: if (!gif_fifo_write_sync(ga_buf[p])) { progress = 0; finished = 0; } break;
    default: break;
    }
    if (finished && progress) {
        ga_ring_t *r = &ga_ring[p];
        __atomic_store_n(&r->r, r->r + 1u + c->qwc, __ATOMIC_RELEASE);
        c->loaded = 0;
        __atomic_add_fetch(&ga_done_cnt, 1u, __ATOMIC_ACQ_REL);
    }
    return progress;
}

static int ga_pass(void)
{
    int any = 0;
    for (;;) {
        int cand[3], nc = 0;
        for (unsigned p = 0; p < 3u; p++) if (ga_load(p)) cand[nc++] = (int)p;
        for (int i = 1; i < nc; i++)
            for (int j = i; j > 0 && (int32_t)(ga_cur[cand[j]].seq - ga_cur[cand[j - 1]].seq) < 0; j--) {
                int t = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = t;
            }
        int prog = 0;
        for (int i = 0; i < nc && !prog; i++) {
            gs_async_lock();
            prog = ga_exec((unsigned)cand[i]);
            gs_async_unlock();
        }
        if (!prog) break;
        any = 1;
    }
    if (g_gif.fifo_count) {
        gs_async_lock();
        const uint8_t before = g_gif.fifo_count;
        gif_fifo_service_sync();
        if (g_gif.fifo_count != before) any = 1;
        gs_async_unlock();
    }
    return any;
}

/* Optional: set by the app. Idle workers call it after many empty polls instead of spinning, so an emulator such
 * as Cemu (or a busy core) does not lose time to a polling thread. */
void (*gekko2_worker_nap)(void);
#ifdef GEKKO2_WIIU
/* Wii U: when the GS worker runs on the main thread (GPU renderer on its own core), the front end keeps ProcUI,
 * controllers, picture output and GX2 requests alive through this hook, called once per worker loop. */
void (*gekko2_worker_tick)(void);
#endif
void gs_async_worker_main(void)
{
    __atomic_store_n(&ga_started, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&ga_running, 1, __ATOMIC_RELEASE);
    extern volatile int gekko2_wiiu_exiting;
    unsigned idle = 0;
    while (!__atomic_load_n(&ga_stop, __ATOMIC_ACQUIRE) && !gekko2_wiiu_exiting) {
#ifdef GEKKO2_WIIU
        if (gekko2_worker_tick) gekko2_worker_tick();
        if (gekko2_wiiu_exiting) break;
#endif
        const uint32_t e = __atomic_load_n(&ga_epoch, __ATOMIC_ACQUIRE);
        const uint64_t b0 = ga_now();
        const int any = ga_pass();
        if (any) { ga_busy_ns += ga_now() - b0; idle = 0; }
        if (!any) {
            __atomic_store_n(&ga_stall_epoch, e, __ATOMIC_RELEASE);
            if (++idle > 512u && gekko2_worker_nap) gekko2_worker_nap(); else ga_wait();
        }
    }
    __atomic_store_n(&ga_running, 0, __ATOMIC_RELEASE);
}

void gs_async_attach(void (*idle)(void))
{
    ga_idle = idle;
    memset(ga_ring, 0, sizeof(ga_ring));
    memset(ga_cur, 0, sizeof(ga_cur));
    memset(ga_stat, 0, sizeof(ga_stat));
    ga_seq = 0; ga_push_cnt = 0; ga_done_cnt = 0; ga_epoch = 1; ga_stall_epoch = 0;
    ga_busy_ns = 0; ga_stop = 0; ga_lock_flag = 0; ga_intc_pending = 0; ga_started = 0; ga_running = 0;
    gs_async_on = 0;
}

/* After the platform started gs_async_worker_main(): wait until it runs, then route GIF work to it. */
int gs_async_enable(void)
{
    for (unsigned i = 0; i < 2000000u && !__atomic_load_n(&ga_running, __ATOMIC_ACQUIRE); i++) ga_wait();
    if (!__atomic_load_n(&ga_running, __ATOMIC_ACQUIRE)) return -1;
    gs_async_on = 1;
    return 0;
}

void gs_async_detach(void)
{
    if (!gs_async_on) { __atomic_store_n(&ga_stop, 1, __ATOMIC_RELEASE); return; }
    gs_async_drain();
    __atomic_store_n(&ga_stop, 1, __ATOMIC_RELEASE);
    if (__atomic_load_n(&ga_started, __ATOMIC_ACQUIRE))
        while (__atomic_load_n(&ga_running, __ATOMIC_ACQUIRE)) ga_wait();
    gs_async_on = 0;
    gs_async_pump();
}

void gs_async_stats(uint64_t out[8]) { for (unsigned i = 0; i < 8u; i++) out[i] = ga_stat[i]; out[6] = ga_busy_ns; }

/* ---- emulation-thread producers ---- */
static uint32_t ga_push(unsigned kind, unsigned path, const uint8_t *data, uint32_t qwc, uint32_t arg, int partial)
{
    extern volatile int gekko2_wiiu_exiting;
    if (gekko2_wiiu_exiting) return qwc;
    ga_ring_t *r = &ga_ring[path];
    uint32_t total = 0;
    do {
        uint32_t take = qwc - total < GA_MAXE ? qwc - total : GA_MAXE;
        const uint32_t w = r->w;
        uint32_t spins = 0;
        for (;;) {
            if (gekko2_wiiu_exiting) return qwc;
            const uint32_t rd = __atomic_load_n(&r->r, __ATOMIC_ACQUIRE);
            const uint32_t room = GA_SLOTS - (w - rd);
            if (room >= 1u + take) break;
            if (partial) {
                if (room >= 2u) { take = room - 1u; break; }
                return total;                      /* ring full: caller retries, exactly like a declined sync call */
            }
            ga_stat[4]++;
            if (++spins > GA_SPIN_LIMIT) return 0xFFFFFFFFu; /* caller falls back to a synchronous call */
            ga_wait();
        }
        uint32_t h[4] = { __atomic_fetch_add(&ga_seq, 1u, __ATOMIC_RELAXED), kind, take, arg };
        memcpy(ga_mem[path][w & GA_MASK_IDX], h, 16);
        if (take) {
            const uint32_t first = (w + 1u) & GA_MASK_IDX;
            const uint32_t n1 = take < GA_SLOTS - first ? take : GA_SLOTS - first;
            memcpy(ga_mem[path][first], data + (size_t)total * 16u, (size_t)n1 * 16u);
            if (n1 < take) memcpy(ga_mem[path][0], data + (size_t)(total + n1) * 16u, (size_t)(take - n1) * 16u);
        }
        __atomic_store_n(&r->w, w + 1u + take, __ATOMIC_RELEASE);
        ga_push_cnt++;
        __atomic_add_fetch(&ga_epoch, 1u, __ATOMIC_ACQ_REL);
        ga_stat[0]++; ga_stat[1] += take;
        total += take;
        if (partial) break;
    } while (total < qwc);
    return total;
}

/* A must-complete submission that could not be queued: finish it synchronously on this thread. */
#define GA_FALLBACK(call) do { ga_stat[3]++; gs_async_drain(); gs_async_lock(); call; gs_async_unlock(); gs_async_touch(); } while (0)

uint32_t gif_try_process_quadwords(int path, const uint8_t *data, uint32_t qwc)
{
    if (!gs_async_on || path < 0 || path > 2 || !data || !qwc) return gif_try_process_quadwords_sync(path, data, qwc);
    const uint32_t n = ga_push(GA_TRY, (unsigned)path, data, qwc, 0, 1);
    return n == 0xFFFFFFFFu ? 0u : n;
}
void gif_process_quadwords(int channel, const uint8_t *data, uint32_t qwc)
{
    const unsigned p = (channel == GIF_PATH_1 || channel == GIF_PATH_2 || channel == GIF_PATH_3) ? (unsigned)channel : GIF_PATH_3;
    if (!gs_async_on || !data || !qwc) { gif_process_quadwords_sync(channel, data, qwc); return; }
    if (ga_push(GA_PROC, p, data, qwc, 0, 0) == 0xFFFFFFFFu) GA_FALLBACK(gif_process_quadwords_sync(channel, data, qwc));
}
void gif_process_xgkick_fragment(const uint8_t *data, uint32_t qwc)
{
    if (!gs_async_on || !data || !qwc) { gif_process_xgkick_fragment_sync(data, qwc); return; }
    if (ga_push(GA_FRAG, GIF_PATH_1, data, qwc, 0, 0) == 0xFFFFFFFFu) GA_FALLBACK(gif_process_xgkick_fragment_sync(data, qwc));
}
void gif_begin_xgkick(void)
{
    if (!gs_async_on) { gif_begin_xgkick_sync(); return; }
    if (ga_push(GA_BEGIN, GIF_PATH_1, NULL, 0, 0, 0) == 0xFFFFFFFFu) GA_FALLBACK(gif_begin_xgkick_sync());
}
void gif_end_xgkick(void)
{
    if (!gs_async_on) { gif_end_xgkick_sync(); return; }
    if (ga_push(GA_END, GIF_PATH_1, NULL, 0, 0, 0) == 0xFFFFFFFFu) GA_FALLBACK(gif_end_xgkick_sync());
}
void gif_process_xgkick(const uint8_t *data, uint32_t qwc)
{
    gif_begin_xgkick();
    gif_process_xgkick_fragment(data, qwc);
    gif_end_xgkick();
}
void gif_set_path3_mask(int masked)
{
    if (!gs_async_on) { gif_set_path3_mask_sync(masked); return; }
    if (ga_push(GA_MASK, GIF_PATH_3, NULL, 0, (uint32_t)(masked != 0), 0) == 0xFFFFFFFFu) GA_FALLBACK(gif_set_path3_mask_sync(masked));
}
void gif_fifo_service(void)
{
    if (!gs_async_on) gif_fifo_service_sync(); /* the worker services the CPU FIFO itself */
}
int gif_fifo_write(const uint8_t *quadword)
{
    if (!gs_async_on) return gif_fifo_write_sync(quadword);
    const uint32_t n = ga_push(GA_FIFOW, GIF_PATH_3, quadword, 1, 0, 1);
    return n == 1u;
}
int gif_mmio_read32(uint32_t addr, uint32_t *out_val)
{
    if (!gs_async_on) return gif_mmio_read32_sync(addr, out_val);
    return gif_mmio_read32_sync(addr, out_val); /* status reads may lag behind queued work, as on hardware */
}
int gif_mmio_write32(uint32_t addr, uint32_t val)
{
    if (addr != GIF_CTRL_ADDR && addr != GIF_MODE_ADDR) return 0;
    if (!gs_async_on) return gif_mmio_write32_sync(addr, val);
    gs_async_drain(); gs_async_lock();
    const int r = gif_mmio_write32_sync(addr, val);
    gs_async_unlock(); gs_async_touch();
    return r;
}
uint32_t gif_read_local_fifo(uint8_t *out, uint32_t qwc)
{
    if (!gs_async_on) return gif_read_local_fifo_sync(out, qwc);
    gs_async_drain(); gs_async_lock();
    const uint32_t r = gif_read_local_fifo_sync(out, qwc);
    gs_async_unlock();
    return r;
}

static void gif_raise_gs_intc(void)
{
    if (gs_async_on) __atomic_store_n(&ga_intc_pending, 1, __ATOMIC_RELEASE);
    else ee_intc_raise(0);
}
