"""Independent byte oracle for bounded PSMCT32 row fills in actual Wii ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
import random
rng=random.Random(1285);initial=rng.randbytes(4*1024*1024);cases=0
for bw in [0,64,128,640]:
 for x,y in [(0,0),(1,1),(7,7),(63,31),(64,32),(127,255)]:
  for bp in [0,64,1048575]:
   count=[0,1,2,79,640][cases%5];color=rng.getrandbits(32);expected=bytearray(initial)
   for n in range(count):
    px=x+n;bx=(px&63)>>3;by=(y&31)>>3
    block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2)
    wordx=(px&1)|((y&1)<<1)|((px&6)<<1)|((y&6)<<3)
    off=(bp*4+((y//32)*max(1,bw//64)+px//64)*8192+block*256+wordx*4)&0xffffffff
    if off<=len(initial)-4:expected[off:off+4]=color.to_bytes(4,'little')
   u.mem_write(syms['g_gs_mem'],initial)
   call('gs_mem_fill_psmct32_span',bp,bw,x,y,count,color)
   assert bytes(u.mem_read(syms['g_gs_mem'],len(initial)))==bytes(expected),(bw,x,y,bp,count)
   cases+=1
print('PASS',cases,'actual Wii ELF PSMCT32 fill full-memory byte oracles: endian, pages, odd X, zero count and VRAM bounds')
