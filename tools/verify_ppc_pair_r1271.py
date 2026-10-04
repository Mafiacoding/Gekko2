"""Actual Wii ELF fused VU pair dispatch and cache failure semantics."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_extensions_r1270.py').read_text(),'verify_ppc_extensions_r1270.py','exec'))
upper=(15<<21)|(2<<16)|(1<<11)|(3<<6)|0x2b
lower=(0x40<<25)|(2<<16)|(1<<11)|(3<<6)|0x30
for j,bits in [(1,0x3f800000),(2,0x40000000)]:
 for k in range(4):word(vf+16*j+4*k,bits)
word(vi+4,7);word(vi+8,9)
def pair(up,lo):return call('vu_jit_try_pair',vf,vi,acc,mem,16383,8,vs+voff['branch_delay'],vs+voff['branch_target'],up,lo)
# Eight ABI arguments are registers; word arguments 9/10 are caller-stack
# arguments. The generic ELF call helper handles registers only, so provide
# the EABI parameter area explicitly before each call.
def pair(up,lo):
 word(0x81780000+8,up);word(0x81780000+12,lo)
 return call('vu_jit_try_pair',vf,vi,acc,mem,16383,8,vs+voff['branch_delay'],vs+voff['branch_target'])
assert pair(upper,lower)==int(enabled)
before=allocs
for n in range(1000):assert pair(upper,lower)==int(enabled)
assert allocs==before
if enabled:
 assert struct.unpack('>4I',bytes(u.mem_read(vf+48,16)))==(0x40000000,)*4
 assert int.from_bytes(bytes(u.mem_read(vi+12,4)),'big')==16
# Decline is transactional: a supported upper must not run if lower declines.
for k in range(4):word(vf+48+4*k,0x12345678)
bad=0x10<<25
assert pair(upper,bad)==0
before=allocs
for n in range(1000):assert pair(upper,bad)==0
assert allocs==before
assert struct.unpack('>4I',bytes(u.mem_read(vf+48,16)))==(0x12345678,)*4
# I-immediate becomes visible to the upper operation within the same pair.
up=(15<<21)|(1<<11)|(4<<6)|0x1d|0x80000000
assert pair(up,0x40800000)==int(enabled)
if enabled:
 assert int.from_bytes(bytes(u.mem_read(vi+84,4)),'big')==0x40800000
 assert struct.unpack('>4I',bytes(u.mem_read(vf+64,16)))==(0x40800000,)*4
# A transient allocation failure must not create a negative entry.
if enabled:
 fresh=upper+(1<<6);fail_alloc=True
 assert pair(fresh,lower)==0
 fail_alloc=False
 assert pair(fresh,lower)==1
print('PASS actual Wii ELF fused-pair hot cache, transactional decline, I visibility and transient retry')
if enabled:
 # Two different I literals with the same direct-mapped pair index must
 # replace the owning entry and execute their own bits, never stale code.
 key=up&0x81ffffff
 def index(lo):return (key^(key>>11)^lo^(lo>>17))&127
 literal=0x40800000
 other=next(x for x in range(literal+1,literal+65536) if index(x)==index(literal))
 assert pair(up,other)==1
 assert int.from_bytes(bytes(u.mem_read(vi+84,4)),'big')==other
 assert pair(up,literal)==1
 assert int.from_bytes(bytes(u.mem_read(vi+84,4)),'big')==literal
print('PASS actual Wii ELF pair-cache collision/word-replacement group')
