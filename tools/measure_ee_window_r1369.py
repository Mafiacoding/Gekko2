"""Warm complete arithmetic windows, linked-PPC instruction count and state."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
word(mask,initial|1|8|16|(1<<20)|(1<<24)|(1<<27))
results={}
for n in [8,16,32]:
 setup();program=[(9<<26)|(2<<21)|(2<<16)|1,(13<<26)|(2<<21)|(3<<16)|0x1234,(3<<16)|(4<<11)|(2<<6),(4<<21)|(2<<16)|(5<<11)|33]*(n//4)
 u.mem_write(ram+base,struct.pack('<'+str(n)+'I',*program));assert call('ee_jit_try_execute_block',state,n)==n
 total[0]=0;active[0]=True
 for _ in range(100):
  word(state+off['pc'],base);word(state+off['next_pc'],base+4);assert call('ee_jit_try_execute_block',state,n)==n
 active[0]=False;results[n]={'ppc_instructions':total[0],'retired':executed(),'state_sha256':hashlib.sha256(bytes(u.mem_read(state,size))).hexdigest()}
print(json.dumps(results,sort_keys=True))
