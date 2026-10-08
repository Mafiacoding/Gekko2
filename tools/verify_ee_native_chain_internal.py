"""Linked native warm-cache chains, exact EE budget and source/IRQ exits."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
sig=hashlib.sha256();cases=0
native='ee_precise_cached_next' in syms and enabled
# Compile a J+DS predecessor and a six-instruction successor, then reuse
# those actual generated functions from the native PPC continuation thunk.
for budget in range(2,9):
 for changed in [False,True]:
  for irq in [0,1,2,3,4,5,6,7,8]:
   extension_setup()
   u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)))
   u.mem_write(ram+base+4,struct.pack('<I',(9<<26)|(6<<16)|77))
   for n in range(8):u.mem_write(ram+base+64+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
   assert call('ee_core_step_n',8)==8
   word(state+off['pc'],base);word(state+off['next_pc'],base+4)
   word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,irq)
   word(state+off['cop0']+12*4,0x18001 if irq else 0)
   u.mem_write(state+4*16,bytes(16))
   if changed:u.mem_write(ram+base+68,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|9))
   before=executed();assert call('ee_core_step_n',budget)==budget and executed()==before+budget
   sig.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8)))
   cases+=1
# Warm, fully valid eight-instruction case MUST use a cached native successor.
if native:
 extension_setup();u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)))
 for n in range(8):u.mem_write(ram+base+64+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
 assert call('ee_core_step_n',8)==8
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 u.mem_write(state+4*16,bytes(16));before=executed()
 hi=call('ee_jit_get_native_successors');prior=(hi<<32)|u.reg_read(UC_PPC_REG_4)
 assert call('ee_core_step_n',8)==8 and executed()==before+8
 hi=call('ee_jit_get_native_successors');after=(hi<<32)|u.reg_read(UC_PPC_REG_4)
 assert after==prior+1,(prior,after)
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 # Direct thunk ABI, plus allocator failure on a fresh cold continuation.
 fn=precise_native_for_pc(base,2)
 thunk=int.from_bytes(bytes(u.mem_read(syms['precise_chain_fn'],4)),'big')
 assert fn and thunk
 saved=[0xa1000000+n for n in range(18)]
 for n,v in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,v)
 # Enter the thunk directly so outer C saves cannot hide ABI damage.
 syms['internal_chain_abi']=thunk
 word(syms['precise_active'],1)
 try:assert call('internal_chain_abi',state,0,0,8,fn,2)==8
 finally:word(syms['precise_active'],0)
 assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved
 extension_setup();fail_alloc=True
 u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)))
 assert call('ee_core_step_n',8)==8
 fail_alloc=False
 print('PASS required warm native successor, all nonvolatile registers and allocation-failure fallback')
print('EE_NATIVE_CHAIN_SIGNATURE '+sig.hexdigest())
print(f'PASS {cases} cold/warm chain budgets, modified successor source and timer boundaries')
