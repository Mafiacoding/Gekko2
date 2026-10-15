"""Linked PPC: warm dispatch cost and source mutation at every block slot.
No hardware timing claim; each slot must retain its live pre-execution guard.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction sequences')[0],'iop_fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
word(mask,(initial|2|(1<<13))&~(1<<30))
program=[(9<<26)|(2<<21)|(2<<16)|1]*16
checks=0
for index in range(8):
 for budget in [1,3,8]:
  for replacement in [(9<<26)|(2<<21)|(2<<16)|7,12]:
   states=[]
   for native in [False,True]:
    setup(program)
    if native:assert call('iop_core_step_n',8)==8
    else:
     for _ in range(8):call('iop_core_step')
    u.mem_write(iram+ibase+index*4,struct.pack('<I',replacement))
    seen=[]
    for grant in [budget,8]:
     word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
     if native:assert call('iop_core_step_n',grant)==grant
     else:
      for _ in range(grant):call('iop_core_step')
     seen.append(snapshot())
    states.append(seen)
   assert states[0]==states[1],('changed source',index,budget,hex(replacement))
   checks+=1
print('PASS',checks,'warm-owner mutations, including unused tails, short grants and syscall recovery')
costs={}
for budget in [1,2,4,8]:
 setup(program);assert call('iop_core_step_n',8)==8
 total[0]=0;active[0]=True
 for _ in range(100):
  word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
  assert call('iop_core_step_n',budget)==budget
 active[0]=False
 costs[budget]={'ppc_instructions':total[0],'state_sha256':hashlib.sha256(snapshot()).hexdigest()}
print('WARM_DISPATCH_COST',json.dumps(costs,sort_keys=True))
setup(program);assert call('iop_core_step_n',1)==1
total[0]=0;active[0]=True
for _ in range(100):
 word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
 assert call('iop_core_step_n',1)==1
active[0]=False
print('ONE_WORD_OWNER_COST',json.dumps({'ppc_instructions':total[0],'state_sha256':hashlib.sha256(snapshot()).hexdigest()},sort_keys=True))
