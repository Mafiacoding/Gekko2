"""Actual linked PPC COP1 blocks and completed REGIMM/BC1 controls."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ee_chain_vector_internal.py').read_text(),'verify_ee_chain_vector_internal.py','exec'))
fpu_sig=hashlib.sha256();fpu_cases=0
funcs=list(range(8))+[0x16,0x18,0x19,0x1a,0x1c,0x1d,0x1e,0x1f,0x24,0x28,0x29,0x32,0x34,0x36]
patterns=[0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0x7fc12345]
for fmt,fn in [(0x10,fn) for fn in funcs]+[(0x14,0x20)]:
 for index,bits in enumerate(patterns):
  for ftbits in [bits,patterns[(-index)%len(patterns)]]:
   extension_setup();u.mem_write(state+fpr+4,struct.pack('>I',bits));u.mem_write(state+fpr+8,struct.pack('>I',ftbits))
   word(state+fpr+128,0x00afffbf);word(state+fpr+132,bits)
   iw=(0x11<<26)|(fmt<<21)|(2<<16)|(1<<11)|(3<<6)|fn
   u.mem_write(ram+base,struct.pack('<I',iw))
   before=executed();assert call('ee_core_step_n',8)==8 and executed()==before+8,(fmt,fn,index)
   if fused:
    hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==8,(fmt,fn,index)
   snapshot=bytes(u.mem_read(state,688))+bytes(u.mem_read(state+fpr,136))
   fpu_sig.update(snapshot);fpu_cases+=1
   print('FPU_CASE '+json.dumps({'fmt':fmt,'fn':fn,'bits':bits,'ft':ftbits,'state':snapshot.hex()}))
if fused:
 from unicorn.ppc_const import UC_PPC_REG_14,UC_PPC_REG_1,UC_PPC_REG_LR
 # The last compiled program is CVT.S.W. Enter its native function directly
 # so an enclosing C function cannot hide a damaged nonvolatile register.
 native=precise_native_for_pc(base)
 assert native
 syms['internal_native_abi']=native
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 saved=[0xa1000000+0x101*n for n in range(18)]
 for n,v in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,v)
 def linkage_write(uc,address,size,user):
  if address==syms['ee_jit_cvt_s_w_helper']:
   sp=uc.reg_read(UC_PPC_REG_1)
   uc.mem_write(sp+4,struct.pack('>I',uc.reg_read(UC_PPC_REG_LR)))
 hook=u.hook_add(UC_HOOK_CODE,linkage_write)
 assert call('internal_native_abi',state,0,0)==8
 u.hook_del(hook)
 assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved
 print('PASS raw native CVT.S.W EABI with callee linkage-area write')
links_sig=hashlib.sha256();link_cases=0
for sub in range(0x10,0x14):
 for rs in [0,3,31]:
  for val in [0,1,0xffffffffffffffff,0x100000000]:
   extension_setup();u.mem_write(state+rs*16,struct.pack('>QQ',val,0x1122334455667788))
   iw=(1<<26)|(rs<<21)|(sub<<16)|13;u.mem_write(ram+base+8,struct.pack('<I',iw))
   assert call('ee_core_step_n',4)==4
   assert int.from_bytes(bytes(u.mem_read(state+31*16,8)),'big')==base+16
   links_sig.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8)));link_cases+=1
for sub in range(4):
 for condition in [False,True]:
  extension_setup();word(state+fpr+128,0x800000 if condition else 0)
  iw=(0x11<<26)|(8<<21)|(sub<<16)|13;u.mem_write(ram+base+8,struct.pack('<I',iw))
  assert call('ee_core_step_n',4)==4
  links_sig.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8))+bytes(u.mem_read(state+fpr+128,4)));link_cases+=1
print('EE_FPU_ARITHMETIC_SIGNATURE '+fpu_sig.hexdigest());print('EE_EXTENDED_LINK_SIGNATURE '+links_sig.hexdigest())
print(f'PASS {fpu_cases} COP1 corner programs and {link_cases} link/BC1 control outcomes')
