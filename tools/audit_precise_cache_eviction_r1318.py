#!/usr/bin/env python3
from pathlib import Path
s=Path('source/core/recompiler/ee_jit.c').read_text()
h=Path('include/core/recompiler/ee_jit.h').read_text()
need=[
 'uint32_t serial;',
 'static int ee_precise_install_slot(',
 'if(!slot||!words||!fn||count<2u||count>8u||precise_active)return 0;',
 'next.serial=ee_precise_next_serial();',
 'if(old&&old!=fn){if(displaced)precise_evictions++;free((void*)old);}',
 'if(slot->serial!=serial||slot->fn!=fn||slot->pc!=st->pc)return 0;',
 'if(!n)ee_precise_release_slot(slot);',
 'for(unsigned n=0;n<256;n++)ee_precise_release_slot(&precise_cache[n]);',
 'uint64_t ee_jit_get_block_evictions(void){return precise_evictions;}'
]
missing=[x for x in need if x not in s]
if 'uint64_t ee_jit_get_block_evictions(void);' not in h:missing.append('public eviction getter')
if missing:
 print('R1318 precise cache audit: FAIL')
 for x in missing:print(' -',x)
 raise SystemExit(1)
print('R1318 precise cache audit: PASS')
print('  transactional install, active-code pinning, allocation serials and collision eviction accounting present')
