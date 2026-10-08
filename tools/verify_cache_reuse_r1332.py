"""Actual PPC collision replay: identical guest results and fewer translations.
PPC instruction counts include cache dispatch; they are not Wii timings.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
def profile(name):
 dest=0x81760000;call(name,dest);return dict(zip(['lookups','hits','misses','collisions','stale','attempts','installed','failures','compile_tb','compile_samples'],struct.unpack('>10Q',u.mem_read(dest,80))))
def counter(name):
 hi=call(name);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
# Four distinct EE blocks in the SAME old direct-mapped set, then replay.
pcs=[base];candidate=base+32
while len(pcs)<4:
 if ((candidate>>2)^(candidate>>12))&255==((base>>2)^(base>>12))&255:pcs.append(candidate)
 candidate+=4
out=[]
for flag in [0,8192]:
 word(mask_addr,initial&~8192|flag);setup()
 for pc in pcs:u.mem_write(ram+pc,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
 total[0]=0;active[0]=True
 for n in range(32):
  pc=pcs[n%4];word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  assert call('ee_core_step_n',8)==8
 active[0]=False;assert reg2()==256
 result=profile('ee_jit_get_cache_profile');result['PPC_instructions']=total[0];out.append(result)
assert out[1]['installed']==4 and out[0]['installed']==32,out
assert out[1]['hits']>=28 and out[1]['collisions']==0,out
assert out[1]['PPC_instructions']<out[0]['PPC_instructions'],out
print('EE_COLLISION_COST',json.dumps(out))
# IOP fixture helpers and independent step-vs-native oracle.
body=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text()
body=body[body.index('import random'):body.index('# Mixed instruction sequences')]
exec(compile(body,'IOP-layout-and-oracle','exec'))
pc_list=[ibase];candidate=ibase+4
h=lambda p:((p>>2)^(p>>5)^(p>>12))&127
while len(pc_list)<4:
 if h(candidate)==h(ibase):pc_list.append(candidate)
 candidate+=4
out=[]
for flag in [0,8192]:
 word(mask_addr,initial&~8192|flag);setup([(9<<26)|(2<<21)|(2<<16)|1]*16)
 for pc in pc_list:u.mem_write(iram+pc,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
 total[0]=0;active[0]=True
 for n in range(64):
  pc=pc_list[n%4];word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4)
  assert call('iop_core_step_n',1)==1
 active[0]=False;assert int.from_bytes(u.mem_read(ist+8,4),'big')==64
 result=profile('iop_jit_get_cache_profile');result['PPC_instructions']=total[0];out.append(result)
assert out[1]['installed']==4 and out[0]['installed']==64,out
assert out[1]['hits']>=60 and out[1]['PPC_instructions']<out[0]['PPC_instructions'],out
print('IOP_COLLISION_COST',json.dumps(out))
# source replacement is observed in every way, not hidden by cache reuse.
word(mask_addr,initial|8192);setup([(9<<26)|(2<<16)|1]*16)
assert call('iop_core_step_n',1)==1
u.mem_write(iram+ibase,struct.pack('<I',(9<<26)|(2<<16)|77));word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
assert call('iop_core_step_n',1)==1 and int.from_bytes(u.mem_read(ist+8,4),'big')==77
print('PASS four-way EE/IOP collision reuse, bounded IOP translation and live source mutation')
word(mask_addr,initial)
