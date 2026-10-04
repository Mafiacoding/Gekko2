"""Controlled full-retirement EE scalar costs in the actual Wii ELF; no FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_cache_r1267.py').read_text(),'verify_ppc_cache_r1267.py','exec'))
import json
active=[False];total=[0]
def count_hook(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,count_hook);u.ctl_remove_cache(0x80000000,0x81800000)
cases=[('ADDIU',(9<<26)|(2<<21)|(2<<16)|1,8),
 ('LUI',(15<<26)|(2<<16)|0x1234,0x12340000),
 ('ORI',(13<<26)|(2<<21)|(2<<16)|1,1),
 ('ANDI',(12<<26)|(2<<21)|(2<<16)|7,0),
 ('XORI',(14<<26)|(2<<21)|(2<<16)|1,0),
 ('ADDU',(3<<21)|(2<<16)|(2<<11)|0x21,8),
 ('SLL',(3<<16)|(2<<11)|(2<<6),4)]
results={}
for name,iw,expected in cases:
 reset_frontend()
 for n in range(8):guest(0x80006000+4*n,iw)
 samples=[]
 for iteration in range(4):
  word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
  word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
  u.mem_write(state,bytes(512));u.mem_write(state+48,(1).to_bytes(8,'big')+bytes(8))
  before_count=int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')
  total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
  assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==(before_count+8)&0xffffffff
  assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006020
  assert bytes(u.mem_read(state,16))==bytes(16)
  actual=int.from_bytes(bytes(u.mem_read(state+32,8)),'big')
  assert retired==8 and actual==expected,(name,retired,actual,expected)
  samples.append(total[0])
 results[name]={'cold':samples[0],'warm':samples[1:]}
print('EE_SCALAR_MEASURE',json.dumps({'elf':Path(a.elf).name,'retired_per_sample':8,'results':results,'scope':'PPC instructions including CPU retirement; mocked allocation/cache maintenance. No Wii timing or BIOS workload claim.'}))
