"""Linked PPC constant retirement exits at every live source-check boundary.
Includes inherited complete ALU/COP1, IRQ, mapping, budget and far-call tests.
No physical Wii performance claim; platform allocation/cache services mocked.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ee_calls_r1301.py').read_text(),'verify_ee_calls_r1301.py','exec'))
if enabled:
 from unicorn.ppc_const import UC_PPC_REG_14
 for count in range(2,9):
  for stop in range(count):
   setup();before=executed();seen=[0]
   def mutate(uc,address,size,user):
    if address==syms['ee_core_block_prepare']:
     seen[0]+=1
     if seen[0]==stop+1:
      uc.mem_write(ram+base+4*stop,struct.pack('<I',(9<<26)|(2<<16)|77))
   # Every nonvolatile register must survive success and every early exit.
   saved=[0xa1000000+0x101*n for n in range(18)]
   for n,v in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,v)
   hook=u.hook_add(UC_HOOK_CODE,mutate)
   result=call('ee_jit_try_execute_block',state,count)
   u.hook_del(hook)
   assert result==stop,(count,stop,result)
   assert executed()==before+stop and reg2()==stop,(count,stop)
   assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+4*stop
   assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved,(count,stop,'ABI')
   # Interpreter continuation must execute the replacement, not stale code.
   assert call('ee_core_step_n',1)==1 and reg2()==77
  setup();before=executed()
  saved=[0xb2000000+0x101*n for n in range(18)]
  for n,v in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,v)
  assert call('ee_jit_try_execute_block',state,count)==count
  assert executed()==before+count and reg2()==count
  assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved
 print('PASS 35 early source exits + seven full exits, exact retirement/PC and all 18 nonvolatile PPC registers')
