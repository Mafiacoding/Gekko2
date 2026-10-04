"""Real CPU retirement for memory ops: warm JIT cache dispatch changes only.
Mocked allocation/cache maintenance; PPC instruction counts, no hardware FPS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_frontend_r1276.py').read_text(),'measure_ee_frontend_r1276.py','exec'))
results={}
for name,op,expected,width,store in [('LB',32,0xffffffffffffffef,1,False),('LH',33,0xffffffffffffcdef,2,False),('LW',35,0xffffffff90abcdef,4,False),('LWU',39,0x90abcdef,4,False),('SW',43,0x90abcdef,4,True),('SD',63,0x1234567890abcdef,8,True)]:
 reset_frontend()
 for n in range(8):guest(0x80006000+n*4,(op<<26)|(3<<21)|(2<<16))
 samples=[]
 for iteration in range(4):
  word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
  word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
  u.mem_write(state,bytes(512));u.mem_write(state+48,(0x80007400).to_bytes(8,'big')+bytes(8))
  u.mem_write(ram+0x7400,(0x1234567890abcdef if not store else 0).to_bytes(8,'little'))
  if store:u.mem_write(state+32,(0x1234567890abcdef).to_bytes(8,'big')+bytes(8))
  total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
  actual=int.from_bytes(bytes(u.mem_read(ram+0x7400,width)),'little') if store else int.from_bytes(bytes(u.mem_read(state+32,8)),'big')
  assert retired==8 and actual==expected,(name,retired,actual,expected)
  assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006020
  samples.append(total[0])
 results[name]={'cold':samples[0],'warm':samples[1:]}
print('EE_MEMORY_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Eight full-retirement RAM instructions; PPC counts, no BIOS workload or real Wii cycles/FPS.'}))
