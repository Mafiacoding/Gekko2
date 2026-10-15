"""Actual PPC warm EE arithmetic blocks; exact retirement and dispatch cost.
Compare the same program on R1364 and R1365; no physical Wii FPS claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
word(mask,initial|1|8|512|8192|(1<<20)|(1<<24)|(1<<27))
results={}
for n in [8,16,32,64]:
 if n>64:continue
 setup();pc=base|0x80000000
 program=[(9<<26)|(2<<21)|(2<<16)|1]*n
 u.mem_write(ram+base,struct.pack('<'+str(n)+'I',*program))
 word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
 u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8))
 call('ee_timers_init');call('ee_timers_tick')
 retired=call('ee_jit_try_execute_block',state,n)
 if retired!=n:continue
 total[0]=0;active[0]=True
 for iteration in range(100):
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  assert call('ee_jit_try_execute_block',state,n)==n
 active[0]=False
 assert executed()==n*101
 assert int.from_bytes(u.mem_read(state+32,8),'big')==n*101
 results[n]={'ppc_instructions':total[0],'retired':executed(),'state_sha256':hashlib.sha256(bytes(u.mem_read(state,size))).hexdigest()}
print(json.dumps(results,sort_keys=True))
