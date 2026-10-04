"""Rendered full-VRAM signatures and measured linked PPC GS workloads."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_linear_r1261.py').read_text(),'verify_ppc_linear_r1261.py','exec'))
import random,json,hashlib
from unicorn import UC_HOOK_CODE
active=[False];count=[0]
def count_gs(uc,address,size,data):
 if active[0]:count[0]+=1
u.hook_add(UC_HOOK_CODE,count_gs);u.ctl_remove_cache(0x80000000,0x81800000)
rng=random.Random(1285);initial=rng.randbytes(4*1024*1024);results={}
for prim in [0,1,3,6]:
 for variant in range(16):
  call('gif_init');u.mem_write(syms['g_gs_mem'],initial)
  ctx=variant%2;mask=[0,0,0xff000000,0x00f0f0f0][variant%4]
  ad(0x4c+ctx,(1<<16)|((1 if variant==14 else 0)<<24),mask)
  ad(0x18+ctx,0);ad(0x40+ctx,(1)|(22<<16),(1)|(22<<16))
  if variant>=4:ad(0x4e+ctx,3,(variant>>2)&1)
  # All four depth predicates; alpha gate variants cover all fail actions.
  test=(((variant>>1)&1)<<16)|((variant%4)<<17)
  if variant>=8:test|=1|((variant%8)<<1)|(120<<4)|((variant%4)<<12)
  ad(0x47+ctx,test);ad(0x45,int(variant==12));ad(0x44,0x01234567,0x76543210)
  ad(0x4a+ctx,int(variant==13));ad(0x49,variant&1);ad(0x22,variant%4 if variant>=8 else 0)
  ad(0x42+ctx,0x64,(64<<0)) # Blend source/destination, fixed factor.
  ad(0x3d,0x00332211)
  gouraud=(variant%3==1);fog=(variant%3==2);blend=(variant>=10)
  textured=(variant==15)
  if textured:
   texword=3000|(1<<14)|(4<<26)|(4<<30)
   ad(6+ctx,texword&0xffffffff,(texword>>32)|(1<<3));ad(8+ctx,5)
  ad(0,prim|(gouraud<<3)|(textured<<4)|(fog<<5)|(blend<<6)|(1<<8)|(ctx<<9))
  vertices={0:[(8,7)],1:[(2,3),(19,17)],3:[(2,2),(21,4),(4,21)],6:[(2,3),(22,21)]}[prim]
  commands=[]
  for n,(x,y) in enumerate(vertices):
   color=[0x40332211,0xc0668844,0x807755aa][n%3]
   commands.extend([(1,color,fbits(1.)),(0x0a,(20+n*90)<<24,0),(3,(n*48)|((n*64)<<16),0),(5,(x<<4)|((y<<4)<<16),100+n*10000)])
  count[0]=0;active[0]=True
  for reg,lo,hi in commands:ad(reg,lo,hi)
  active[0]=False
  results[f'{prim}_{variant}']={'vram_sha256':hashlib.sha256(bytes(u.mem_read(syms['g_gs_mem'],len(initial)))).hexdigest(),'PPC':count[0]}
# Normal opaque BIOS-style fills, no ancillary flags, larger rectangles.
for prim in [3,6]:
 call('gif_init');call('gs_mem_init');ad(0x4c,1<<16);ad(0x18,0);ad(0x40,127<<16,127<<16);ad(0,prim);ad(1,0x80332211)
 vertices=[(1,1),(127,1),(1,127)] if prim==3 else [(1,1),(127,127)]
 count[0]=0;active[0]=True
 for x,y in vertices:ad(5,(x<<4)|((y<<4)<<16),300)
 active[0]=False
 results[f'fill_{prim}']={'vram_sha256':hashlib.sha256(bytes(u.mem_read(syms['g_gs_mem'],4*1024*1024))).hexdigest(),'PPC':count[0]}
print('GS_PATHS_R1285',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Full shared VRAM signatures and real PPC GIF draw workloads; not Wii cycles/FPS.'}))
