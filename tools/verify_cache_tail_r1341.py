"""Synthetic linked PPC instruction costs; not hardware cycles/FPS."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];word(mask_addr,(int.from_bytes(u.mem_read(mask_addr,4),'big')|8192)&~(1<<17))
pcs=[base|0x80000000];candidate=pcs[0]+32
h=lambda pc:((pc>>2)^(pc>>12))&1023
while len(pcs)<4:
 if h(candidate)==h(pcs[0]):pcs.append(candidate)
 candidate+=4
def step(pc,budget):
 word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
 assert call('ee_core_step_n',budget)==budget
setup()
for pc in pcs:u.mem_write(ram+(pc&0x1fffffff),struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
for pc in pcs:step(pc,8)
# Warm the scalar tail at its successor PC, outside the measured phase.
step(pcs[0],2)
total[0]=0;active[0]=True
for cycle in range(100):
 step(pcs[0],2);step(pcs[0],8)
active[0]=False
assert reg2()==1034
seen=set()
for index in range(precise_cache_entries):
 addr=syms['precise_cache']+index*precise_slot_size
 if int.from_bytes(u.mem_read(addr+precise_fn_offset,4),'big'):
  pc=int.from_bytes(u.mem_read(addr,4),'big');n=int.from_bytes(u.mem_read(addr+precise_count_offset,4),'big')
  key=(pc,n,bytes(u.mem_read(addr+4,n*4)))
  assert key not in seen,('duplicate owner',pc,n)
  seen.add(key)
print('TAIL_COST' ,json.dumps({'PPC_instructions':total[0],'retired':1034}))
if 'precise_refusals' in syms:
 call('ee_jit_get_budget_cache_stat',4);assert u.reg_read(UC_PPC_REG_4)>0
# Declining compilation never suppresses live instruction mutation.
u.mem_write(ram+(pcs[0]&0x1fffffff),struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
old=reg2();step(pcs[0],2);assert reg2()==old+10
print('PASS short-grant replay, exact guest results and live mutation')
