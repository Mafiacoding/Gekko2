"""Regression for Wii scalar SQ/LQ losing IPU FIFO bytes. Linked PPC CPU,
real EE/IPU code; SDK allocation/cache hooks from the fixture are mocked.
Run against R1335: SETIQ never completes. No guest ROM/image required.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
checks=0
for jit in [0,1]:
 for fastmem in [0,1]:
  word(mask_addr,(initial&~(1|512))|jit|(512 if fastmem else 0));setup();call('ipu_init')
  word(state+off['cop0']+12*4,0x70400000)
  u.mem_write(state+16,struct.pack('>QQ',0xb0007010,0))
  iq=bytes(range(64));call('ipu_mmio_write32',0x10002000,0x50000000)
  for n in range(4):
   lo,hi=struct.unpack('<QQ',iq[n*16:n*16+16]);u.mem_write(state+32,struct.pack('>QQ',lo,hi))
   u.mem_write(ram+base+n*4,struct.pack('<I',(0x1f<<26)|(1<<21)|(2<<16)))
   assert call('ee_core_step_n',1)==1
  dest=0x81750000;call('ipu_mmio_read32',0x10002010,dest)
  ctrl=int.from_bytes(u.mem_read(dest,4),'big')
  assert not ctrl&0x80000000,('SETIQ starved after four real SQ',jit,fastmem,hex(ctrl))
  if jit:
   hi=call('ee_jit_get_executed_count');assert hi or u.reg_read(UC_PPC_REG_4)>=4
  checks+=1
  # Output read: two LQs consume exactly two QWCs. rt==rs proves EA captured
  # before the load overwrites the base; alias mirrors use actual FIFO bytes.
  call('ipu_init');call('dma_init');call('dma_bind_ee_ram',ram,0x400000);source=bytes(range(256))+bytes([128])*128
  u.mem_write(ram+0x1000,source);call('ipu_mmio_write32',0x10002000,0x70000001)
  for offset,value in [(16,0x1000),(32,24),(0,0x101)]:call('dma_mmio_write32',0x1000b400+offset,value)
  expected=bytes(c for y in range(8) for c in ([max(0,min(255,(((max(0,y-16)*149)>>6)+1)>>1))]*3+[128]))
  for n in range(2):
   u.mem_write(state+48,struct.pack('>QQ',0xb0007000,0));pc=base+0x40+n*4
   u.mem_write(ram+pc,struct.pack('<I',(0x1e<<26)|(3<<21)|(3<<16)|7))
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   assert call('ee_core_step_n',1)==1
   lo,hi=struct.unpack('<QQ',expected[n*16:n*16+16]);assert bytes(u.mem_read(state+48,16))==struct.pack('>QQ',lo,hi)
   checks+=1
word(mask_addr,initial)
print(f'PASS {checks} scalar JIT/interpreter IPU SQ/LQ cases, Fastmem on/off, SETIQ completion and aliased LQ')
