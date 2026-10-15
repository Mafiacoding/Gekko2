"""Linked PPC admission: exact guest state, transient declines and stale code."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
pc=base|0x80000000
lw=(35<<26)|(1<<21)|(2<<16)
add=(9<<26)|(3<<21)|(3<<16)|1
program=struct.pack('<8I',lw,*([add]*7))
def installed():
 call('ee_jit_get_cache_profile',0x817f0000)
 return int.from_bytes(u.mem_read(0x817f0000+6*8,8),'big')
def reset_pc():
 word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
def address(value):u.mem_write(state+16,struct.pack('>QQ',value,0))
checks=0;costs=[]
for service in [0,1]:
 word(mask_addr,(initial|1|8|8192)&~(1<<20)|(service<<20))
 setup();u.mem_write(ram+base,program);reset_pc()
 for value in [0x10000000,0x80300001,0x00600000]:
  address(value);before=bytes(u.mem_read(state,size));assert call('ee_jit_try_execute_block',state,8)==0
  assert bytes(u.mem_read(state,size))==before
  if service:assert installed()==0
  checks+=1
 address(0x80300000);u.mem_write(ram+0x300000,struct.pack('<I',123))
 assert call('ee_jit_try_execute_block',state,8)==8
 assert int.from_bytes(u.mem_read(state+32,8),'big')==123
 owner=precise_native_for_pc(pc,8);assert owner
 # A transient decline must keep a warmed owner when admission is enabled.
 reset_pc();address(0x10000000);before=bytes(u.mem_read(state,size))
 assert call('ee_jit_try_execute_block',state,8)==0
 assert bytes(u.mem_read(state,size))==before
 if service:assert precise_native_for_pc(pc,8)==owner
 reset_pc();address(0x80300000);assert call('ee_jit_try_execute_block',state,8)==8
 snapshot=bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+0x300000,64))
 if service:assert snapshot==memory_control
 else:memory_control=snapshot
 # Raw code change bypasses notification, so entry byte validation must still reject.
 reset_pc();u.mem_write(ram+base,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
 n=call('ee_jit_try_execute_block',state,8)
 if service:assert n==8
 else:
  assert n==0
  reset_pc();assert call('ee_jit_try_execute_block',state,8)==8
 assert int.from_bytes(u.mem_read(state+32,8),'big')==132
 # Nonsequential pipeline admission must not mutate state or translate.
 reset_pc();word(state+off['next_pc'],pc+16);before=bytes(u.mem_read(state,size));count=installed()
 assert call('ee_jit_try_execute_block',state,8)==0
 assert bytes(u.mem_read(state,size))==before and installed()==count
 checks+=4
 # Warm arithmetic entry cost, complete state is compared between modes.
 setup();u.mem_write(ram+base,struct.pack('<8I',*([add]*8)))
 reset_pc();assert call('ee_jit_try_execute_block',state,8)==8
 total[0]=0;active[0]=True
 for n in range(100):
  reset_pc();assert call('ee_jit_try_execute_block',state,8)==8
 active[0]=False;costs.append(total[0]);snapshot=bytes(u.mem_read(state,size))
 if service:assert snapshot==control
 else:control=snapshot
print('PASS',checks,'entry cases: pure declines, warmed owners, raw mutation, pipeline and complete state')
print('ENTRY_WARM_COST',json.dumps({'legacy':costs[0],'admitted':costs[1],'blocks':100}))
