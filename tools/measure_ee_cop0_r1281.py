"""COP0 read/write CPU frontend cost with full retirement, no FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_frontend_r1276.py').read_text(),'measure_ee_frontend_r1276.py','exec'))
results={}
for name,rs,rd in [('MFC0_COUNT',0,9),('MTC0_COMPARE',4,11),('MTC0_CONFIG',4,16)]:
 reset_frontend();call('ee_timers_init');call('ee_intc_init')
 for n in range(8):guest(0x80006000+4*n,(16<<26)|(rs<<21)|(2<<16)|(rd<<11))
 samples=[]
 for k in range(4):
  word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
  word(state+off['cop0']+12*4,0);word(state+off['cop0']+9*4,100)
  u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0');u.mem_write(state,bytes(512))
  value=0x7fffffff if rd==11 else 0xffffffff
  if rs==4:u.mem_write(state+32,value.to_bytes(8,'big')+bytes(8))
  if rd==11:word(state+off['cop0']+13*4,0x8000)
  total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
  assert retired==8 and int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==108
  if rs==0:assert int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==107
  if rd==11:
   assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+11*4,4)),'big')==0x7fffffff
   assert not(int.from_bytes(bytes(u.mem_read(state+off['cop0']+13*4,4)),'big')&0x8000)
  if rd==16:assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+16*4,4)),'big')==0xfffff47f
  samples.append(total[0])
 results[name]={'cold':samples[0],'warm':samples[1:]}
print('EE_COP0_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Eight actual EE COP0 instructions and full retirement. PPC counts, not real Wii cycles/FPS or BIOS workload.'}))
