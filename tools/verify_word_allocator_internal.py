"""Linked PPC allocation proof and independent aliased integer oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
def counter(name):
 hi=call(name);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
checks=0
for fn in [0x21,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b]:
 for rd in [0,2,3,4]:
  for x,y in [(0,1),(0x8000000000000000,0xffffffffffffffff),(0x123456789abcdef0,0x0123456789abcdef)]:
   setup();u.mem_write(state+32,struct.pack('>QQ',x,0x1111222233334444));u.mem_write(state+48,struct.pack('>QQ',y,0x5555666677778888))
   for n in range(1,8):u.mem_write(ram+base+n*4,bytes(4))
   u.mem_write(ram+base,bytes(4))
   u.mem_write(ram+base+4,struct.pack('<I',(2<<21)|(3<<16)|(rd<<11)|fn))
   expected=0
   if fn in [0x21,0x23]:
    expected=((x+y) if fn==0x21 else (x-y))&0xffffffff
    if expected&0x80000000:expected|=0xffffffff00000000
   elif fn==0x24:expected=x&y
   elif fn==0x25:expected=x|y
   elif fn==0x26:expected=x^y
   elif fn==0x27:expected=(~(x|y))&0xffffffffffffffff
   elif fn==0x2a:expected=int((x if x<1<<63 else x-(1<<64))<(y if y<1<<63 else y-(1<<64)))
   else:expected=int(x<y)
   assert call('ee_core_step_n',8)==8
   if rd:
    assert int.from_bytes(u.mem_read(state+rd*16,8),'big')==expected,(fn,rd,hex(x),hex(y))
    assert int.from_bytes(u.mem_read(state+rd*16+8,8),'big')==({2:0x1111222233334444,3:0x5555666677778888}.get(rd,0))
   else:assert bytes(u.mem_read(state,16))==bytes(16)
   checks+=1
setup();x=0x123456789abcdef0;u.mem_write(state+32,struct.pack('>QQ',x,0))
for n in range(8):u.mem_write(ram+base+n*4,bytes(4))
u.mem_write(ram+base+4,struct.pack('<I',(2<<21)|(2<<16)|(4<<11)|0x25))
assert call('ee_core_step_n',8)==8
assert int.from_bytes(u.mem_read(state+64,8),'big')==x
checks+=1
if enabled:
 bodies=counter('ppc_dynarec_get_allocated_bodies');removed=counter('ppc_dynarec_get_eliminated_word_ops');spills=counter('ppc_dynarec_get_word_spills')
 reused=counter('ppc_dynarec_get_reused_word_loads')
 assert bodies>0 and reused>0,(bodies,removed,spills,reused)
 print('WORD_ALLOC_COMPILE_COUNTERS',bodies,removed,spills,reused)
print('PASS',checks,'independent allocated EE integer/alias/upper-lane oracles')
