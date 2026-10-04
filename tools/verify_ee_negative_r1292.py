"""Real PPC EE rejection reuse across PCs and exact-instruction replacement."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_cache_r1267.py').read_text(),'verify_ppc_cache_r1267.py','exec'))
import json
from unicorn import UC_HOOK_CODE
reset_frontend();before=allocs;total=[0]
def tick(uc,address,size,user):total[0]+=1
h=u.hook_add(UC_HOOK_CODE,tick)
for n in range(64):assert call('ee_jit_try_execute_one_at',state,0x200000+4*n,bad)==0
u.hook_del(h)
enabled='Interpreter' not in Path(a.elf).name
assert allocs-before==(1 if 'g_ee_rejected' in syms else 64)*int(enabled)
print('EE_NEGATIVE_BENCH '+json.dumps({'elf':Path(a.elf).name,'PCs':64,'allocations':allocs-before,'PPC':total[0]}))
if 'g_ee_rejected' not in syms:raise SystemExit(0)
if enabled:
 assert call('ee_jit_try_execute_one_at',state,0x200000,good)==1
 # Two rejected MMI encodings collide in the 64-slot index.
 collision=bad^0x00400040;before=allocs
 assert call('ee_jit_try_execute_one_at',state,0x210000,collision)==0
 assert call('ee_jit_try_execute_one_at',state,0x210004,collision)==0
 assert call('ee_jit_try_execute_one_at',state,0x210008,bad)==0
 assert allocs==before+2
print('PASS EE cross-PC negative reuse, exact-key collision, SMC positive replacement and existing fallback/allocation-retry checks')
