#!/usr/bin/env python3
"""R1326 final-batch acceptance audit.

This intentionally checks architectural contracts rather than pretending that
host-side structural tests are Wii hardware validation.  It runs after the
three guarded R1326 source patches have been applied.
"""
from pathlib import Path
import subprocess, tempfile, textwrap

ROOT=Path(__file__).resolve().parents[1]

def text(path): return (ROOT/path).read_text()
def require(path,*needles):
    s=text(path)
    missing=[n for n in needles if n not in s]
    if missing:
        raise SystemExit(f"{path}: missing required R1326 markers: {missing}")

# Phase 1: VU scheduler-visible boundaries and real REG_R semantics.
require(Path('source/hw/vu.c'),
        'fdslot == VULS_FD_R_GROUP', 'vi[20] = r', '/* RNEXT */',
        '/* RGET */', '/* RINIT */', '/* RXOR */', '0x007fffffu', '0x3f800000u')
require(Path('source/core/recompiler/vu_jit.c'),
        'vu_lower_pipeline_boundary', 'VULS_FD_DIVQ_GROUP', 'VULS_FD_R_GROUP',
        'fd >= 0x19u', 'vu_lower_pipeline_boundary(lo[n])')

# Phase 2: 128-bit EE GPR residency for MMI-heavy blocks remains write-through.
require(Path('source/core/recompiler/ppc_residency_internal.h'),
        'has_mmi', 'unsigned lanes=ee?(has_mmi?4u:2u):1u',
        'unsigned max_guests=12u/lanes', 'a->slot[word]',
        'Context remains canonical at every helper and exit')

# Phase 3: direct learned links remain serial/generation/source guarded.
require(Path('source/core/recompiler/ee_jit.c'),
        'link_pc,link_serial', 'link_index', 'link_fn',
        'precise_direct_link_hits', 'source->link_serial==serial',
        'ee_precise_slot_generation_current(slot)',
        'ee_core_block_peek(st,st->pc,&word)',
        'slot->serial!=serial||slot->fn!=fn||slot->pc!=st->pc',
        'precise_active')
require(Path('include/core/recompiler/ee_jit.h'), 'ee_jit_get_direct_link_hits')

# Phase 4: privileged COP0 stays an explicit scalar boundary.  This is safer
# than inventing native semantics for ERET/TLB/BC0 until the scalar path itself
# defines those exact architectural actions.  Verify canonical forms against
# the real shared block policy, not a duplicated Python decoder.
csrc=textwrap.dedent(r'''
    #include <stdint.h>
    #include "core/recompiler/ee_block_policy.h"
    int main(void) {
      const uint32_t w[] = {
        0x40000000u, /* MFC0 */ 0x40800000u, /* MTC0 */
        0x42000001u, /* TLBR */ 0x42000002u, /* TLBWI */
        0x42000006u, /* TLBWR */ 0x42000008u, /* TLBP */
        0x42000018u, /* ERET */
        (0x10u<<26)|(8u<<21)|(0u<<16), /* BC0F */
        (0x10u<<26)|(8u<<21)|(1u<<16), /* BC0T */
        (0x10u<<26)|(8u<<21)|(2u<<16), /* BC0FL */
        (0x10u<<26)|(8u<<21)|(3u<<16)  /* BC0TL */
      };
      for (unsigned i=0;i<sizeof(w)/sizeof(w[0]);i++)
        if (ee_jit_block_candidate(w[i])) return 10+(int)i;
      return 0;
    }
''')
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/'cop0.c'; exe=Path(td)/'cop0'; c.write_text(csrc)
    subprocess.run(['gcc','-O2','-I'+str(ROOT/'include'),str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

# Phase 5: IOP warm blocks validate every compiled source word, not just the
# first instruction.  This closes later-word SMC/DMA stale-code reuse.
require(Path('source/core/recompiler/iop_jit.c'),
        'words[BLOCK_WORDS]', 'uint8_t count',
        'for(unsigned n=0;warm&&n<slot->count;n++)',
        'live!=slot->words[n]', 'memcpy(slot->words,words,count*sizeof(uint32_t))')

# Phase 6: correctness-first event policy.  A native block is still retired at
# guest-instruction granularity; this is deliberate.  Until a cycle/event
# deadline proof exists, coarser retirement would be an accuracy regression.
require(Path('source/core/ee/ee_core.c'),
        'void ee_core_block_commit(ee_state_t *st){ee_retire_instruction(st,0);}')
require(Path('source/core/iop/iop_core.c'),
        'void iop_core_block_retire(iop_state_t *st,uint32_t pc)',
        'if(st==&g_iop)iop_retire(st,pc,st->pc);')
require(Path('source/core/recompiler/ppc_dynarec.c'),
        'ee_core_block_commit', 'iop_core_block_retire')

print('R1326 final dynarec closure audit: PASS')
print('  VU scheduler/R semantics: guarded')
print('  EE MMI 128-bit residency plan: present, canonical write-through')
print('  EE direct successor links: serial + generation + live-source guarded')
print('  COP0 privileged/control forms: scalar boundary verified')
print('  IOP warm blocks: full compiled-prefix SMC validation')
print('  EE/IOP event retirement: instruction-granular by design')
