"""Real IOP control/divide integration after the VU ELF checks."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_vu_r1269.py').read_text(),'verify_ppc_vu_r1269.py','exec'))
# Clear only the fixture-owned IOP cache table to undo the inherited full-cache test.
if enabled:
 u.mem_write(syms['code_cache'],bytes(1024*8));u.mem_write(syms['pc_cache'],bytes(2048*16));word(syms['cache_size'],0)
call('iop_intc_init');call('iop_timers_init');u.mem_write(ios,bytes(ioff['ram']))
word(ios+ioff['ram'],io_ram);word(ios+ioff['ram_size'],65536);u.mem_write(ios+ioff['halted'],b'\0');u.mem_write(ios+ioff['idle'],b'\0');word(ios+ioff['cop0']+12*4,0)
word(ios+4,0xffffffff);word(ios+ioff['pc'],0x80004000);word(ios+ioff['next_pc'],0x80004004)
branch=(1<<26)|(1<<21)|2
program=[branch,(9<<26)|(2<<16)|7,(9<<26)|(3<<16)|99,(9<<26)|(3<<16)|9]
for n,iw in enumerate(program):u.mem_write(io_ram+0x4000+n*4,struct.pack('<I',iw))
call('iop_core_step')
assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==0x80004004
assert int.from_bytes(bytes(u.mem_read(ios+ioff['next_pc'],4)),'big')==0x8000400c
call('iop_core_step');assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==7
assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==0x8000400c
call('iop_core_step');assert int.from_bytes(bytes(u.mem_read(ios+12,4)),'big')==9
print('PASS 1 actual Wii ELF IOP branch/delay/target retirement group')
if enabled:
 before=allocs
 assert call('iop_jit_try_execute_one',ios,0x80004100,branch)==1
 assert int.from_bytes(bytes(u.mem_read(ios+ioff['next_pc'],4)),'big')==0x8000410c
 assert allocs==before
 print('PASS 1 actual Wii ELF IOP cached branch encoding with a different runtime PC group')
for fn,aa,bb,lo,hi in [(0x1a,1,0,0xffffffff,1),(0x1a,0xffffffff,0,1,0xffffffff),(0x1a,0x80000000,0xffffffff,0x80000000,0),(0x1b,0x80000000,0,0xffffffff,0x80000000)]:
 word(ios+4,aa);word(ios+8,bb);word(ios+ioff['pc'],0x80004000);word(ios+ioff['next_pc'],0x80004004)
 u.mem_write(io_ram+0x4000,struct.pack('<I',(1<<21)|(2<<16)|fn));call('iop_core_step')
 assert int.from_bytes(bytes(u.mem_read(ios+136,4)),'big')==hi
 assert int.from_bytes(bytes(u.mem_read(ios+140,4)),'big')==lo
 assert bytes(u.mem_read(ios+ioff['halted'],1))==b'\0'
print('PASS 1 actual Wii ELF IOP DIV/DIVU zero and signed-overflow CPU group')
# Prove all memory encodings enter native code in this exact ELF.
for op in [0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2b,0x22,0x26,0x2a,0x2e]:
 word(ios+4,0x3000);word(ios+8,0x12345678)
 assert call('iop_jit_try_execute_one',ios,0x80005000+op*4,(op<<26)|(1<<21)|(2<<16))==int(enabled)
print('PASS 1 actual Wii ELF IOP native gate for all 12 memory forms')
# Real MMIO and isolation behavior must survive native memory emission.
call('iop_intc_init');call('iop_intc_raise',0);word(ios+4,0x1f801070);word(ios+ioff['cop0']+12*4,0)
word(ios+ioff['pc'],0x80004000);word(ios+ioff['next_pc'],0x80004004)
u.mem_write(io_ram+0x4000,struct.pack('<I',(0x23<<26)|(1<<21)|(4<<16)));call('iop_core_step')
assert int.from_bytes(bytes(u.mem_read(ios+16,4)),'big')==1
word(ios+ioff['pc'],0x80004004);word(ios+ioff['next_pc'],0x80004008)
u.mem_write(io_ram+0x4004,struct.pack('<I',(0x2b<<26)|(1<<21)));call('iop_core_step')
assert call('iop_mem_read32',ios,0x1f801070)==0
# Cache-isolated stores must not overwrite guest main RAM.
word(ios+4,0x3000);word(ios+8,0xdeadbeef);word(ios+ioff['cop0']+12*4,0x10000)
u.mem_write(io_ram+0x3000,struct.pack('<I',0x12345678));word(ios+ioff['pc'],0x80004000);word(ios+ioff['next_pc'],0x80004004)
u.mem_write(io_ram+0x4000,struct.pack('<I',(0x2b<<26)|(1<<21)|(2<<16)));call('iop_core_step')
assert bytes(u.mem_read(io_ram+0x3000,4))==struct.pack('<I',0x12345678)
word(ios+ioff['cop0']+12*4,0)
print('PASS 1 actual Wii ELF native IOP MMIO read/ack and Status.IsC preservation group')
# All four unaligned merge forms, all four byte offsets, real LE guest RAM.
for op in [0x22,0x26,0x2a,0x2e]:
 for k in range(4):
  old=0xaabbccdd;data=0x12345678;word(ios+4,0x3000+k);word(ios+8,old)
  u.mem_write(io_ram+0x3000,struct.pack('<I',data));word(ios+ioff['pc'],0x80004000);word(ios+ioff['next_pc'],0x80004004)
  u.mem_write(io_ram+0x4000,struct.pack('<I',(op<<26)|(1<<21)|(2<<16)));call('iop_core_step')
  if op==0x22:expected=(old&[0xffffff,0xffff,0xff,0][k])|((data<<[24,16,8,0][k])&0xffffffff)
  elif op==0x26:expected=(old&[0,0xff000000,0xffff0000,0xffffff00][k])|(data>>[0,8,16,24][k])
  elif op==0x2a:expected=(data&[0xffffff00,0xffff0000,0xff000000,0][k])|(old>>[24,16,8,0][k])
  else:expected=(data&[0,0xff,0xffff,0xffffff][k])|((old<<[0,8,16,24][k])&0xffffffff)
  got=int.from_bytes(bytes(u.mem_read(ios+8,4)),'big') if op in [0x22,0x26] else int.from_bytes(bytes(u.mem_read(io_ram+0x3000,4)),'little')
  assert got==expected,(op,k,hex(got),hex(expected))
print('PASS 1 actual Wii ELF IOP all unaligned forms/byte offsets group')
# Loads to zero still acknowledge an MMIO read; a RAM read to rt0 preserves zero.
word(ios+4,0x3000);word(ios+ioff['pc'],0x80004000);word(ios+ioff['next_pc'],0x80004004)
u.mem_write(io_ram+0x4000,struct.pack('<I',(0x23<<26)|(1<<21)));call('iop_core_step')
assert int.from_bytes(bytes(u.mem_read(ios,4)),'big')==0
print('PASS 1 actual Wii ELF IOP memory rt0 group')
