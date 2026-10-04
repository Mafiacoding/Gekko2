"""Internal chain/VF transfer tests; matching interpreter architectural state."""
from pathlib import Path
old=Path(__file__).with_name('verify_ee_delay_internal.py').read_text()
exec(compile(old,'verify_ee_delay_internal.py','exec'))
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={offsetof(ee_state_t,vu0_vf)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 vf=struct.unpack('>I',raw.read_bytes())[0]
vec_digest=hashlib.sha256();vec_cases=0
for op in [0x27,0x36,0x3e]:
 for mapped in [False,True]:
  for rt in [0,2]:
   for offset in [0,4,0xff0,0xff4,0xffc]:
    extension_setup();address=(0x300000 if mapped else 0x80300000)+offset
    u.mem_write(state+48,struct.pack('>QQ',address,0));u.mem_write(state+32,struct.pack('>QQ',0x90abcdef,0x1122334455667788))
    u.mem_write(state+vf,bytes.fromhex('12345678')*128) # deliberately dirty VF00 backing
    u.mem_write(state+vf+rt*16,struct.pack('>4I',0x80000000,0x7fc12345,0xdeadbeef,0x12345678))
    u.mem_write(ram+0x300000+offset,bytes(range(32)))
    iw=(op<<26)|(3<<21)|(rt<<16);u.mem_write(ram+base,struct.pack('<I',iw))
    before=executed();assert call('ee_core_step_n',8)==8 and executed()==before+8
    vec_digest.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+vf,512))+bytes(u.mem_read(ram+0x300000+offset,32)))
    if op==0x3e and rt==0:assert bytes(u.mem_read(ram+0x300000+offset,16))==struct.pack('<4I',0,0,0,0x3f800000)
    vec_cases+=1
chain_sig=hashlib.sha256()
for budget in [2,3,4,5,6,7,8]:
 extension_setup();u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)))
 for n in range(8):u.mem_write(ram+base+64+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
 before=executed();assert call('ee_core_step_n',budget)==budget and executed()==before+budget
 chain_sig.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8)))
 if fused and budget>=4:
  hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==budget
# Test continuation into an unsafe memory op: let the scalar path fault,
# with no speculative access/retirement in the predecessor's chain.
extension_setup();u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)));u.mem_write(state+48,struct.pack('>QQ',0x00401000,0))
u.mem_write(ram+base+64,struct.pack('<I',(0x23<<26)|(3<<21)|(6<<16)))
before=executed();assert call('ee_core_step_n',3)==3 and executed()==before+3
chain_sig.update(bytes(u.mem_read(state,688))+bytes(u.mem_read(state+exc_offset,8)))
print('EE_VECTOR_SIGNATURE '+vec_digest.hexdigest());print('EE_CHAIN_SIGNATURE '+chain_sig.hexdigest())
print(f'PASS {vec_cases} LWU/vector programs, VF00 constants, unmasked/cross-page addresses and seven chain budgets')
