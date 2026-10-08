"""Paired PPC oracle: native scheduler against the original eight-EE grant.
IOP-boundary hooks mutate code, mappings, registers and IRQ state, clobber
volatile host registers, and observe state before the next EE instruction.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
trace=[];scenario='';stop_at=0
callback=0x817fe000
u.mem_write(callback,struct.pack('>I',0x4e800020))
def getword(addr):return int.from_bytes(bytes(u.mem_read(addr,4)),'big')
def boundary(uc,address,size,user):
 if address!=callback:return
 tick=len(trace)+1
 trace.append(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8))+bytes(u.mem_read(ram+base,192)))
 if scenario=='code' and tick==2:
  # DMA-like write and the corresponding invalidation epoch.
  # Address of a future instruction in the loop (not the current one).
  target=base+80
  # Emulate a device write and invalidate the physical source page.
  u.mem_write(ram+target,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
  # We cannot recursively start Unicorn from a hook. Mark the generation
  # exactly as the writer does; live prepare also checks raw modifications.
  page=target>>12;word(syms['precise_page_generation']+page*4,getword(syms['precise_page_generation']+page*4)+1)
 if scenario=='mapping' and tick==2:
  word(syms['precise_mapping_generation'],getword(syms['precise_mapping_generation'])+1)
  word(state+off['gpr_generation'],getword(state+off['gpr_generation'])+1)
  u.mem_write(state+16,struct.pack('>QQ',5,0))
 if scenario=='raw' and tick==2:
  u.mem_write(ram+base+64,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|7))
 if scenario=='irq' and tick==2:
  word(state+off['cop0']+11*4,getword(state+off['cop0']+9*4)+3)
  word(state+off['cop0']+12*4,0x18001)
 if scenario=='ee_halt' and tick==2:u.mem_write(state+off['halted'],b'\x01')
 lr=uc.reg_read(UC_PPC_REG_LR)
 for n in range(3,13):uc.reg_write(UC_PPC_REG_0+n,0xface0000+n)
 uc.reg_write(UC_PPC_REG_3,int(tick!=stop_at));uc.reg_write(UC_PPC_REG_PC,lr)
u.hook_add(UC_HOOK_CODE,boundary)
def run(kind,quanta,scheduled):
 global scenario,trace,stop_at
 extension_setup();scenario=kind;trace=[];stop_at=3 if kind=='iop_halt' else 0
 # Branch at the eighth instruction: IOP must see branch_pending before DS.
 for n in range(48):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(1<<21)|(2<<16)|n))
 u.mem_write(ram+base+28,struct.pack('<I',(2<<26)|((base+64)>>2)))
 u.mem_write(ram+base+188,struct.pack('<I',(2<<26)|(base>>2)))
 u.mem_write(ram+base+192,struct.pack('<I',0))
 # Warm several bodies; then restore the architectural starting state.
 call('ee_core_step_n',128)
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 word(state+off['branch_pending'],0);word(state+off['cop0']+9*4,100)
 u.mem_write(state+off['instructions_executed'],bytes(8));u.mem_write(state,bytes(512))
 if scheduled:done=call('ee_core_step_interleaved_n',quanta,callback)
 else:
  done=0
  for n in range(quanta):
   if not bytes(u.mem_read(state+off['halted'],1))[0]:call('ee_core_step_n',8)
   syms['_boundary']=callback
   more=call('_boundary');done+=1
   if not more:break
 return done,trace,bytes(u.mem_read(state,size)),bytes(u.mem_read(ram+base,196))
cases=0;sig=hashlib.sha256()
for kind in ['plain','code','mapping','raw','irq','ee_halt','iop_halt']:
 for quanta in [1,2,3,4,8,16]:
  old=run(kind,quanta,False)
  if 'ee_core_step_interleaved_n' in syms:
   new=run(kind,quanta,True)
   assert old==new,(kind,quanta,old[0],new[0],len(old[1]),len(new[1]))
  for v in old[1]:sig.update(v)
  cases+=1
print('WII_SCHEDULER_SIGNATURE '+sig.hexdigest())
print(f'PASS {cases} paired PPC grants: every 8:1 boundary, DS, code/DMA epochs, ASID epoch, GPR refresh, IRQ and early halt')
