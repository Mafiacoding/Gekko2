"""Independent execution/counter checks for cold-boot CPU options on linked PPC."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_address=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_address,4),'big')
def counter(name):
 hi=call(name);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
states=[]
for flag in [0,1]:
 word(mask_address,(initial&~1)|flag);setup()
 for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|7))
 assert call('ee_core_step_n',8)==8 and reg2()==56
 assert counter('ee_jit_get_block_retired')==(8 if flag and initial&8 else 0)
 assert counter('ee_jit_get_native_retired_count')==(8 if flag and initial&8 else 0)
 states.append(bytes(u.mem_read(state,688)))
assert states[0]==states[1]
# Scalar entry itself must respect the EE switch as well.
for flag in [0,1]:
 word(mask_address,(initial&~1)|flag);setup()
 assert call('ee_jit_try_execute_one_at',state,base,(9<<26)|(2<<16)|77)==flag
 assert reg2()==(77 if flag else 0)
for bit,name in [(2,'iop_jit_try_execute_one'),(4,'vu_jit_try_upper')]:
 word(mask_address,initial&~bit)
 # Disabled entry must return before dereferencing intentionally NULL state.
 assert call(name,0,0,0,0)==0
# Residency and intra-body allocation are independent compile options.
for bit,name in [(32,'ppc_dynarec_get_resident_blocks'),(2048,'ppc_dynarec_get_allocated_bodies')]:
 for flag in [0,bit]:
  word(mask_address,(initial|8)&~bit|flag);setup()
  u.mem_write(state+16,struct.pack('>QQ',0x123456789abcdef0,0))
  u.mem_write(state+32,struct.pack('>QQ',0x0f0f0f0f0f0f0f0f,0))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(1<<21)|(2<<16)|(3<<11)|0x25))
  before=counter(name);assert call('ee_core_step_n',8)==8
  assert int.from_bytes(u.mem_read(state+48,8),'big')==0x1f3f5f7f9fbfdfff
  assert (counter(name)>before)==bool(flag)
# Native successor option preserves the result and actually changes dispatch.
for flag in [0,16]:
 word(mask_address,(initial|8)&~16|flag);setup()
 u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)))
 for n in range(8):u.mem_write(ram+base+64+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
 assert call('ee_core_step_n',8)==8
 word(state+off['pc'],base);word(state+off['next_pc'],base+4);u.mem_write(state+64,bytes(16))
 before=counter('ee_jit_get_native_successors');assert call('ee_core_step_n',8)==8
 assert int.from_bytes(u.mem_read(state+64,8),'big')==6
 assert counter('ee_jit_get_native_successors')-before==(1 if flag else 0)
word(mask_address,initial);setup()
print('PASS EE native/interpreter state parity, disabled EE/IOP/VU dispatch, residency/allocation options and guarded links')
