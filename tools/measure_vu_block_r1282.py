"""Actual Wii ELF VU block dispatch costs; no hardware FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_block_r1272.py').read_text(),'verify_ppc_block_r1272.py','exec'))
import json
active=[False];total=[0]
def count_hook(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,count_hook);u.ctl_remove_cache(0x80000000,0x81800000)
forms=[('VI_ADD',up1,lo),('I_MUL',(15<<21)|(1<<11)|(3<<6)|0x1d|0x80000000,0x40000000),
       ('ACC',(15<<21)|(2<<16)|(1<<11)|0x3c,lo),
       ('LQ',up1,(15<<21)|(4<<16)|(1<<11)),
       ('SQ',up1,(1<<25)|(15<<21)|(1<<16)|(3<<11))]
results={}
for name,up,low in forms:
 for count in [2,4,8]:
  samples=[]
  for iteration in range(4):
   setup();u.mem_write(acc,bytes(16));u.mem_write(mem,bytes(16384));
   base=count*128;word(pcp,base)
   for n in range(count):put(base+n*8,low,up)
   put(base+count*8,lo,0x400002ff)
   total[0]=0;active[0]=True;got=block(count);active[0]=False
   assert got==(count if enabled else 0),(name,count,got)
   if enabled:
    assert int.from_bytes(bytes(u.mem_read(pcp,4)),'big')==base+count*8
    assert int.from_bytes(bytes(u.mem_read(counter,8)),'big')==count
   samples.append(total[0])
  results[name+'_'+str(count)]={'cold':samples[0],'warm':samples[1:]}
print('VU_BLOCK_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'PPC instructions for actual bounded VU block call, including dispatch; not Wii cycles/FPS or BIOS workload.'}))
