#!/usr/bin/env python3
"""Real linked PPC GIF vertex queues and software GS line pixels; no GPU claim."""
from pathlib import Path
source=Path(__file__).with_name('verify_beta12_22_linked.py').read_text()
exec(compile(source.split('_,vo=')[0],'fixture','exec'))
inp=ram+0x1000
word(syms['gekko2_optimization_mask'],0)
def ad(reg,lo,hi=0):
 u.mem_write(inp,struct.pack('<8I',0x8001,1<<28,14,0,lo&0xffffffff,hi&0xffffffff,reg,0))
 call('gif_process_quadwords',2,inp,2)
def fbits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def vertex(x,uv,s,q,draw):
 ad(1,0x80808080,fbits(q));ad(2,fbits(s),0);ad(3,int(uv*16));ad(5 if draw else 13,(x*16)|((12*16)<<16))
checks=0
for context in [0,1]:
 for strip in [0,1]:
  for fst in [0,1]:
   for varying in [0,1]:
    call('gs_mem_init');call('gif_init')
    ad(0x4c+context,1<<16);ad(6+context,64|(1<<14)|(2<<26),12);ad(8+context,0)
    ad(0,(2 if strip else 1)|16|(256 if fst else 0)|(context<<9))
    for x in range(4):call('gs_mem_write_psmct32',4096,64,x,0,0x80010210+x)
    vertex(10,0,0,1,False);vertex(14,4,4 if varying else 1,4 if varying else 1,True)
    for i in range(5):
     t=i/4;q=1+t*(3 if varying else 0);uv=i if fst else t*(4 if varying else 1)*4/q
     expected=0x80010210+(int(uv)&3)
     got=call('gs_mem_read_psmct32',0,64,10+i,12)
     assert got==expected,(context,strip,fst,varying,i,hex(got),hex(expected));checks+=1
    if strip:
     vertex(18,0,0,1,True);assert call('gs_mem_read_psmct32',0,64,18,12)==0x80010210;checks+=1
print('PASS',checks,'actual PPC LINE/STRIP pixels, both GS contexts, UV/STQ, varying Q and XYZ3 queue advance')
