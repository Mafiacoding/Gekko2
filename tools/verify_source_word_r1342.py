"""Actual PPC source-word loads: endian order, unaligned backing and pages."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
import random
rng=random.Random(0x1342);dest=ram+0x1000
for alignment in range(4):
 setup();word(state+off['ram'],ram+alignment)
 values=[rng.getrandbits(32) for _ in range(8)]
 u.mem_write(ram+alignment+base,struct.pack('<8I',*values))
 for index,value in enumerate(values):
  assert call('ee_core_block_peek',state,0x80000000+base+4*index,dest)==1
  assert bytes(u.mem_read(dest,4))==struct.pack('>I',value)
 assert call('ee_core_block_words',state,0x80000000+base,dest,8)==8
 assert bytes(u.mem_read(dest,32))==struct.pack('>8I',*values)
 # Admission may not read beyond the mapped page or the RAM backing.
 u.mem_write(ram+alignment+base+4092,struct.pack('<2I',0x12345678,0xdeadbeef))
 u.mem_write(dest,bytes([0xa5])*32)
 assert call('ee_core_block_words',state,0x80000000+base+4092,dest,8)==1
 assert bytes(u.mem_read(dest,8))==struct.pack('>I',0x12345678)+bytes([0xa5])*4
 word(state+off['ram_size'],base+4)
 assert call('ee_core_block_peek',state,0x80000000+base+4,dest)==0
print('PASS linked PPC source loads: endian order, all backing alignments, page bounds and short RAM backing')
