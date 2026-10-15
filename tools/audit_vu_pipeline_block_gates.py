#!/usr/bin/env python3
"""R1325: guard native VU blocks against pending Q/P pipeline state.

vu_jit_try_block() intentionally has no vu_pipeline_t argument. Therefore both
VU0 and VU1 entry classifiers must reject native block execution while either
Q or P has a pending result; otherwise the block path can advance cycles past a
ready point without publishing the result/status at the architectural boundary.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
ee=(root/'source/core/ee/ee_core.c').read_text()
vu=(root/'source/hw/vu.c').read_text()
jit=(root/'source/core/recompiler/vu_jit.c').read_text()

vu0='if(st->vu0_branch_delay||st->vu0_ebit_delay||(st->vu0_pipeline.q_pending||st->vu0_pipeline.p_pending))return 0;'
vu1='if(g_vu1.branch_delay||g_vu1.ebit_delay||(g_vu1.pipeline.q_pending||g_vu1.pipeline.p_pending))return 0;'
block='if(*delay||*ebit||budget<2u)return 0;'

missing=[]
if vu0 not in ee: missing.append('VU0 candidate does not gate both q_pending and p_pending')
if vu1 not in vu: missing.append('VU1 candidate does not gate both q_pending and p_pending')
if block not in jit: missing.append('VU block entry guard changed; re-audit pipeline ABI')
if 'vu_pipeline_t' in jit:
    # The current JIT file may include the type through headers, but the block ABI itself
    # must not silently gain a pipeline pointer without this audit being updated.
    sig='unsigned vu_jit_try_block(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint8_t *mem,'
    if sig not in jit: missing.append('VU block ABI changed; re-audit pending pipeline handling')

if missing:
    for m in missing: print('FAIL:',m)
    raise SystemExit(1)
print('R1325 VU pipeline block-gate audit: PASS')
print('  VU0 blocks gate pending Q and P')
print('  VU1 blocks gate pending Q and P')
print('  block ABI remains pipeline-blind, so caller gating is mandatory')
