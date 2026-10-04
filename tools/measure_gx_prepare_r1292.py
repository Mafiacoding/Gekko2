"""Actual linked PPC GS target preparation work; no Wii timing/FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
from unicorn import UC_HOOK_CODE
import json
u.mem_map(0x90000000,0x10000)
xy=0x90002000;out=0x90003000
u.mem_write(xy,struct.pack('>6i',0,0,320,256,0,0))
total=[0]
def tick(uc,address,size,user):total[0]+=1
h=u.hook_add(UC_HOOK_CODE,tick)
u.reg_write(UC_PPC_REG_1,0x81780000)
args=[out,6,0,640,0,0,320,256,xy,0x80112233,0]
for i,v in enumerate(args[:8]):u.reg_write(UC_PPC_REG_3+i,v)
for i,v in enumerate(args[8:]):u.mem_write(0x81780008+i*4,struct.pack('>I',v))
u.reg_write(UC_PPC_REG_LR,0x817ff000)
u.emu_start(syms['gs_gx_prepare_flat'],0x817ff000,count=50000000)
assert u.reg_read(UC_PPC_REG_PC)==0x817ff000 and u.reg_read(UC_PPC_REG_3)==1
u.hook_del(h)
print('GX_PREPARE_BENCH '+json.dumps({'elf':Path(a.elf).name,'width':320,'height':256,'PPC':total[0],'plan_sha256':__import__('hashlib').sha256(bytes(u.mem_read(out,2076))).hexdigest(),'scope':'isolated linked-PPC preparation; no GPU work or Wii FPS'}))
