"""Execute linked PPC cache policy against hot/cold collision and budget tails.
Synthetic instruction counts are not physical Wii cycles or FPS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask']
initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
word(mask_addr,(initial|8192)&~(1<<17)&~(1<<19))
def profile():
 dest=0x81760000;call('ee_jit_get_cache_profile',dest)
 return dict(zip(['lookups','hits','misses','collisions','stale','attempts','installed','failures','compile_tb','compile_samples'],struct.unpack('>10Q',u.mem_read(dest,80))))
pcs=[base|0x80000000];candidate=pcs[0]+32
h=lambda pc:((pc>>2)^(pc>>12))&1023
while len(pcs)<5:
 if h(candidate)==h(pcs[0]):pcs.append(candidate)
 candidate+=4
def step(pc,budget):
 word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
 assert call('ee_core_step_n',budget)==budget
def init():
 setup()
 for pc in pcs:u.mem_write(ram+(pc&0x1fffffff),struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
 total[0]=0;active[0]=True
init()
for pc in pcs[:4]:step(pc,8)
# Three frequently used owners share a set with two alternating cold owners.
for cycle in range(12):
 for pc in pcs[:3]:step(pc,8)
 step(pcs[3+cycle%2],8)
active[0]=False
assert reg2()==(4+12*4)*8
collision=profile();collision['PPC_instructions']=total[0]
if 'precise_recency' in syms:assert collision['installed']==15,collision
init()
for pc in pcs[:4]:step(pc,8)
before=profile()['installed']
for cycle in range(8):
 for pc in pcs[:4]:
  for budget in range(2,8):
   step(pc,budget)
   for warm in pcs[:4]:step(warm,8)
active[0]=False
assert reg2()==32+8*4*(sum(range(2,8))+6*4*8)
tails=profile();tails['PPC_instructions']=total[0]
if 'precise_recency' in syms:
 assert before==4 and tails['installed']<=16,tails
 assert call('ee_jit_get_budget_cache_stat',3)==0 and u.reg_read(UC_PPC_REG_4)>0
# A modified instruction must remain visible even when variant admission declines.
u.mem_write(ram+(pcs[0]&0x1fffffff),struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
old=reg2();step(pcs[0],2);assert reg2()==old+10
print('CACHE_POLICY',json.dumps({'hot_cold':collision,'budget_tails':tails}))
print('PASS equal retirement/register results, live mutation and guarded cache admission')
