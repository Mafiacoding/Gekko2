"""Actual Wii ELF straight VU block retirement/boundaries and micro SMC."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_pair_r1271.py').read_text(),'verify_ppc_pair_r1271.py','exec'))
up1=(15<<21)|(2<<16)|(1<<11)|(3<<6)|0x28
up2=(15<<21)|(2<<16)|(3<<11)|(4<<6)|0x2a
lo=(0x40<<25)|(2<<16)|(1<<11)|(3<<6)|0x30
mc=vs+voff['micro'];counter=vs+voff['instructions_executed'];ebit=vs+voff['ebit_delay'];pcp=vs+voff['tpc'];dp=vs+voff['branch_delay'];tp=vs+voff['branch_target']
def put(at,l,h):u.mem_write(mc+at,struct.pack('<2I',l,h))
def setup():
 for k in range(4):word(vf+16+k*4,0x3f800000);word(vf+32+k*4,0x40000000);word(vf+48+k*4,0);word(vf+64+k*4,0)
 word(vi+4,7);word(vi+8,9);word(pcp,0);word(dp,0);word(tp,0);word(ebit,0);u.mem_write(counter,bytes(8))
 put(0,lo,up1);put(8,lo,up2);put(16,lo,0x400002ff)
def block(budget=8,data=mem):
 for off,value in [(8,dp),(12,tp),(16,ebit),(20,counter),(24,budget)]:word(0x81780000+off,value)
 return call('vu_jit_try_block',vf,vi,acc,data,16383,mc,16383,pcp)
setup();assert block()==(2 if enabled else 0)
if enabled:
 assert int.from_bytes(bytes(u.mem_read(pcp,4)),'big')==16
 assert int.from_bytes(bytes(u.mem_read(counter,8)),'big')==2
 assert struct.unpack('>4I',bytes(u.mem_read(vf+64,16)))==(0x40c00000,)*4
 setup();before=allocs;assert block()==2;assert allocs==before
 setup();put(8,lo,(15<<21)|(2<<16)|(3<<11)|(4<<6)|0x2c)
 assert block()==2
 assert struct.unpack('>4I',bytes(u.mem_read(vf+64,16)))==(0x3f800000,)*4
for guard in [dp,ebit]:
 setup();word(guard,1);assert block()==0;assert bytes(u.mem_read(counter,8))==bytes(8)
setup();assert block(1)==0;assert bytes(u.mem_read(counter,8))==bytes(8)
setup();assert block(data=mc)==0;assert bytes(u.mem_read(counter,8))==bytes(8)
setup();put(8,0x20<<25,up2);assert block()==0;assert bytes(u.mem_read(vf+48,16))==bytes(16)
setup();put(0,lo,0x30);assert block()==0;assert bytes(u.mem_read(counter,8))==bytes(8)
setup();put(16368,lo,up1);put(16376,lo,up2);put(0,lo,0x400002ff);word(pcp,16368)
assert block()==(2 if enabled else 0)
if enabled:assert int.from_bytes(bytes(u.mem_read(pcp,4)),'big')==0
print('PASS actual Wii ELF native block retirement, E/branch/budget guards, micro-word replacement, no partial execution, wrap and alias guard')
if enabled:
 setup();put(0,lo,up1+(1<<6));fail_alloc=True
 assert block()==0;assert bytes(u.mem_read(counter,8))==bytes(8)
 fail_alloc=False;assert block()==2
 # Budget clamps a cached longer block; it may never retire past the cap.
 setup()
 for n in range(8):put(n*8,lo,up1)
 put(64,lo,0x400002ff)
 assert block()==8
 setup()
 for n in range(8):put(n*8,lo,up1)
 assert block(3)==3
 assert int.from_bytes(bytes(u.mem_read(counter,8)),'big')==3
 assert int.from_bytes(bytes(u.mem_read(pcp,4)),'big')==24
print('PASS actual Wii ELF block transient retry and cached-block budget clamp')
