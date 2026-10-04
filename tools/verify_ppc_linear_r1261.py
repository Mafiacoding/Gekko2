"""Rendered STQ four-tap texture tests in both emitted Wii engines."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_stq_r1260.py').read_text(),'verify_ppc_stq_r1260.py','exec'))
colors=[0x20000000,0x60402080,0xa08040c0,0xe0c06000]
for prim,uv,clamp,expected in [(6,(1.,1.),5,0x80603050),(3,(1.,1.),5,0x80603050),(6,(.5,.5),5,colors[0]),(6,(0.,.5),5,colors[0]),(6,(0.,.5),4,0x40201040)]:
 call('gif_init');call('gs_mem_init')
 tex=3000
 for y in range(2):
  for x in range(2):call('gs_mem_write_psmct32',tex*64,64,x,y,colors[y*2+x])
 ad(0x4c,1<<16);ad(0x18,0);ad(0x40,15<<16,15<<16)
 ad(6,tex|(1<<14)|(1<<26)|(1<<30),(1<<2)|(1<<3))
 ad(0x14,(1<<5)|(1<<6));ad(8,clamp)
 ad(0,prim|(1<<4))
 vertices=[(0,0),(4,4)] if prim==6 else [(0,0),(9,0),(0,9)]
 for x,y in vertices:
  ad(1,0x80808080,fbits(1.));ad(2,fbits(uv[0]/2.),fbits(uv[1]/2.));ad(5,(x<<4)|((y<<4)<<16))
 pixel=call('gs_mem_read_psmct32',0,64,1,1)
 assert pixel==expected,(prim,uv,clamp,hex(pixel),hex(expected))
print('PASS 5 actual Wii ELF linear STQ images: sprite, triangle, texel center, clamp and negative repeat')
