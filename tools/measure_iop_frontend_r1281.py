"""Full-retirement IOP scalar costs in actual Wii ELFs, no hardware FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_iop_control_r1269.py').read_text(),'verify_ppc_iop_control_r1269.py','exec'))
import json
active=[False];total=[0]
def counter(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,counter);u.ctl_remove_cache(0x80000000,0x81800000)
cases=[('ADDIU',(9<<26)|(2<<21)|(2<<16)|1,8),('LUI',(15<<26)|(2<<16)|0x1234,0x12340000),
 ('ORI',(13<<26)|(2<<21)|(2<<16)|1,1),('ANDI',(12<<26)|(2<<21)|(2<<16)|7,0),('XORI',(14<<26)|(2<<21)|(2<<16)|1,0),
 ('ADDU',(3<<21)|(2<<16)|(2<<11)|0x21,8),('SLL',(3<<16)|(2<<11)|(2<<6),4),
 ('SRL',(3<<16)|(2<<11)|(1<<6)|2,0),('SRA',(3<<16)|(2<<11)|(1<<6)|3,0),
 ('SLLV',(3<<21)|(3<<16)|(2<<11)|4,2),('SRLV',(3<<21)|(3<<16)|(2<<11)|6,0),('SRAV',(3<<21)|(3<<16)|(2<<11)|7,0),
 ('AND',(2<<21)|(3<<16)|(2<<11)|0x24,0),('OR',(2<<21)|(3<<16)|(2<<11)|0x25,1),
 ('XOR',(2<<21)|(3<<16)|(2<<11)|0x26,0),('NOR',(2<<21)|(3<<16)|(2<<11)|0x27,0),
 ('SLT',(2<<21)|(3<<16)|(2<<11)|0x2a,0),('SLTU',(2<<21)|(3<<16)|(2<<11)|0x2b,0),('SUBU',(2<<21)|(3<<16)|(2<<11)|0x23,0xfffffff8)]
results={}
for name,iw,expected in cases:
 if enabled:
  u.mem_write(syms['code_cache'],bytes(1024*8));u.mem_write(syms['pc_cache'],bytes(2048*16));word(syms['cache_size'],0)
 for n in range(8):u.mem_write(io_ram+0x6000+4*n,struct.pack('<I',iw))
 samples=[]
 for iteration in range(4):
  call('iop_intc_init');call('iop_timers_init')
  u.mem_write(ios,bytes(128));word(ios+12,1)
  word(ios+ioff['pc'],0x80006000);word(ios+ioff['next_pc'],0x80006004)
  u.mem_write(ios+ioff['halted'],b'\0');u.mem_write(ios+ioff['idle'],b'\0');word(ios+ioff['cop0']+12*4,0)
  total[0]=0;active[0]=True
  for n in range(8):call('iop_core_step')
  active[0]=False
  got=int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')
  assert got==expected,(name,got,expected)
  assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==0x80006020
  assert bytes(u.mem_read(ios,4))==bytes(4)
  samples.append(total[0])
 results[name]={'cold':samples[0],'warm':samples[1:]}
print('IOP_SCALAR_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Eight actual IOP instructions including CPU retirement; mocked allocation/cache maintenance; not Wii cycles/FPS.'}))
