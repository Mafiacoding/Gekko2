"""Actual PPC IOP negative translation cache across PCs and SMC changes."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_iop_control_r1269.py').read_text(),'verify_ppc_iop_control_r1269.py','exec'))
from unicorn import UC_HOOK_CODE
negative_count=[0]
def negative_instruction(uc,address,size,user):negative_count[0]+=1
handle=u.hook_add(UC_HOOK_CODE,negative_instruction)
before=allocs
unsupported=0xfc123456
for n in range(64):
    assert call('iop_jit_try_execute_one',ios,0x80007000+4*n,unsupported)==0
u.hook_del(handle)
import json
old='R1287' in Path(a.elf).name
assert allocs==before+(64 if old else 1)*int(enabled),(before,allocs,enabled)
print('IOP_NEGATIVE_BENCH '+json.dumps({'elf':Path(a.elf).name,'PCs':64,'compile_allocations':allocs-before,'PPC':negative_count[0],'enabled':enabled}))
if old:raise SystemExit(0)
# Same PC with a different supported instruction must not retain rejection.
word(ios+8,0)
good=(9<<26)|(2<<16)|123
assert call('iop_jit_try_execute_one',ios,0x80007000,good)==int(enabled)
assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==(123 if enabled else 0)
# Direct-mapped negative collisions remain exact-instruction keyed.
collision=unsupported^0x00400040
before=allocs
assert call('iop_jit_try_execute_one',ios,0x80009000,collision)==0
assert call('iop_jit_try_execute_one',ios,0x80009004,collision)==0
assert call('iop_jit_try_execute_one',ios,0x80009008,unsupported)==0
assert allocs==before+2*int(enabled),(before,allocs)
print('PASS IOP negative instruction cache: 64 PCs, SMC replacement, exact-key collision; no hardware FPS claim')
