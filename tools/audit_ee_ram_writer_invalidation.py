#!/usr/bin/env python3
"""Guard direct EE-RAM writers that bypass ee_mem_write* against stale JIT source pages."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
ee = (root / 'source/core/ee/ee_core.c').read_text()
ck = (root / 'source/core/checkpoint.c').read_text()
dma = (root / 'source/hw/dma.c').read_text()
errors = []


def need(text, token, label):
    if token not in text:
        errors.append(label)


need(ee, 'ee_jit_notify_physical_write(phys, 1u);\n        st->ram[phys] = val;',
     'LOADFILE raw RAM writer lacks notification')
need(ee, 'memcpy(st->ram+physical+copied,bytes,chunk);\n            ee_jit_notify_physical_write(physical+copied,chunk);',
     'RSPU2 raw RAM copy lacks notification')
need(ee, 'ee_jit_notify_physical_write(0x01045a00u, 8u);',
     'legacy frame repair lacks notification')
need(ee, 'ee_jit_notify_physical_write(0u, g_state.ram_size);\n    ee_jit_notify_mapping_change();',
     'EE re-init lacks full invalidation')
need(ck, '#include "core/recompiler/ee_jit.h"',
     'checkpoint loader lacks JIT invalidation API')
need(ck, 'ee_jit_notify_physical_write(0u, ee->ram_size);\n    ee_jit_notify_mapping_change();',
     'checkpoint restore lacks source/mapping invalidation')
need(dma, 'if (g_ee_write_notify) g_ee_write_notify(dst, 16u);',
     'fromSPR DMA lost source-generation notification')
need(dma, 'if (g_ee_write_notify) g_ee_write_notify(written_madr, len);',
     'inbound DMA lost source-generation notification')

if errors:
    print('EE RAM writer invalidation audit: FAIL', file=sys.stderr)
    for error in errors:
        print('  - ' + error, file=sys.stderr)
    raise SystemExit(1)
print('EE RAM writer invalidation audit: PASS')
print('  checkpoint/re-init, LOADFILE, RSPU2, legacy repair and DMA writers are generation-aware')
