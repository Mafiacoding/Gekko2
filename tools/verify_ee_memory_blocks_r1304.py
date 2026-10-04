"""Actual linked-PPC mixed memory blocks and exact scalar-decline behavior.
Run on old/new and Interpreter/JIT for complete state+RAM signature parity.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ee_exit_counts_r1303.py').read_text(),'verify_ee_exit_counts_r1303.py','exec'))
import hashlib,random
rng=random.Random(1304);digest=hashlib.sha256();guard='ee_core_block_memory_safe' in syms
ops=[0x20,0x24,0x21,0x25,0x23,0x28,0x29,0x2b]
data=0x300000
for op in ops:
 for alias in [0x80000000,0xa0000000]:
  for bits in [0,0x80000000,0x90abcdef,0xffffffff]:
   setup();before_count=executed();u.mem_write(state+48,struct.pack('>QQ',alias+data+4,0x1122334455667788))
   u.mem_write(state+32,struct.pack('>QQ',bits,0xaabbccddeeff0011))
   u.mem_write(ram+data,rng.randbytes(32))
   # Memory operations mixed with ALU; effective address includes negative offset.
   for n in range(8):
    iw=((op<<26)|(3<<21)|(2<<16)|0xfffc) if n%2==0 else ((9<<26)|(4<<21)|(4<<16)|1)
    u.mem_write(ram+base+4*n,struct.pack('<I',iw))
   assert call('ee_core_step_n',8)==8 and executed()==before_count+8
   assert int.from_bytes(bytes(u.mem_read(state+64,8)),'big')==4
   assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+32
   if enabled and guard:
    hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==8
   digest.update(bytes(u.mem_read(state,544))+bytes(u.mem_read(ram+data,32)))
# Same first-word fallback under mapped TLB, MMIO, ROM, scratch and invalid/alignment cases.
for op in ops:
 for address in [0x300400,0x10000000,0xbfc00000,0x70000000,0x80400000,0x00401000]+([0x803fffff,0x80300001] if op not in [0x20,0x24,0x28] else []):
  setup();before_count=executed();u.mem_write(state+48,struct.pack('>QQ',address,0));u.mem_write(state+32,struct.pack('>QQ',0x90abcdef,0))
  u.mem_write(ram+base,struct.pack('<I',(op<<26)|(3<<21)|(2<<16)))
  if enabled and guard:
   before=bytes(u.mem_read(state,1700));assert call('ee_jit_try_execute_block',state,8)==0
   assert bytes(u.mem_read(state,1700))==before
  assert call('ee_core_step_n',1)==1
  digest.update(bytes(u.mem_read(state,544))+bytes(u.mem_read(state+off['cop0'],128))+bytes(u.mem_read(state+off['pc'],8)))
# Later unsafe data address must stop at that instruction with no access/retirement.
if enabled and guard:
 for op in ops:
  setup();before_count=executed();u.mem_write(state+48,struct.pack('>QQ',0x10000000,0))
  u.mem_write(ram+base+4,struct.pack('<I',(op<<26)|(3<<21)|(2<<16)))
  assert call('ee_jit_try_execute_block',state,8)==1 and executed()==before_count+1 and reg2()==1
  assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+4
 # SW modifies the very next source word: live prepare must reject stale code.
 setup();before_count=executed();u.mem_write(state+48,struct.pack('>QQ',0x80200000,0))
 replacement=(9<<26)|(2<<16)|77
 u.mem_write(state+5*16,struct.pack('>QQ',replacement,0))
 u.mem_write(ram+base,struct.pack('<I',(0x2b<<26)|(3<<21)|(5<<16)|4))
 assert call('ee_jit_try_execute_block',state,8)==1 and executed()==before_count+1
 assert bytes(u.mem_read(ram+base+4,4))==struct.pack('<I',replacement)
 assert call('ee_core_step_n',1)==1 and reg2()==77
 # Interrupt at every boundary in a memory/ALU block must prevent later stores.
 for boundary in range(1,9):
  setup();before_count=executed();u.mem_write(state+48,struct.pack('>QQ',0x80300000,0));u.mem_write(state+32,struct.pack('>QQ',0x90abcdef,0))
  u.mem_write(ram+data,bytes(32))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x2b<<26)|(3<<21)|(2<<16)|(n*4)))
  word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,boundary);word(state+off['cop0']+12*4,0x18001)
  assert call('ee_jit_try_execute_block',state,8)==boundary and executed()==before_count+boundary
  assert bytes(u.mem_read(ram+data,32))==struct.pack('<I',0x90abcdef)*boundary+bytes((8-boundary)*4)
  assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+4*boundary
 print('PASS eight native memory families, mixed-block retirement, unsafe live exits, SW self-modification and eight store IRQ boundaries')
# Same exact direct-memory workload on prior scalar frontend and new block frontend.
bench=[]
for iteration in range(5):
 setup();before_count=executed();u.mem_write(state+48,struct.pack('>QQ',0x80300000,0));u.mem_write(ram+data,struct.pack('<I',0x90abcdef))
 for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x23<<26)|(3<<21)|(2<<16)))
 assert call('ee_core_step_n',8)==8
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;bench.append(total[0])
print('EE_MEMORY_SIGNATURE '+digest.hexdigest())
print('EE_MEMORY_BLOCK_BENCH '+json.dumps({'elf':Path(a.elf).name,'LW_8_warm_PPC':bench,'scope':'real guest retirement; mocked services; no Wii FPS claim'}))
