"""Internal fused delay slots; no user release artifacts or FPS claims."""
from pathlib import Path
old=Path(__file__).with_name('verify_ee_extensions_r1306.py').read_text()
old=old.replace("extended='R1306' in Path(a.elf).name","extended='R1306' in Path(a.elf).name or 'ee_core_block_prepare_delay' in syms")
exec(compile(old,'verify_ee_extensions_r1306.py','exec'))
fused='ee_core_block_prepare_delay' in syms and enabled
delay_sig=hashlib.sha256();programs=0
# Same 16 controls and 4x2 outcomes as the prior suite, now budget includes DS.
for name,iw in cases:
 for value in [0,1,0xffffffffffffffff,0x100000000]:
  for equal in [False,True]:
   for mem in [False,True]:
    extension_setup();u.mem_write(state+48,struct.pack('>QQ',base+64 if name in ['JR','JALR'] else value,0))
    u.mem_write(state+80,struct.pack('>QQ',value if equal else value^0x100000000,0))
    u.mem_write(state+7*16,struct.pack('>QQ',0x80300000,0));u.mem_write(ram+0x300000,struct.pack('<I',0x90abcdef))
    for n in range(2):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
    u.mem_write(ram+base+8,struct.pack('<I',iw))
    slot=((0x23<<26)|(7<<21)|(6<<16)) if mem else ((9<<26)|(6<<16)|77)
    u.mem_write(ram+base+12,struct.pack('<I',slot));before=executed()
    assert call('ee_core_step_n',4)==4 and executed()==before+4
    delay_sig.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8)))
    if fused:
     hi=call('ee_jit_get_block_retired');n=((hi<<32)|u.reg_read(UC_PPC_REG_4));assert n in [3,4],(name,value,equal,n)
    programs+=1
if fused:
 # A branch at the first instruction can form a two-op native block.
 for op in [2,3]:
  extension_setup();u.mem_write(ram+base,struct.pack('<I',(op<<26)|((base+64)>>2)))
  assert call('ee_core_step_n',2)==2
  hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==2
  assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+64
 # Changed delay source exits with branch pending; scalar resumes new word.
 extension_setup();u.mem_write(ram+base+8,struct.pack('<I',(2<<26)|((base+64)>>2)))
 def mutate_slot(uc,address,size,user):
  if address==syms['ee_core_block_prepare_delay']:uc.mem_write(ram+base+12,struct.pack('<I',(9<<26)|(6<<16)|99))
 hook=u.hook_add(UC_HOOK_CODE,mutate_slot);before=executed();assert call('ee_jit_try_execute_block',state,4)==3
 u.hook_del(hook);assert executed()==before+3 and bytes(u.mem_read(state+off['branch_pending'],1))==b'\x01'
 assert call('ee_core_step_n',1)==1 and int.from_bytes(bytes(u.mem_read(state+6*16,8)),'big')==99
 # Unsafe slot faults only through scalar continuation, preserving BD/EPC.
 for taken in [False,True]:
  extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x00401000,0))
  branch=((2<<26)|((base+64)>>2)) if taken else ((4<<26)|(2<<16)|13)
  u.mem_write(ram+base+8,struct.pack('<I',branch));u.mem_write(ram+base+12,struct.pack('<I',(0x23<<26)|(3<<21)|(6<<16)))
  assert call('ee_jit_try_execute_block',state,4)==3
  assert call('ee_core_step_n',1)==1
  cause=int.from_bytes(bytes(u.mem_read(state+off['cop0']+13*4,4)),'big');epc=int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')
  assert cause&0x80000000 and epc==base+8
 # An IRQ due on the branch waits until after the compiled delay slot.
 for taken in [False,True]:
  extension_setup();branch=((2<<26)|((base+64)>>2)) if taken else ((4<<26)|(2<<16)|13)
  u.mem_write(ram+base+8,struct.pack('<I',branch));word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,3);word(state+off['cop0']+12*4,0x18001)
  before=executed();assert call('ee_jit_try_execute_block',state,4)==4 and executed()==before+4
  cause=int.from_bytes(bytes(u.mem_read(state+off['cop0']+13*4,4)),'big');epc=int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')
  assert not cause&0x80000000 and epc==(base+64 if taken else base+16)
 print('PASS native compiled DS, first-op jumps, mutated delay source, scalar fault BD and delayed IRQ')
print('EE_DELAY_SIGNATURE '+delay_sig.hexdigest());print(f'PASS {programs} control+ALU/memory delay programs with exact retirement')
