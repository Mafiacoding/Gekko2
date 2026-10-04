"""Run homogeneous STQ pixel regressions in the actual PowerPC Wii ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_packed_context.py').read_text(), 'verify_ppc_packed_context.py', 'exec'))
def fbits(x):
 return struct.unpack('<I',struct.pack('<f',x))[0]
for qs, expected_x, expected_y in [([1.,1.,4.],2,3),([2.,2.,2.],2,3),([1.,1.,1.],3,6)]:
 call('gif_init');call('gs_mem_init')
 tex=3000
 for y in range(8):
  for x in range(16):
   call('gs_mem_write_psmct32',tex*64,64,x,y,0x80000000|(y*20<<8)|x*10)
 ad(0x4c,1<<16);ad(0x18,0);ad(0x40,15<<16,15<<16)
 ad(0x06,tex|(1<<14)|(4<<26)|(3<<30),1<<3)
 ad(0,3|(1<<4)) # Textured triangle, FST=0.
 for (x,y),s,q in zip([(0,0),(9,0),(0,9)],[0.,9./16.,0.],qs):
  ad(1,0x80808080,fbits(q));ad(2,fbits(s),fbits(6./8.));ad(5,(x<<4)|((y<<4)<<16))
 pixel=call('gs_mem_read_psmct32',0,64,3,3)
 assert pixel&0xffffff == (expected_y*20<<8)|expected_x*10, (qs,hex(pixel))
print('PASS 3 actual Wii ELF STQ triangle pixels: varying Q, constant Q=2, Q=1; both S and T')
