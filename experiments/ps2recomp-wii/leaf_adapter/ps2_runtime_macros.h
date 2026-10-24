/* GPL-3.0-or-later. Scalar semantics for the explicit integer leaf subset. */
#pragma once
#include "ps2_runtime.h"
static inline uint64_t leaf_sign32(uint32_t v) {
 return uint64_t(v)|((v&0x80000000u)?0xffffffff00000000ull:0ull);
}
static inline void leaf_set32(R5900Context *ctx,unsigned r,uint32_t v) {
 if(r)ctx->r[r].lo=leaf_sign32(v);
}
static inline void leaf_set64(R5900Context *ctx,unsigned r,uint64_t v) {
 if(r)ctx->r[r].lo=v;
}
#define GPR_U32(ctx,reg) ((reg)?uint32_t((ctx)->r[(reg)].lo):0u)
#define GPR_S32(ctx,reg) int32_t(GPR_U32(ctx,reg))
#define GPR_U64(ctx,reg) ((reg)?(ctx)->r[(reg)].lo:0ull)
#define GPR_S64(ctx,reg) int64_t(GPR_U64(ctx,reg))
#define SET_GPR_S32(ctx,reg,v) leaf_set32((ctx),(reg),uint32_t(v))
#define SET_GPR_U32(ctx,reg,v) leaf_set32((ctx),(reg),uint32_t(v))
#define SET_GPR_U64(ctx,reg,v) leaf_set64((ctx),(reg),uint64_t(v))
#define SET_GPR_S64(ctx,reg,v) leaf_set64((ctx),(reg),uint64_t(v))
#define ADD32(a,b) (uint32_t(uint32_t(a)+uint32_t(b)))
#define SUB32(a,b) (uint32_t(uint32_t(a)-uint32_t(b)))
#define SLL32(a,b) (uint32_t(a)<<uint32_t(b))
#define SRL32(a,b) (uint32_t(a)>>uint32_t(b))
#define SRA32(a,b) (int32_t(a)>>uint32_t(b))
#define XOR32(a,b) (uint32_t(a)^uint32_t(b))
#define OR32(a,b) (uint32_t(a)|uint32_t(b))
#define AND32(a,b) (uint32_t(a)&uint32_t(b))
