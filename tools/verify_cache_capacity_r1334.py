"""Linked PPC owner-capacity replay; same source also runs R1333 ELF.
Counted PPC instructions use mocked allocation/libc, not physical Wii timing.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
word(mask_addr,initial|8192)
def profile():
 dest=0x81760000;call('ee_jit_get_cache_profile',dest)
 return dict(zip(['lookups','hits','misses','collisions','stale','attempts','installed','failures','compile_tb','compile_samples'],struct.unpack('>10Q',u.mem_read(dest,80))))
h=lambda pc:((pc>>2)^(pc>>12))&255
pcs=[base];candidate=base+32
while len(pcs)<8:
 if h(candidate)==h(base):pcs.append(candidate)
 candidate+=4
results={}
for workload in ['single_warm','eight_conflicts','length_variants']:
 setup()
 for pc in pcs:u.mem_write(ram+pc,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
 total[0]=0;active[0]=True;guest=0
 for n in range(128):
  pc=pcs[0] if workload=='single_warm' else pcs[(n//2)%8] if workload=='length_variants' else pcs[n%8]
  budget=2 if workload=='length_variants' and n%2 else 8
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  assert call('ee_core_step_n',budget)==budget;guest+=budget
 active[0]=False;assert reg2()==guest and executed()==guest
 result=profile();result['PPC_instructions']=total[0];result['guest_result']=guest;results[workload]=result
 if 'ee_jit_get_cache_entries' in syms:
  assert call('ee_jit_get_cache_entries')==4096
  assert result['installed']==(1 if workload=='single_warm' else 8 if workload=='eight_conflicts' else 16),result
# Source replacement still applies when the same PC has two block lengths.
u.mem_write(ram+pcs[0],struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|19));call('ee_jit_notify_physical_write',pcs[0],4)
for budget in [8,2]:
 word(state+off['pc'],pcs[0]);word(state+off['next_pc'],pcs[0]+4);before=reg2()
 assert call('ee_core_step_n',budget)==budget and reg2()==before+budget+18
# Also overfill a NEW four-way set: eviction remains bounded and precise.
if 'ee_jit_get_cache_entries' in syms:
 setup();new_pcs=[base];candidate=base+32
 hnew=lambda pc:((pc>>2)^(pc>>12))&1023
 while len(new_pcs)<8:
  if hnew(candidate)==hnew(base):new_pcs.append(candidate)
  candidate+=4
 for pc in new_pcs:u.mem_write(ram+pc,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
 for n in range(128):
  pc=new_pcs[n%8]|0x80000000;word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  assert call('ee_core_step_n',8)==8
 assert reg2()==1024 and executed()==1024
 assert profile()['installed']==128 and profile()['collisions']==124
 word(mask_addr,initial&~8192);setup();assert call('ee_jit_get_cache_entries')==256
 assert call('ee_core_step_n',8)==8 and reg2()==8
 word(mask_addr,initial)
 print('PASS overfull new sets evict correctly and Control retains 256 owners')
print('EE_CACHE_CAPACITY_COST',json.dumps(results,sort_keys=True))
print('PASS guest results, warm replay, overfull old sets, length variants and live source changes')
