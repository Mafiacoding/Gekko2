#!/usr/bin/env python3
"""Source-order check for native EE JIT fault fencing (not a PPC runtime proof)."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'source/core/recompiler/ee_jit.c').read_text()
for fn in ('ee_precise_cached_next', 'ee_precise_event_next', 'ee_precise_execute'):
    pos=s.find('static ',s.find('static ') if fn=='ee_precise_cached_next' else 0)
    import re
    m=re.search(r'static\s+(?:uint64_t|unsigned)\s+'+fn+r'\s*\(',s)
    if not m: raise AssertionError(f'missing function {fn}')
    block=s[m.start():m.start()+2200]
    first=block.find('st->exc_raised_this_step')
    fetch=block.find('ee_core_block_peek(')
    if first<0 or (fetch>=0 and first>fetch):raise AssertionError(f'unguarded first fetch in {fn}')
assert 'if(!st||st->exc_raised_this_step||st->mem_tlb_miss)return 0;' in s
print('BETA12.19 EE native-chain exception guard: 4 source-order checks passed (not Wii execution)')
