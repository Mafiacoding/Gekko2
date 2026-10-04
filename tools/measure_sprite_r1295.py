"""Actual PPC software sprite costs with independent full-VRAM signatures."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_MSR
u.reg_write(UC_PPC_REG_MSR,0x2000)
import hashlib,json
packet=0x81740000;active=[False];total=[0]
def counter(uc,address,size,user):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,counter)
def ad(reg,lo,hi=0):
 blob=struct.pack('<4I',0x8001,1<<28,14,0)+struct.pack('<4I',lo,hi,reg,0)
 u.mem_write(packet,blob)
 call('gif_process_quadwords',2,packet,2)
results={}
for kind in ['opaque_masked_Z','nearest_texture','linear_texture']:
 call('gif_init');call('gs_mem_init');u.mem_write(syms['g_gs_mem']+0x200000,b'\x10\x40\x70\x80'*16384)
 ad(0x4c,2<<16);ad(0x18,0);ad(0x40,127<<16,63<<16)
 ad(0x4e,128,1);ad(0x47,(1<<16)|(1<<17))
 textured=kind!='opaque_masked_Z'
 if textured:
  ad(6,8192|(1<<14)|(5<<26)|(1<<30),1|4) # TH=5, TCC=1, MODULATE
  ad(8,0);ad(0x14,0x60 if kind=='linear_texture'else 0)
 ad(0,6|256|(16 if textured else 0));ad(1,0x80808080);ad(3,0);ad(5,0)
 ad(3,(32<<4)|((32<<4)<<16))
 total[0]=0;active[0]=True;ad(5,(128<<4)|((64<<4)<<16));active[0]=False
 results[kind]={'PPC':total[0],'vram_sha256':hashlib.sha256(bytes(u.mem_read(syms['g_gs_mem'],4*1024*1024))).hexdigest()}
print('SPRITE_R1295 '+json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'128x64 GIF sprite, real emitted PPC; GX disabled. CPU counters, not Wii FPS.'}))
