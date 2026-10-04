"""Actual Wii ELF GX texture packing, independent tiled-byte oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_scanout_r1282.py').read_text(),'verify_scanout_r1282.py','exec'))
u.mem_write(syms['g_gs_mem'],vram) # Reset writes made by inherited filter tests.
checks=0
for width,height in [(1,1),(3,5),(4,4),(7,9),(64,33),(640,4),(1023,3),(1024,8),(3,512)]:
 tw=(width+3)&~3;th=(height+3)&~3;size=tw*th*4;sx=63;sy=31;bp=64;bw=1024
 # Independent pixel swizzle from PS2 block/column definition.
 def pixel(x,y):
  bx=(x&63)>>3;by=(y&31)>>3
  block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2)
  wordx=(x&1)|((y&1)<<1)|((x&6)<<1)|((y&6)<<3)
  off=(bp*4+((y//32)*(bw//64)+x//64)*8192+block*256+wordx*4)&0xffffffff
  return int.from_bytes(vram[off:off+4],'little') if off<=len(vram)-4 else 0
 def clamp(x):return max(16,min(240,x&255))
 expected=bytearray(size)
 for y in range(th):
  for x in range(tw):
   p=pixel(sx+min(x,width-1),sy+min(y,height-1))
   off=((y//4)*(tw//4)+x//4)*64+((y%4)*4+x%4)*2
   expected[off:off+2]=bytes([255,clamp(p)])
   expected[off+32:off+34]=bytes([clamp(p>>8),clamp(p>>16)])
 u.mem_write(out-4,b'ABCD');u.mem_write(out+size,b'WXYZ')
 for cap in [size-1,size]:
  u.mem_write(out,bytes([0xa5])*size)
  got=call('gs_gx_pack_rgba8',out,cap,bp,bw,sx,sy,width,height)
  assert got==(size if cap==size else 0)
  assert bytes(u.mem_read(out,size))==(bytes(expected) if cap==size else bytes([0xa5])*size)
  assert bytes(u.mem_read(out-4,4))==b'ABCD' and bytes(u.mem_read(out+size,4))==b'WXYZ'
  checks+=1
# Repack sees changed shared VRAM, not stale texture data.
call('gs_mem_write_psmct32',64,1024,63,31,0x00112233)
assert call('gs_gx_pack_rgba8',out,64,64,1024,63,31,1,1)==64
assert bytes(u.mem_read(out,2))==bytes([255,0x33]) and bytes(u.mem_read(out+32,2))==bytes([0x22,0x11])
for w,h in [(0,1),(1,0),(1025,1),(1,513),(0xffffffff,1)]:
 assert call('gs_gx_pack_rgba8',out,0xffffffff,64,1024,63,31,w,h)==0
 checks+=1
print('PASS',checks+1,'GX RGBA8 packing byte-oracle/guard cases: AR/GB tiles, edges, capacity, current VRAM, dimensions. GPU execution remains hardware-unverified.')
