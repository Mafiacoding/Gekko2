"""Linked PPC low-RAM instruction-fetch reuse with scalar/native oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
body=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text()
body=body[body.index('import random'):body.index('# Mixed instruction sequences')]
exec(compile(body,'IOP-layout-and-oracle','exec'))
original_setup=setup
def setup(words,regs=None):
 original_setup(words,regs);u.mem_write(syms['s_zero_run'],bytes(8))
for addr in [0x19000,0x80019000,0xa0019000,0x100000]:
 ibase=addr&0x1fffffff
 words=[(9<<26)|(2<<21)|(2<<16)|1]*8
 def alias_pc():word(ist+io['pc'],addr);word(ist+io['next_pc'],addr+4)
 run_case(words,8,extra=alias_pc)
 # Zero-opcode diagnostic still advances or halts at the same boundary.
 run_case([0]*8,6,extra=alias_pc)
 # Source mutation remains live on warm cache hits.
 setup(words);assert call('iop_core_step_n',1)==1
 word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
 u.mem_write(iram+ibase,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
 assert call('iop_core_step_n',1)==1
 assert int.from_bytes(u.mem_read(ist+8,4),'big')==10
print('PASS low-RAM native/scalar fetch, zero diagnostics, aliases and live code mutation')
