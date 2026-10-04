"""Wide RAM + terminal-control retirement; old/new/interpreter digest parity."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ee_mapped_blocks_r1305.py').read_text(),'verify_ee_mapped_blocks_r1305.py','exec'))
extended='R1306' in Path(a.elf).name
# Derive FPR offset from target ABI, preserving NaN bits and fpr[0].
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={offsetof(ee_state_t,fpr),offsetof(ee_state_t,exc_this_pc)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 fpr,exc_offset=struct.unpack('>2I',raw.read_bytes())
def extension_setup():
 mapped_setup()
 # This suite compares COP0/FPR too. Initialize all those registers, not
 # only the subset reset by the inherited GPR-focused tests; otherwise
 # earlier JIT-only IRQ cases leak an unrelated old EPC into the digest.
 u.mem_write(state+off['cop0'],bytes(128))
 word(state+off['cop0']+9*4,100);word(state+off['cop0']+10*4,7)
 u.mem_write(state+fpr,bytes(128))
 u.mem_write(state+exc_offset,bytes(8))
wide=hashlib.sha256();wide_ops=[0x37,0x3f,0x1e,0x1f,0x31,0x39];programs=0
for op in wide_ops:
 for mapped in [False,True]:
  for rt in [0,2,3]:
   for pattern in [0x90abcdef,0x7fc12345]:
    extension_setup();address=0x300000 if mapped else 0x80300000
    # rt==rs validates that a load retains the original EA for all words.
    target=3 if rt==3 else 2
    u.mem_write(state+target*16,struct.pack('>QQ',pattern,0x1122334455667788))
    u.mem_write(state+48,struct.pack('>QQ',address+((15 if op in [0x1e,0x1f] else 4)),0x8899aabbccddeeff))
    u.mem_write(state+fpr+rt*4,struct.pack('>I',pattern))
    u.mem_write(ram+0x300000,bytes(range(64)))
    iw=(op<<26)|(3<<21)|(rt<<16)|(0 if op in [0x1e,0x1f] else 0xfffc)
    # One memory operation + seven non-memory operations. This permits
    # rt==rs without unintentionally changing later memory addresses.
    u.mem_write(ram+base,struct.pack('<I',iw))
    for n in range(1,8):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
    before=executed();assert call('ee_core_step_n',8)==8 and executed()==before+8
    if enabled and extended:
     hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==8,(op,mapped,rt)
    wide.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+fpr,128))+bytes(u.mem_read(ram+0x300000,64)))
    programs+=1
# Wide/float stores and loads at every retirement boundary; only one
# memory op avoids rt==rs mutation and confirms exact pre-IRQ state.
if enabled and extended:
 for op in wide_ops:
  for boundary in range(1,9):
   extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x300000,0));u.mem_write(state+32,struct.pack('>QQ',0x90abcdef,0x1122334455667788))
   u.mem_write(state+fpr+8,struct.pack('>I',0x7fc12345));u.mem_write(ram+0x300000,bytes(64))
   for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(op<<26)|(3<<21)|(2<<16)))
   word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,boundary);word(state+off['cop0']+12*4,0x18001)
   before=executed();assert call('ee_jit_try_execute_block',state,8)==boundary,(op,boundary)
   assert executed()==before+boundary
   assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+4*boundary
 print('PASS 48 wide/float memory IRQ boundaries')
# Terminal branch stays last in a block. Execute its delay slot or annul it
# with the existing scalar frontend; compare complete state at both stages.
control=hashlib.sha256();control_programs=0
cases=[('J',2<<26|((base+64)>>2)),('JAL',3<<26|((base+64)>>2)),
 ('JR',(3<<21)|8),('JALR',(3<<21)|(3<<11)|9)]
for op in [4,5,6,7,20,21,22,23]:cases.append(('B'+str(op),(op<<26)|(3<<21)|((5 if op in [4,5,20,21] else 0)<<16)|13))
for sub in range(4):cases.append(('REGIMM'+str(sub),(1<<26)|(3<<21)|(sub<<16)|13))
for name,iw in cases:
 for value in [0,1,0xffffffffffffffff,0x100000000]:
  for equal in [False,True]:
   extension_setup();u.mem_write(state+48,struct.pack('>QQ',base+64 if name in ['JR','JALR'] else value,0))
   u.mem_write(state+80,struct.pack('>QQ',value if equal else (value^0x100000000),0))
   # The third instruction terminates this block, even if budget is eight.
   for n in range(2):u.mem_write(ram+base+n*4,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
   u.mem_write(ram+base+8,struct.pack('<I',iw));u.mem_write(ram+base+12,struct.pack('<I',(9<<26)|(6<<16)|77))
   before=executed();assert call('ee_core_step_n',3)==3 and executed()==before+3,(name,value,equal)
   if enabled and extended:
    hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==3,(name,value,equal)
   snap1=bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8))
   control.update(snap1)
   assert call('ee_core_step_n',1)==1
   snap2=bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8))
   control.update(snap2)
   print('EE_CONTROL_CASE '+json.dumps({'name':name,'value':value,'equal':equal,'sha':hashlib.sha256(snap1+snap2).hexdigest(),'context1':snap1[544:].hex(),'context2':snap2[544:].hex()}))
   control_programs+=1
# Timer Compare coinciding with a taken branch must defer the IRQ until
# after its delay slot. Fault in that slot must carry EPC=branch and BD.
if enabled and extended:
 for taken in [False,True]:
  for fault in [False,True]:
   extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x00401000,0))
   u.mem_write(ram+base+8,struct.pack('<I',((2<<26)|((base+64)>>2)) if taken else ((4<<26)|(2<<16)|13)))
   if fault:u.mem_write(ram+base+12,struct.pack('<I',(0x23<<26)|(3<<21)|(6<<16)))
   else:
    word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,3);word(state+off['cop0']+12*4,0x18001)
   before=executed();assert call('ee_core_step_n',3)==3 and executed()==before+3
   assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+12
   assert call('ee_core_step_n',1)==1
   cause=int.from_bytes(bytes(u.mem_read(state+off['cop0']+13*4,4)),'big')
   epc=int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')
   if fault:assert cause&0x80000000 and epc==base+8,(hex(cause),hex(epc))
   else:assert not cause&0x80000000 and epc==(base+64 if taken else base+16),(hex(cause),hex(epc))
 # Verify the previous scalar REGIMM failure independently of block admission.
 extension_setup();u.mem_write(state+48,struct.pack('>QQ',1,0))
 u.mem_write(ram+base,struct.pack('<I',(1<<26)|(3<<21)|13))
 assert call('ee_core_step_n',1)==1
 assert bytes(u.mem_read(state+off['branch_pending'],1))==b'\x01'
 print('PASS taken/not-taken terminal IRQ deferral, delay-slot TLB fault EPC/BD and scalar REGIMM delay flag')
# Warm identical wide-memory workload on old/new engines.
bench={}
for op in wide_ops:
 samples=[]
 for iteration in range(4):
  extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x300000,0));u.mem_write(ram+0x300000,bytes(range(64)))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(op<<26)|(3<<21)|(2<<16)))
  assert call('ee_core_step_n',8)==8
  word(state+off['pc'],base);word(state+off['next_pc'],base+4)
  total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;samples.append(total[0])
 bench[hex(op)]=samples
print('EE_WIDE_SIGNATURE '+wide.hexdigest());print('EE_CONTROL_SIGNATURE '+control.hexdigest())
print('EE_EXTENSION_BENCH '+json.dumps({'elf':Path(a.elf).name,'memory':bench,'scope':'eight real guest retirements, mocked platform services; not Wii cycles or FPS'}))
print(f'PASS {programs} wide-memory and {control_programs} terminal-control programs')
