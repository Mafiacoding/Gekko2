"""IOP short-owner growth, changed source and precise mixed-budget states."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction')[0],'iop_growth_fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
words=[(9<<26)|(2<<21)|(2<<16)|1]*16
snapshots=[];growths=[]
for mode in [0,1,2]:
 word(mask,(initial|(1<<13))&~(1<<30)|((1<<30) if mode==2 else 0))
 setup(words);states=[]
 for j,budget in enumerate([1,2,4,8,3,1,8]):
  word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
  if j==6:u.mem_write(iram+ibase+12,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|7))
  if mode:assert call('iop_core_step_n',budget)==budget
  else:
   for _ in range(budget):call('iop_core_step')
  states.append(snapshot())
 call('iop_jit_get_block_growths');growths.append((u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4))
 snapshots.append(states)
assert snapshots[0]==snapshots[1]==snapshots[2]
assert growths==[0,0,3],growths
print('PASS 21 mixed-budget complete IOP states,1->2->4->8 owner growth and changed-source recompilation')
print('IOP_GROWTH_COUNTS',growths)
