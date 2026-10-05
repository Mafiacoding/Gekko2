#!/usr/bin/env python3
from pathlib import Path


def once(path, old, new):
    p = Path(path)
    s = p.read_text()
    assert s.count(old) == 1, (path, s.count(old), old[:100])
    p.write_text(s.replace(old, new, 1))


# Checkpoint replacement installs a completely different RAM/TLB image while
# precise JIT generation arrays are intentionally process-local.
once(
    'source/core/checkpoint.c',
    '#include "core/ee/ee_core.h"\n',
    '#include "core/ee/ee_core.h"\n#include "core/recompiler/ee_jit.h" /* R1317 checkpoint source/mapping invalidation */\n',
)
once(
    'source/core/checkpoint.c',
    '''    ee->ram = era_scratch; /* ownership transferred - this becomes the live EE RAM buffer */
    ee->ram_size = era_size;
    ee->bios = ee_bios;
    iop->ram = ira_scratch; /* ownership transferred - live IOP RAM buffer */''',
    '''    ee->ram = era_scratch; /* ownership transferred - this becomes the live EE RAM buffer */
    ee->ram_size = era_size;
    ee->bios = ee_bios;
    /* R1317: process-local precise JIT stamps are not checkpoint state. */
    ee_jit_notify_physical_write(0u, ee->ram_size);
    ee_jit_notify_mapping_change();
    iop->ram = ira_scratch; /* ownership transferred - live IOP RAM buffer */''',
)

# LOADFILE's fixed-delta transfer writes physical EE RAM directly.
once(
    'source/core/ee/ee_core.c',
    '''    if (st->ram && phys < st->ram_size)
        st->ram[phys] = val;''',
    '''    if (st->ram && phys < st->ram_size) {
        ee_jit_notify_physical_write(phys, 1u);
        st->ram[phys] = val;
    }''',
)

# Verified RSPU2 resource RPC bypasses ee_mem_write* for its EE destination.
once(
    'source/core/ee/ee_core.c',
    '''        if(command==0x204eu)memcpy(st->ram+physical+copied,bytes,chunk);
        else memcpy(spu2_mixer_get_ram()+physical+copied,bytes,chunk);''',
    '''        if(command==0x204eu) {
            memcpy(st->ram+physical+copied,bytes,chunk);
            ee_jit_notify_physical_write(physical+copied,chunk);
        } else memcpy(spu2_mixer_get_ram()+physical+copied,bytes,chunk);''',
)

# Opt-in legacy repair is still a raw RAM writer when enabled.
once(
    'source/core/ee/ee_core.c',
    '''            for (k = 0; k < 8; k++) q[k] = (uint8_t)(ra >> (8u * k));
            r1249_repairs++;''',
    '''            for (k = 0; k < 8; k++) q[k] = (uint8_t)(ra >> (8u * k));
            ee_jit_notify_physical_write(0x01045a00u, 8u);
            r1249_repairs++;''',
)

# Same-process EE re-init replaces all RAM/TLB state but JIT caches persist.
once(
    'source/core/ee/ee_core.c',
    '''    dma_bind_ee_ram(g_state.ram, g_state.ram_size); /* chain-mode DMA reads tags/data from here */
    dma_set_ee_write_notify(ee_jit_notify_physical_write); /* R1316 code-page generations */''',
    '''    dma_bind_ee_ram(g_state.ram, g_state.ram_size); /* chain-mode DMA reads tags/data from here */
    dma_set_ee_write_notify(ee_jit_notify_physical_write); /* R1316 code-page generations */
    /* R1317: same-process EE re-init replaces the prior RAM/TLB image. */
    ee_jit_notify_physical_write(0u, g_state.ram_size);
    ee_jit_notify_mapping_change();''',
)

# Permanent regression audit for every direct writer found by the R1317 scan.
audit = r'''#!/usr/bin/env python3
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
'''
Path('tools/audit_ee_ram_writer_invalidation.py').write_text(audit)

notes = Path('docs/DYNAREC-WORKING-NOTES.md')
s = notes.read_text()
marker = '## R1317 — raw EE RAM writer invalidation'
if marker not in s:
    s += (
        '\n\n' + marker + '\n\n'
        'Precise-block source generations now cover the remaining audited direct EE-RAM bypass writers: '
        'checkpoint RAM/TLB replacement, same-process EE re-init, LOADFILE fixed-delta delivery, the verified '
        'RSPU2 EE-RAM copy, and the opt-in legacy frame repair. Existing R1316 CPU stores and DMA inbound/fromSPR '
        'notifications remain guarded by a permanent static audit. This is source/mapping coherency work only; '
        'it is not a Wii performance or hardware-certification claim.\n'
    )
    notes.write_text(s)
