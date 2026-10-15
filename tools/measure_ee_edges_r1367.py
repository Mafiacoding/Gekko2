"""Warm learned successors: linked PPC cost and complete EE-state hash."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
word(mask,(initial|1|8|16|(1<<13))&~(1<<24))
setup()
program=[(2<<26)|((base+16)>>2),(9<<26)|(2<<21)|(2<<16)|1,0,0,(2<<26)|(base>>2),(9<<26)|(3<<21)|(3<<16)|1]
u.mem_write(ram+base,struct.pack('<6I',*program))
assert call('ee_core_step_n',128)==128
total[0]=0;active[0]=True
for _ in range(100):assert call('ee_core_step_n',32)==32
active[0]=False
assert executed()==3328
print(json.dumps({'ppc_instructions':total[0],'retired':executed(),'state_sha256':hashlib.sha256(bytes(u.mem_read(state,size))).hexdigest()},sort_keys=True))
