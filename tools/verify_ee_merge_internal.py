"""Actual linked PPC merge blocks against scalar state and byte-lane oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mergenative=fused and "before-merge" not in Path(a.elf).name
merge_sig=hashlib.sha256();merge_cases=0
ops=[0x22,0x26,0x2a,0x2e,0x1a,0x1b,0x2c,0x2d]
for op in ops:
 width=4 if op in ops[:4] else 8
 store=op in [0x2a,0x2e,0x2c,0x2d];left=op in [0x22,0x2a,0x1a,0x2c]
 for k in range(width):
  for mapped in [False,True]:
   for rt in [0,2,3]:
    for old in [0x1122334490abcdef,0xfedcba9876543210]:
     for endpage in [False,True]:
      extension_setup();physical=0x301000-width if endpage else 0x300000
      ea=(physical if mapped else 0x80000000+physical)+k
      u.mem_write(state+32,struct.pack('>QQ',old,0x0123456789abcdef))
      u.mem_write(state+48,struct.pack('>QQ',ea+4,0x8899aabbccddeeff))
      source=(ea+4) if rt==3 else old if rt else 0
      initial=bytes(range(32));u.mem_write(ram+physical,initial)
      iw=(op<<26)|(3<<21)|(rt<<16)|0xfffc
      u.mem_write(ram+base,struct.pack('<I',iw))
      for n in range(1,8):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
      expected=bytearray(initial);reg=bytearray(source.to_bytes(8,'little'))
      for n in range(width):
       if (n<=k if left else n>=k):
        lane=width-1-k+n if left else n-k
        if store:expected[n]=reg[lane]
        elif rt:reg[lane]=initial[n]
      if not store and rt and width==4 and (left or k==0):
       reg[4:8]=(b'\xff'*4) if reg[3]&128 else bytes(4)
      before=executed();assert call('ee_core_step_n',8)==8 and executed()==before+8
      actual=bytes(u.mem_read(ram+physical,32));assert actual==bytes(expected),(hex(op),k,rt,'RAM byte oracle')
      if rt:
       got=int.from_bytes(bytes(u.mem_read(state+rt*16,8)),'big')
       assert got==int.from_bytes(reg,'little'),(hex(op),k,rt,hex(got),reg.hex())
       upper=int.from_bytes(bytes(u.mem_read(state+rt*16+8,8)),'big')
       assert upper==(0x8899aabbccddeeff if rt==3 else 0x0123456789abcdef),(hex(op),'upper 128-bit lanes')
      if mergenative:
       hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==8
      merge_sig.update(bytes(u.mem_read(state,688))+actual);merge_cases+=1
      if merge_cases%192==0:print(f"MERGE_PROGRESS {merge_cases}",flush=True)
if mergenative:
 # An aliasing store changes the following instruction: the source guard
 # returns after exactly one retirement instead of executing stale code.
 extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x80000000+base+4,0))
 replacement=(9<<26)|(2<<16)|77
 u.mem_write(state+5*16,struct.pack('>QQ',replacement,0))
 u.mem_write(ram+base,struct.pack('<I',(0x2e<<26)|(3<<21)|(5<<16)))
 before=executed();assert call('ee_jit_try_execute_block',state,8)==1
 assert executed()==before+1 and bytes(u.mem_read(ram+base+4,4))==struct.pack('<I',replacement)
 assert call('ee_core_step_n',1)==1 and reg2()==77
 for op in ops:
  for boundary in range(1,9):
   extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x80300003,0))
   u.mem_write(state+32,struct.pack('>QQ',0xfedcba9876543210,0))
   u.mem_write(ram+0x300000,bytes(range(32)))
   for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(op<<26)|(3<<21)|(2<<16)))
   word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,boundary);word(state+off['cop0']+12*4,0x18001)
   before=executed();assert call('ee_jit_try_execute_block',state,8)==boundary
   assert executed()==before+boundary
   assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+4*boundary
 print('PASS merge aliased-source invalidation and 64 exact IRQ exits')
print('EE_MERGE_SIGNATURE '+merge_sig.hexdigest())
print(f'PASS {merge_cases} merge programs, full state/RAM and independent byte-lane oracle')

bench={}
for op in ops:
 samples=[]
 for trial in range(3):
  extension_setup();u.mem_write(state+48,struct.pack('>QQ',0x80300003,0))
  u.mem_write(state+32,struct.pack('>QQ',0xfedcba9876543210,0));u.mem_write(ram+0x300000,bytes(range(32)))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(op<<26)|(3<<21)|(2<<16)))
  assert call('ee_core_step_n',8)==8
  word(state+off['pc'],base);word(state+off['next_pc'],base+4)
  total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;samples.append(total[0])
 bench[hex(op)]=samples
print('EE_MERGE_BENCH '+json.dumps({'elf':Path(a.elf).name,'PPC':bench,'scope':'Eight fully retired warm guest instructions, mocked platform; not Wii cycles or FPS.'}))
