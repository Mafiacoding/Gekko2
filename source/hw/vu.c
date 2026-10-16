#include "core/hw/game_trace.h"
/*
 * vu.c - see include/core/hw/vu.h for scope notes and references, and
 * source/hw/vu_opcodes.h for the real VU instruction encoding this
 * round (task #94) added.
 */

#include "core/hw/vu.h"
#include "core/hw/vu_math.h"
#include "core/hw/vu_pair.h"
#include "core/hw/gif.h"
#include "core/hw/vif.h"
#include "core/hw/ee_intc.h"
#include "core/ee/ee_core.h"
#include "vu_opcodes.h"
#include "core/recompiler/vu_jit.h"
#include "core/recompiler/optimization.h"
#include <string.h>
#include <math.h>

static vu1_state_t g_vu1;
#ifdef GEKKO2_GAME_TRACE
static uint32_t unknown_words[8][2];
static unsigned unknown_used;
#endif
const vu1_state_t * const vu1_scheduler_view = &g_vu1;
static void vu1_service_xgkick(int flush);
static void vu1_issue_xgkick(uint32_t addr);

void vu1_init(void)
{
    memset(&g_vu1, 0, sizeof(g_vu1));
#ifdef GEKKO2_GAME_TRACE
    unknown_used=0;
#endif
    /* VF00 is hardwired to (0,0,0,1.0f) on real hardware, matching
     * this project's existing VU0 macro-mode convention (ee_core.c's
     * vu0_vf_read_lane) - see docs/STATUS.md's "round 13" section for
     * the same fact cited there. Stored directly here (rather than
     * via a read-time special case like VU0's helpers) since VU1
     * micro mode has no other read path yet to route through. */
    g_vu1.vf[0][3] = 0x3F800000u; /* float bit pattern of 1.0f */
}

vu1_state_t *vu1_get_state(void) { return &g_vu1; }

static inline uint32_t vu_rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void vu_wr_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

void vu1_micro_write32(uint32_t addr, uint32_t value)
{
    uint32_t off = addr & (VU1_MICRO_SIZE - 1u);
    vu_wr_le32(g_vu1.micro + off, value);
}

/* VU1 local DATA memory (g_vu1.mem, distinct from micro[] above) -
 * called from vif.c's VIF1 UNPACK handling (task: "VIF UNPACK"). */
void vu1_mem_write32(uint32_t addr, uint32_t value)
{
    uint32_t off = addr & (VU1_MEM_SIZE - 1u);
    vu_wr_le32(g_vu1.mem + off, value);
}

/* raw-bits <-> float helpers (VF lanes are stored as raw bit
 * patterns throughout this project, matching ee_core.c's own
 * convention) */
static inline float vu_f(uint32_t bits) { union { uint32_t u; float f; } c; c.u = bits; return c.f; }
static inline uint32_t vu_u(float f)    { union { uint32_t u; float f; } c; c.f = f; return c.u; }

static uint32_t vu_scalar_bits(uint32_t bits) {
    unsigned exp=bits&0x7f800000u;
    if(!exp)return bits&0x80000000u;
    if(exp==0x7f800000u)return (bits&0x80000000u)|0x7f7fffffu;
    return bits;
}
static void vu_q_compute(uint32_t vf[32][4],uint32_t *vi,uint32_t w) {
    unsigned op=w&3u,fs=(w>>11)&31u,ft=(w>>16)&31u;
    uint32_t a=vu_scalar_bits(vf[fs][(w>>21)&3u]);
    uint32_t b=vu_scalar_bits(vf[ft][(w>>23)&3u]);
    float x=vu_f(a),y=vu_f(b);uint32_t flags=0,result;
    if(op==1u) {
        if(y<0)flags=0x10u;
        result=vu_scalar_bits(vu_u(sqrtf(fabsf(y))));
    } else if(y==0) {
        if(op==0u) {
            flags=x==0?0x10u:0x20u;
            result=((a^b)&0x80000000u)|0x7f7fffffu;
        } else {
            flags=0x20u|(x==0?0x10u:0);
            result=((a^b)&0x80000000u)|(x==0?0:0x7f7fffffu);
        }
    } else {
        if(op==2u&&y<0)flags=0x10u;
        result=vu_scalar_bits(vu_u(x/(op==2u?sqrtf(fabsf(y)):y)));
    }
    vi[22]=result;vi[16]=(vi[16]&~0x30u)|flags|(flags<<6);
}
/* EFU polynomial constants and instruction semantics follow PCSX2
 * VUops.cpp (GPL-3.0+, PCSX2 Dev Team). This is a scalar reference path;
 * exact hardware polynomial rounding remains a separate validation task. */
static float vu_eatan(float x) {
    static const float c[9]={0.999999344348907f,-0.333298563957214f,
        0.199465364217758f,-0.139085337519646f,0.096420042216778f,
        -0.055909886956215f,0.021861229091883f,-0.004054057877511f,
        0.785398185253143f};
    double sum=c[0]*x;
    for(unsigned i=1;i<8;i++)sum+=c[i]*pow((double)x,2*i+1);
    float result=(float)sum;result+=c[8];
    return vu_f(vu_scalar_bits(vu_u(result)));
}
static unsigned vu_efu_latency(uint32_t w) {
    static const unsigned cycles[4][4]={
        {11,18,18,24},{54,54,12,0},{12,18,12,0},{29,54,44,0}};
    unsigned sub=(w>>6)&31u;
    return sub>=28u?cycles[sub-28u][w&3u]:0;
}
static int vu_efu_compute(uint32_t vf[32][4],uint32_t *vi,uint32_t w) {
    unsigned sub=(w>>6)&31u,bc=w&3u,fs=(w>>11)&31u;
    if(!vu_efu_latency(w))return 0;
    float x=vu_f(vu_scalar_bits(vf[fs][0])),y=vu_f(vu_scalar_bits(vf[fs][1]));
    float z=vu_f(vu_scalar_bits(vf[fs][2])),v=vu_f(vu_scalar_bits(vf[fs][(w>>21)&3u]));
    float p=0;
    if(sub==28u) {
        p=x*x+y*y+z*z;
        if(bc==1u){if(p!=0)p=1.0f/p;}
        if(bc>=2u&&p>=0){p=sqrtf(p);if(bc==3u&&p!=0)p=1.0f/p;}
    } else if(sub==29u) {
        if(bc==2u)p=x+y+z+vu_f(vu_scalar_bits(vf[fs][3]));
        else if(x!=0)p=vu_eatan((bc==0u?y:z)/x);
    } else if(sub==30u) {
        p=v;if(bc==2u){if(p!=0)p=(float)(1.0/(double)p);}
        else if(p>=0){p=sqrtf(p);if(bc==1u&&p!=0)p=1.0f/p;}
    } else {
        if(bc==0u) {
            static const float c[5]={1,-0.166666567325592f,0.008333025500178f,
                -0.000198074136279f,0.000002601886990f};
            double sum=c[0]*v;
            for(unsigned i=1;i<5;i++)sum+=c[i]*pow((double)v,2*i+1);
            p=vu_f(vu_scalar_bits(vu_u((float)sum)));
        } else if(bc==1u)p=vu_eatan(v);
        else {
            static const float c[6]={0.249998688697815f,0.031257584691048f,
                0.002591371303424f,0.000171562001924f,0.000005430199963f,0.000000690600018f};
            double sum=1.0f+c[0]*v;
            for(unsigned i=1;i<6;i++)sum+=c[i]*pow((double)v,i+1);
            p=(float)sum;p=(float)pow((double)p,4);
            p=vu_f(vu_scalar_bits(vu_u(p)));p=(float)(1.0/(double)p);
        }
    }
    vi[23]=vu_u(p);return 1;
}
static void vu_q_publish(vu_pipeline_t *p,uint32_t *vi) {
    if(p&&p->q_pending&&p->cycle>=p->q_ready) {
        vi[22]=p->q_value;
        vi[16]=(vi[16]&~0x30u)|p->q_status|(p->q_status<<6);
        p->q_pending=0;
    }
}
/* Writes result[lane] into vf[fd_idx][lane] for every lane selected
 * by dest_mask (bit3=x..bit0=w, see vu_opcodes.h). Register 0 (VF00)
 * is hardwired on real hardware - writes to it are always discarded,
 * matching the existing vu1_init()/ee_core.c convention. */
static void vu_write_dest(uint32_t vf[32][4], uint32_t fd_idx, uint32_t dest_mask, const float result[4])
{
    if (fd_idx == 0) return;
    for (int lane = 0; lane < 4; lane++) {
        int bit = (dest_mask >> (3 - lane)) & 1;
        if (bit) vf[fd_idx][lane] = vu_u(result[lane]);
    }
}

/* 16-bit VU integer register convention (real hardware: VI0-VI15 are
 * 16-bit, VI0 hardwired to 0 - see vu_opcodes.h). Registers >=16 in
 * this project's vi[32] array are the special/reserved slots
 * (REG_STATUS_FLAG etc, REG_I=21, REG_Q=22 - see vu.h) which are NOT
 * masked/zero-hardwired by this helper - only touch it for real
 * general-purpose integer-register writes (indices 1-15). */
static inline void vu_write_vi(uint32_t *vi, uint32_t idx, uint32_t value)
{
    /* Round 653 fix (task #640): real VU hardware only has 16
     * addressable integer registers (I0-I15) wired to a 5-bit
     * Is/It/Id field (see vu_opcodes.h's manual-sourced field-width
     * note). The unused 5th bit is not a "reserved slot" selector -
     * it is simply not decoded by the register-file hardware, so a
     * real encoded index of 16-31 aliases directly onto 0-15
     * (idx & 0xF), the same way an oversized register-select field
     * aliases on real silicon. This project's own special registers
     * (Status/MAC/Clip flag, R, I, Q - stored at vi[16..31] in this
     * project's vi[32] convention) are never reached through this
     * general Is-field path on real hardware or in this codebase -
     * they are written directly (e.g. "vi[22] = ..." in the
     * DIV/SQRT/RSQRT handlers below), bypassing this helper entirely,
     * so masking here cannot collide with those slots.
     *
     * Before this fix, idx>15 was silently dropped (a no-op write),
     * which permanently froze any loop-counter register that a real
     * microprogram encoded with the 5th bit set (e.g. SQI using Is=27
     * instead of the aliased 11) - the address computed from that
     * counter would never advance past its initial value, so the
     * same VU1 memory address got overwritten every iteration.
     * Live-captured via r653/r654 instrumentation (task #640): SQI at
     * micro-pc 0x0188/0x01a8/0x01b8 (Is=27/26/27) was stuck writing
     * to address 0 for 391+ consecutive iterations pre-fix; after
     * aliasing idx to 0xF, the same instructions correctly resolve to
     * real registers 11/10/11 and the address computation changes
     * accordingly. This directly implicates the old idx>15 no-op as a
     * likely contributor to the "period-16 repeating GIFtag content"
     * anomaly documented in Round 652. */
    idx &= 0xFu;
    if (idx == 0) return;
    vi[idx] = value & 0xFFFFu;
}
static inline uint32_t vu_read_vi16(const uint32_t *vi, uint32_t idx)
{
    idx &= 0xFu;
    if (idx == 0) return 0;
    return vi[idx] & 0xFFFFu;
}
static inline int16_t vu_read_vi16_signed(const uint32_t *vi, uint32_t idx)
{
    return (int16_t)vu_read_vi16(vi, idx);
}

static inline int32_t vu_sext(uint32_t value, int bits)
{
    uint32_t signbit = 1u << (bits - 1);
    return (int32_t)((value ^ signbit) - signbit);
}

/* ============================================================
 * UPPER WORD (FMAC arithmetic) - see vu_opcodes.h for the full
 * citation trail and confidence notes for every case below.
 * Returns 1 if a real instruction was decoded and executed, 0 if not
 * (caller counts the pair as unimplemented in that case).
 * ============================================================ */
static int vu_exec_upper(uint32_t vf[32][4], uint32_t *vi, uint32_t acc[4], uint32_t w)
{
#include "vu_upper_body.inc"
}

/* ============================================================
 * LOWER WORD (integer ALU / load-store / branch / misc)
 * Returns 1 if handled. pc is the byte address of THIS instruction
 * pair (needed for branch target computation); on a taken branch,
 * branch_delay/branch_target are set using the same 1-instruction-
 * delay-slot mechanism as the E-bit (see vu.h).
 * ============================================================ */
static int vu_exec_lower(uint32_t vf[32][4], uint32_t *vi,
                          uint8_t *mem, uint32_t mem_mask,
                          uint32_t w, uint32_t pc,
                          uint32_t *branch_delay, uint32_t *branch_target)
{
#include "vu_lower_body.inc"
}

#include "vu_fmac_internal.h"

/* See vu.h's header comment for the full citation and scope. */
static uint64_t vu_aot_counts[2];
uint64_t vu_aot_executed_count(unsigned half){return half<2u?vu_aot_counts[half]:0u;}
#ifdef GEKKO2_VU_AOT
#include "hw/vu_aot_generated.inc"
#else
#define vu_aot_try_timed_upper(vf,vi,acc,word,mac,status) 0
#define vu_aot_available(upper,lower) 0
#define vu_aot_try_upper(vf,vi,acc,word) 0
#define vu_aot_try_lower(vf,vi,mem,mask,pc,bd,bt,word) 0
#endif
int vu_aot_pair_available(uint32_t upper,uint32_t lower){(void)upper;(void)lower;return vu_aot_available(upper,lower);}

static int vu_micro_words_timed(uint32_t vf[32][4], uint32_t *vi, uint32_t acc[4],
                   uint8_t *mem, uint32_t mem_mask,
                   uint8_t *micro, uint32_t micro_mask,
                   uint32_t *tpc, uint32_t *branch_delay, uint32_t *branch_target,
                   uint32_t *ebit_delay,
                   uint64_t *instructions_executed, uint64_t *unimplemented_opcodes_seen,
                   vu_pipeline_t *pipeline,vu_timing_t *timing,uint32_t upper,uint32_t lower)
{
    uint32_t off = *tpc & micro_mask;
    /* A second XGKICK cannot replace a still-blocked first kick. */
    if(mem==g_vu1.mem&&pipeline==&g_vu1.pipeline&&!(upper&0x80000000u)&&
       VU_L_OPCODE(lower)==64u&&VU_L_FUNCT6(lower)>=60u&&VU_L_RD(lower)==27u&&(lower&3u)==0u&&g_vu1.xgkick_pending) {
        g_vu1.xgkick_busy_stalls++;
        vu1_service_xgkick(1);
        if(g_vu1.xgkick_pending)return 2; /* producer stalled; no pair retired */
    }
    uint32_t this_pc = off;
    uint32_t previous_branch_delay=*branch_delay;
    uint32_t previous_branch_target=*branch_target;

    int qop=!(upper&0x80000000u)&&VU_L_OPCODE(lower)==64u&&
        VU_L_FUNCT6(lower)>=60u&&VU_L_RD(lower)==14u;
    int pop=!(upper&0x80000000u)&&VU_L_OPCODE(lower)==64u&&
        VU_L_FUNCT6(lower)>=60u&&mem_mask==VU1_MEM_SIZE-1u&&
        (vu_efu_latency(lower)|| (VU_L_RD(lower)==30u&&(lower&3u)==3u));
    if(pipeline) {
        if(pop&&pipeline->p_pending&&pipeline->cycle+1<pipeline->p_ready)
            pipeline->cycle=pipeline->p_ready-1;
        if(pipeline->p_pending&&(pipeline->cycle>=pipeline->p_ready||pop)) {
            vi[23]=pipeline->p_value;pipeline->p_pending=0;
        }
        /* FDIV/WAITQ stalls happen BEFORE either half executes. */
        if(qop&&pipeline->q_pending&&pipeline->cycle<pipeline->q_ready)
            pipeline->cycle=pipeline->q_ready;
        if(pipeline->p_pending&&pipeline->cycle>=pipeline->p_ready) {
            vi[23]=pipeline->p_value;pipeline->p_pending=0;
        }
    }
    vu_fmac_stall(timing,pipeline,upper,lower);
    if(pipeline){vu_fmac_publish(timing,vi,pipeline->cycle);vu_q_publish(pipeline,vi);}
    /* Drain bytes whose VU issue clocks elapsed before this pair can
     * overwrite them. FDIV/EFU stalls contribute to the same clock. */
    if(mem==g_vu1.mem && pipeline==&g_vu1.pipeline)
        vu1_service_xgkick(0);
    unsigned upper_dst=vu_pair_upper_dest(upper);
    int conflict=vu_pair_vf_conflict(upper,lower);
    uint32_t old_vf[4],new_vf[4];
    if(conflict)memcpy(old_vf,vf[upper_dst],sizeof(old_vf));
    uint32_t old_q=vi[22],old_status=vi[16],old_p=vi[23],old_clip=vi[18];
    int clip_upper=VU_U_IS_SPECIAL(upper)&&VU_SPEC_SUBOP(upper)==7u&&(upper&3u)==3u;
    int clip_lower=(VU_L_OPCODE(lower)>=16u&&VU_L_OPCODE(lower)<=19u)||VU_L_OPCODE(lower)==28u;

    *tpc = (off + 8u) & micro_mask;

    /* E flag (bit 30 of the upper word) - real hardware executes
     * exactly one more instruction after this one before actually
     * stopping (the classic VU "E-bit delay slot") - see header
     * comment for the exact countdown arithmetic this mirrors. */
    if (upper & 0x40000000u)
        *ebit_delay = 2u;

    /* PCSX2 VU0microInterp.cpp / VU1microInterp.cpp: D/T use
     * VU0's shared FBRST and VPU_STAT even for VU1. A gated trap
     * executes this pair, then stops without an extra E delay pair.
     * Disabled traps leave an already pending E countdown intact. */
    if (upper & 0x18000000u) {
        int unit1=mem_mask==VU1_MEM_SIZE-1u;
        uint32_t *control=unit1?ee_core_get_state()->cop2_ctrl:vi;
        unsigned shift=unit1?8u:0u;
        uint32_t flags=0;
        if((upper&0x10000000u)&&(control[28]&(4u<<shift)))flags|=2u<<shift;
        if((upper&0x08000000u)&&(control[28]&(8u<<shift)))flags|=4u<<shift;
        if(flags) {
            control[29]|=flags;
            /* Each enabled source has its own raise, including D+T. */
            if(flags&(2u<<shift))ee_intc_raise(unit1?7:6);
            if(flags&(4u<<shift))ee_intc_raise(unit1?7:6);
            *ebit_delay=1u;
        }
    }

    int upper_ok, lower_ok;
    uint32_t fmac_mac=0,fmac_status=0;
    /* Flags remain invisible to the paired lower instruction. */
    int arithmetic=timing?(vu_aot_try_timed_upper(vf,vi,acc,upper,&fmac_mac,&fmac_status)||
        vu_fmac_arithmetic(vf,vi,acc,upper,&fmac_mac,&fmac_status)):0;
    int fsset=!(upper&0x80000000u)&&VU_L_OPCODE(lower)==21u;

    /* I flag (bit 31 of the upper word): only the upper instruction
     * executes this pair; the lower word's raw bits become the real
     * $I$ register (VI[21], REG_I per PCSX2's VU.h VURegFlags enum). */
    if ((!timing||(!arithmetic&&!fsset))&&!vu_aot_available(upper,lower)&&!qop&&!pop&&!conflict&&!(clip_upper&&clip_lower)&&vu_jit_try_pair(vf,vi,acc,mem,mem_mask,this_pc,branch_delay,branch_target,upper,lower)) {
        upper_ok=lower_ok=1;
    } else if (upper & 0x80000000u) {
        upper_ok = arithmetic || vu_aot_try_upper(vf,vi,acc,upper) || vu_jit_try_upper(vf, vi, acc, upper) || vu_exec_upper(vf, vi, acc, upper);
        vi[21] = lower;
        lower_ok = 1; /* the lower word was consumed as data (the I-immediate), not an instruction - correctly decoded, not "unimplemented" */
    } else {
        upper_ok = arithmetic || vu_aot_try_upper(vf,vi,acc,upper) || vu_jit_try_upper(vf, vi, acc, upper) || vu_exec_upper(vf, vi, acc, upper);
        if(conflict) {
            memcpy(new_vf,vf[upper_dst],sizeof(new_vf));
            memcpy(vf[upper_dst],old_vf,sizeof(old_vf));
        }
        /* Upper wins a same-VF write collision: discard the lower half,
         * including an LQI/LQD base update, as in the reference pipeline. */
        uint32_t new_clip=vi[18];
        if(clip_upper&&clip_lower)vi[18]=old_clip;
        if((conflict&&vu_pair_lower_dest(lower)==upper_dst) || (clip_upper&&VU_L_OPCODE(lower)==17u))lower_ok=1;
        else lower_ok = vu_aot_try_lower(vf,vi,mem,mem_mask,this_pc,branch_delay,branch_target,lower) || (!qop&&!pop&&vu_jit_try_lower(vf, vi, mem, mem_mask, lower, this_pc, branch_delay, branch_target)) ||
                   vu_exec_lower(vf, vi, mem, mem_mask, lower, this_pc, branch_delay, branch_target);
        if(conflict)memcpy(vf[upper_dst],new_vf,sizeof(new_vf));
        if(clip_upper&&clip_lower)vi[18]=new_clip;
    }

    if(pipeline) {
        if(qop&&(lower&3u)<3u) {
            pipeline->q_value=vi[22];pipeline->q_status=vi[16]&0x30u;
            pipeline->q_ready=pipeline->cycle+((lower&3u)==2u?13u:7u);
            pipeline->q_pending=1;vi[22]=old_q;vi[16]=(vi[16]&~0xc30u)|(old_status&0xc30u);
        }
        if(pop&&vu_efu_latency(lower)) {
            pipeline->p_value=vi[23];pipeline->p_ready=pipeline->cycle+vu_efu_latency(lower);
            pipeline->p_pending=1;vi[23]=old_p;
        }
        if(timing&&(arithmetic||fsset)) {
            uint32_t set_status=fsset?vi[16]:fmac_status;
            uint32_t issue_mac=arithmetic?fmac_mac:vi[17];
            if(fsset&&!arithmetic&&timing->count) {
                unsigned last=(timing->head+timing->count-1u)&3u;
                issue_mac=timing->flags[last].mac;
                set_status=(set_status&0xfc0u)|(timing->flags[last].status&15u);
            }
            if(fsset) {
                if(arithmetic)set_status=(set_status&0xfc0u)|(fmac_status&15u);
                vi[16]=old_status;
            }
            vu_fmac_issue(timing,vi,pipeline,issue_mac,set_status,fsset);
        }
        vu_fmac_mark_write(timing,pipeline,upper_dst,(upper&63u)==46u?14u:((upper>>21)&15u));
        if(!(upper&0x80000000u)&&!(conflict&&vu_pair_lower_dest(lower)==upper_dst))
            vu_fmac_mark_write(timing,pipeline,vu_pair_lower_dest(lower),(lower>>21)&15u);
        if(timing&&mem_mask==4095u&&(upper&0x20000000u))timing->m_yield=1;
        pipeline->cycle++;
    }
    if (!upper_ok || !lower_ok) {
        (*unimplemented_opcodes_seen)++;
#ifdef GEKKO2_GAME_TRACE
        if(mem==g_vu1.mem&&unknown_used<8u) {
            unsigned i=0;while(i<unknown_used&&(unknown_words[i][0]!=upper||unknown_words[i][1]!=lower))i++;
            if(i==unknown_used){unknown_words[i][0]=upper;unknown_words[i][1]=lower;unknown_used++;
                game_trace_record(GT_VU_UNKNOWN,this_pc,upper,lower,((unsigned)upper_ok<<1)|(unsigned)lower_ok);}
        }
#endif
    }
    (*instructions_executed)++;

    /* Branch delay-slot mechanism (same 1-instruction-delay pattern as
     * the E-bit, per PCSX2's _vu0Exec) - vu_exec_lower() above may
     * have just set branch_delay=2 and branch_target for a taken branch
     * this instruction; this countdown (shared with any branch armed
     * on a PRIOR step) is what actually redirects *tpc once the one
     * real delay-slot instruction has retired. */
    if(previous_branch_delay==1u && *branch_delay==2u) {
        /* A taken branch in a branch delay slot does not replace the
         * older redirect. Its own delay slot executes at that older
         * target, then redirects to the younger branch target. */
        *tpc=previous_branch_target&micro_mask;
        *branch_delay=1u;
    } else if (*branch_delay > 0) {
        if (--(*branch_delay) == 0)
            *tpc = *branch_target & micro_mask;
    }

    if (*ebit_delay > 0) {
        if (--(*ebit_delay) == 0) {
            if(pipeline&&pipeline->q_pending) {
                pipeline->cycle=pipeline->q_ready;vu_q_publish(pipeline,vi);
            }
            if(pipeline&&pipeline->p_pending) {
                if(pipeline->cycle<pipeline->p_ready)pipeline->cycle=pipeline->p_ready;
                vi[23]=pipeline->p_value;pipeline->p_pending=0;
            }
            if(pipeline&&timing&&timing->count) {
                unsigned last=(timing->head+timing->count-1u)&3u;
                if(pipeline->cycle<timing->flags[last].ready)pipeline->cycle=timing->flags[last].ready;
                vu_fmac_publish(timing,vi,pipeline->cycle);
            }
            return 1; /* stopped */
        }
    }
    return 0;
}

int vu_micro_step_timed(uint32_t vf[32][4], uint32_t *vi, uint32_t acc[4],
                   uint8_t *mem, uint32_t mem_mask,
                   uint8_t *micro, uint32_t micro_mask,
                   uint32_t *tpc, uint32_t *branch_delay, uint32_t *branch_target,
                   uint32_t *ebit_delay,
                   uint64_t *instructions_executed, uint64_t *unimplemented_opcodes_seen,
                   vu_pipeline_t *pipeline,vu_timing_t *timing)
{
    uint32_t off=*tpc&micro_mask;
    return vu_micro_words_timed(vf,vi,acc,mem,mem_mask,micro,micro_mask,tpc,
        branch_delay,branch_target,ebit_delay,instructions_executed,
        unimplemented_opcodes_seen,pipeline,timing,
        vu_rd_le32(micro+off+4u),vu_rd_le32(micro+off));
}

/* Embedded words are checked immediately before execution, so local writes
 * or synchronous helpers cannot execute a stale later pair. */
int vu_jit_timed_pair(vu_jit_timed_context *c,uint32_t upper,uint32_t lower,uint32_t pc)
{
    if((*c->pc&c->micro_mask)!=pc || *c->delay || *c->ebit ||
       (c->timing&&c->timing->m_yield) ||
       vu_rd_le32(c->micro+pc)!=lower || vu_rd_le32(c->micro+pc+4u)!=upper)return 0;
    uint64_t before=*c->retired;
    c->stopped=vu_micro_words_timed(c->vf,c->vi,c->acc,c->mem,c->mask,
        c->micro,c->micro_mask,c->pc,c->delay,c->target,c->ebit,
        c->retired,c->unknown,c->pipeline,c->timing,upper,lower);
    c->ran+=(unsigned)(*c->retired-before);
    return !c->stopped && !*c->delay && !*c->ebit &&
        !(c->timing&&c->timing->m_yield);
}

/* Legacy test/embedding API has immediate arithmetic semantics. Production
 * units use vu_micro_step_timed with serialized timing state. */
int vu_micro_step_pipeline(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],
    uint8_t *mem,uint32_t mm,uint8_t *micro,uint32_t um,uint32_t *pc,
    uint32_t *bd,uint32_t *bt,uint32_t *ed,uint64_t *retired,uint64_t *unknown,
    vu_pipeline_t *pipeline) {
    return vu_micro_step_timed(vf,vi,acc,mem,mm,micro,um,pc,bd,bt,ed,retired,unknown,pipeline,NULL);
}

/* PCSX2 VUops.cpp _vuXGKICKTransfer/_vuXGKICK: one quadword per
 * two VU clocks, with one issue-cycle credit. Packet lengths are read
 * lazily from live VU memory; later SQ writes are visible until transfer.
 * No ring copy or transfer-sized temporary buffer is needed. The one-ring
 * cap retains the BIOS malformed-packet bound from GSState::Transfer<0>. */
static void vu1_start_xgkick(uint32_t addr)
{
    game_trace_record(GT_KICK,addr,g_vu1.tpc,gif_get_state()->arb_owner,0);
    g_vu1.xgkick_addr=addr&(VU1_MEM_SIZE-16u);
    g_vu1.xgkick_packet_qwords=0;
    g_vu1.xgkick_ring_qwords=VU1_MEM_SIZE/16u;
    g_vu1.xgkick_cycle_credit=1u;
    g_vu1.xgkick_last_cycle=g_vu1.pipeline.cycle;
    g_vu1.xgkick_eop=0;
    g_vu1.xgkick_pending=1;
    ee_core_get_state()->cop2_ctrl[29]|=1u<<12;
    gif_begin_xgkick();
}

static void vu1_service_xgkick(int flush)
{
    if(!g_vu1.xgkick_pending)return;
    uint64_t now=g_vu1.pipeline.cycle;
    if(now>=g_vu1.xgkick_last_cycle)
        g_vu1.xgkick_cycle_credit+=now-g_vu1.xgkick_last_cycle;
    g_vu1.xgkick_last_cycle=now;
    while(g_vu1.xgkick_pending && (flush || g_vu1.xgkick_cycle_credit>=2u)) {
        uint32_t addr=g_vu1.xgkick_addr;
        uint32_t packet=g_vu1.xgkick_packet_qwords,eop=g_vu1.xgkick_eop;
        int new_tag=!packet;
        if(new_tag) {
            uint32_t lo=vu_rd_le32(g_vu1.mem+addr);
            uint32_t hi=vu_rd_le32(g_vu1.mem+addr+4u);
            uint32_t loops=lo&0x7fffu,mode=(hi>>26)&3u,nreg=hi>>28;
            if(!nreg)nreg=16u;
            uint32_t payload=mode==0u?loops*nreg:
                mode==1u?(loops*nreg+1u)/2u:loops;
            packet=1u+payload;
            eop=!!(lo&0x8000u);
        }
        uint32_t take=flush?g_vu1.xgkick_ring_qwords:
            (uint32_t)(g_vu1.xgkick_cycle_credit/2u > g_vu1.xgkick_ring_qwords ?
                g_vu1.xgkick_ring_qwords:g_vu1.xgkick_cycle_credit/2u);
        if(take>packet)take=packet;
        if(take>(VU1_MEM_SIZE-addr)/16u)take=(VU1_MEM_SIZE-addr)/16u;
        take=gif_try_process_quadwords(GIF_PATH_1,g_vu1.mem+addr,take);
        if(!take)break; /* Rejected tags remain live, including length/EOP. */
        if(new_tag){g_vu1.xgkick_packet_qwords=packet;g_vu1.xgkick_eop=eop;}
        g_vu1.xgkick_addr=(addr+take*16u)&(VU1_MEM_SIZE-1u);
        g_vu1.xgkick_packet_qwords-=take;
        g_vu1.xgkick_ring_qwords-=take;
        uint64_t cost=take*2u;
        g_vu1.xgkick_cycle_credit=g_vu1.xgkick_cycle_credit>cost?
            g_vu1.xgkick_cycle_credit-cost:0u;
        if(flush)g_vu1.pipeline.cycle+=cost;
        if(!g_vu1.xgkick_ring_qwords ||
           (!g_vu1.xgkick_packet_qwords && g_vu1.xgkick_eop)) {
            g_vu1.xgkick_pending=0;
            ee_core_get_state()->cop2_ctrl[29]&=~(1u<<12);
            gif_end_xgkick();
            if(g_vu1.xgkick_deferred){
                uint32_t next=g_vu1.xgkick_deferred_addr;
                g_vu1.xgkick_deferred=0;
                vu1_start_xgkick(next);
                /* The next packet has independent timing credit. */
                if(!flush)break;
            }
        }
    }
    g_vu1.xgkick_last_cycle=g_vu1.pipeline.cycle;
}

static void vu1_issue_xgkick(uint32_t addr)
{
    if(g_vu1.xgkick_pending) {
        g_vu1.xgkick_busy_stalls++;
        vu1_service_xgkick(1);
        if(g_vu1.xgkick_pending) {
            /* GIF may be owned by PATH3 and refuse the forced drain.
             * Never discard the current packet by replacing its address.
             * Retain one subsequent start; additional starts are counted
             * as unresolved stalls for later precise VU scheduling work. */
            if(!g_vu1.xgkick_deferred){
                g_vu1.xgkick_deferred_addr=addr&(VU1_MEM_SIZE-16u);
                g_vu1.xgkick_deferred=1;
            }else g_vu1.xgkick_deferred_stalls++;
            return;
        }
    }
    vu1_start_xgkick(addr);
}

/* Compatibility one-pair API: no asynchronous state supplied. Production
 * VU0/VU1 runners use the explicit per-unit pipeline entry below. */
int vu_micro_step(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],
    uint8_t *mem,uint32_t mm,uint8_t *micro,uint32_t um,
    uint32_t *pc,uint32_t *bd,uint32_t *bt,uint32_t *ed,
    uint64_t *retired,uint64_t *unknown) {
    return vu_micro_step_pipeline(vf,vi,acc,mem,mm,micro,um,pc,bd,bt,ed,retired,unknown,NULL);
}

/* Safety cap on a single MSCAL/MSCNT run - this project's own guard
 * against a genuinely-infinite microprogram (e.g. malformed/garbage
 * micro memory with no E-bit ever set), not a real hardware behavior. */
#define VU_EXEC_STEP_CAP 65536u

/* Avoid constructing the large block ABI on an obvious single-pair or
 * delay-slot path. Classification does not execute or alter micro state. */
static inline int vu1_block_candidate(void)
{
    if(!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT))return 0;
    if(g_vu1.xgkick_pending||g_vu1.branch_delay||g_vu1.ebit_delay)return 0;
    for(unsigned n=0;n<2;n++) {
        uint32_t off=(g_vu1.tpc+n*8u)&(VU1_MICRO_SIZE-1u);
        uint32_t up=vu_rd_le32(g_vu1.micro+off+4u);
        if(up&0x7e000000u)return 0;
        if(!(up&0x80000000u)) {
            unsigned op=vu_rd_le32(g_vu1.micro+off)>>25;
            if(op>=0x20u&&op<=0x2fu)return 0;
        }
    }
    return 1;
}

static void vu1_run_pairs(void)
{
    for (uint32_t i = 0; i < VU_EXEC_STEP_CAP; i++) {
        int stopped = vu_micro_step_timed(g_vu1.vf, g_vu1.vi, g_vu1.acc,
                                     g_vu1.mem, VU1_MEM_SIZE - 1u,
                                     g_vu1.micro, VU1_MICRO_SIZE - 1u,
                                     &g_vu1.tpc, &g_vu1.branch_delay, &g_vu1.branch_target,
                                     &g_vu1.ebit_delay,
                                     &g_vu1.instructions_executed, &g_vu1.unimplemented_opcodes_seen,&g_vu1.pipeline,&g_vu1.timing);
        if(stopped==2)break;
        if (stopped==1) {
            g_vu1.running=0;ee_core_get_state()->cop2_ctrl[29]&=~(1u<<8);
            vu1_service_xgkick(1);
            break;
        }
    }

}

static void vu1_run_blocks(void)
{
    for (uint32_t i = 0; i < VU_EXEC_STEP_CAP; i++) {
        unsigned ran=0;
        vu_jit_timed_context c={g_vu1.vf,g_vu1.vi,g_vu1.acc,g_vu1.mem,g_vu1.micro,
            VU1_MEM_SIZE-1u,VU1_MICRO_SIZE-1u,&g_vu1.tpc,&g_vu1.branch_delay,
            &g_vu1.branch_target,&g_vu1.ebit_delay,&g_vu1.instructions_executed,
            &g_vu1.unimplemented_opcodes_seen,&g_vu1.pipeline,&g_vu1.timing,0,0};
        if(vu1_block_candidate())ran=vu_jit_try_timed_block(&c,VU_EXEC_STEP_CAP-i);
        if(ran){
            if(c.stopped==1){g_vu1.running=0;ee_core_get_state()->cop2_ctrl[29]&=~(1u<<8);vu1_service_xgkick(1);break;}
            if(c.stopped==2)break;
            i+=ran-1u;continue;
        }
        int stopped = vu_micro_step_timed(g_vu1.vf, g_vu1.vi, g_vu1.acc,
                                     g_vu1.mem, VU1_MEM_SIZE - 1u,
                                     g_vu1.micro, VU1_MICRO_SIZE - 1u,
                                     &g_vu1.tpc, &g_vu1.branch_delay, &g_vu1.branch_target,
                                     &g_vu1.ebit_delay,
                                     &g_vu1.instructions_executed, &g_vu1.unimplemented_opcodes_seen,&g_vu1.pipeline,&g_vu1.timing);
        if(stopped==2)break;
        if (stopped==1) {
            g_vu1.running=0;ee_core_get_state()->cop2_ctrl[29]&=~(1u<<8);
            vu1_service_xgkick(1);
            break;
        }
    }

}

void vu1_exec_micro(uint32_t start_addr)
{
    g_vu1.tpc = (start_addr << 3) & (VU1_MICRO_SIZE - 1u);
    g_vu1.running = 1;ee_core_get_state()->cop2_ctrl[29]|=1u<<8;

    /* Q/P, branch and GIF boundaries can be scalar; later work can
     * resume native blocks within the same synchronously ordered run. */
    if(gekko2_opt_enabled(GEKKO2_OPT_VU_JIT)&&gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES))vu1_run_blocks();
    else if(vu1_block_candidate())vu1_run_blocks();else vu1_run_pairs();

}

/* Round 576 (task #551): real MSCNT semantics. Ground-truthed against
 * ps2sdk's packet2_utils_vu_add_continue_program() (ee/packet2/include/
 * packet2_utils.h) - the real, standard way game code re-invokes an
 * already-running VU1 microprogram for each subsequent draw call issues
 * FLUSH + MSCNT with NO address operand at all (unlike MSCAL/MSCALF,
 * which always carry an explicit start address). Real hardware's MSCNT
 * resumes execution from wherever the VU's own micro program counter
 * (TPC) was left after the PREVIOUS run stopped (typically an E-bit
 * halt partway through a larger multi-object microprogram, or address
 * 0 the very first time) - it does NOT restart from a caller-supplied
 * address. vu1_exec_micro() (above) was, until this round, called
 * identically for MSCAL/MSCALF/MSCNT alike (vif.c's dispatch passed the
 * VIFcode's IMM field to all three), silently resetting g_vu1.tpc to
 * that IMM value (typically 0, since a real MSCNT VIFcode's IMM field
 * is unused/reserved) on every MSCNT - i.e. every "continue" call was
 * actually restarting the program from address 0 instead of resuming.
 * This function fixes that: it runs from g_vu1.tpc AS-IS, exactly like
 * vu1_exec_micro() but without the reset line. */
void vu1_exec_micro_continue(void)
{
    g_vu1.running = 1;ee_core_get_state()->cop2_ctrl[29]|=1u<<8;

    /* Q/P, branch and GIF boundaries can be scalar; later work can
     * resume native blocks within the same synchronously ordered run. */
    if(gekko2_opt_enabled(GEKKO2_OPT_VU_JIT)&&gekko2_opt_enabled(GEKKO2_OPT_EE_SERVICES))vu1_run_blocks();
    else if(vu1_block_candidate())vu1_run_blocks();else vu1_run_pairs();

}

/* Called by the common EE scheduler for a yielded microprogram or a
 * pending transfer after E. No synthetic microinstruction retirement. */
void vu1_tick(void) {
    if(!g_vu1.running&&!g_vu1.xgkick_pending)return;
    if(g_vu1.running) {
        int stopped=vu_micro_step_timed(g_vu1.vf,g_vu1.vi,g_vu1.acc,
            g_vu1.mem,VU1_MEM_SIZE-1u,g_vu1.micro,VU1_MICRO_SIZE-1u,
            &g_vu1.tpc,&g_vu1.branch_delay,&g_vu1.branch_target,&g_vu1.ebit_delay,
            &g_vu1.instructions_executed,&g_vu1.unimplemented_opcodes_seen,
            &g_vu1.pipeline,&g_vu1.timing);
        if(stopped==1){g_vu1.running=0;ee_core_get_state()->cop2_ctrl[29]&=~(1u<<8);}
        if(stopped==2)g_vu1.pipeline.cycle++;
    } else g_vu1.pipeline.cycle++;
    vu1_service_xgkick(!g_vu1.running);
}
