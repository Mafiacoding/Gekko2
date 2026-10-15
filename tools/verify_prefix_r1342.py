"""Actual linked PPC short grants: compare complete EE state with prefixes OFF/ON."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
def ins(op,rs=0,rt=0,imm=0):return (op<<26)|(rs<<21)|(rt<<16)|(imm&65535)
programs=[
 [ins(9,2,2,1)]*8,
 [ins(9,2,2,1)]*5+[ins(4,imm=2),ins(9,3,3,2)],
 [ins(15,rt=1,imm=0x8028),ins(9,2,2,7),ins(43,1,2),ins(35,1,3)]+[ins(9,4,4,1)]*4]
signatures={};costs=[0,0];checks=0
for enabled in [0,1]:
 word(mask_addr,(initial|8192|8)&~(1<<17)&~(1<<19)|(enabled<<19))
 for kind,program in enumerate(programs):
  for budget in range(2,min(8,len(program))):
   setup();pc=base|0x80000000
   u.mem_write(ram+0x280000,bytes(64))
   u.mem_write(ram+base,struct.pack('<'+str(len(program))+'I',*program))
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   assert call('ee_core_step_n',len(program))==len(program)
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4);word(state+off['branch_pending'],0)
   total[0]=0;active[0]=True
   assert call('ee_core_step_n',budget)==budget
   active[0]=False;costs[enabled]+=total[0]
   snapshot=bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+0x280000,64))
   if not enabled:signatures[(kind,budget)]=snapshot
   else:assert signatures[(kind,budget)]==snapshot,(kind,budget)
   if enabled:
    call('ee_jit_get_budget_cache_stat',6);assert u.reg_read(UC_PPC_REG_4)>0,(kind,budget)
   checks+=1
print('PREFIX_COST',json.dumps({'off':costs[0],'on':costs[1],'cold_prefix_compilation_included':True}))
print('PASS',checks,'linked PPC grants: complete state, branches/delay slots and RAM stores/loads')

# Warm execution cost; the one-time bounded-body compilation is excluded.
warm=[0,0]
for enabled in [0,1]:
 word(mask_addr,(initial|8192|8)&~(1<<17)&~(1<<19)|(enabled<<19))
 setup();pc=base|0x80000000
 u.mem_write(ram+base,struct.pack('<8I',*([ins(9,2,2,1)]*8)))
 def grant(n):
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  assert call('ee_core_step_n',n)==n
 grant(8)
 for budget in range(2,8):grant(budget)
 total[0]=0;active[0]=True
 for iteration in range(100):
  for budget in range(2,8):grant(budget)
 active[0]=False;warm[enabled]=total[0]
 assert reg2()==8+27+2700
print('WARM_PREFIX_COST',json.dumps({'off':warm[0],'on':warm[1],'retired_per_mode':2735}))
# Invalidate a warmed prefix owner and prove that the new first word executes.
call('ee_jit_notify_physical_write',base,4)
u.mem_write(ram+base,struct.pack('<I',ins(9,2,2,9)))
before=reg2();grant(3);assert reg2()==before+11
call('ee_jit_reset_stats_for_test')
print('PASS warm prefix reuse, first-word mutation and prefix allocation teardown')
