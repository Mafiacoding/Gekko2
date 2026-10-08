"""Fragmentation regression: ~2 MiB live code, ~4 MiB free, no large
contiguous reservation. R1335 compilation fails; R1336 publishes exact size.
Linked PPC, mocked SDK heap/cache; no physical Wii timing assertion.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<12))
setup();owners=[]
while True:
 p=call('ppc_code_cache_alloc',1024)
 if not p:break
 u.mem_write(p,struct.pack('>I',len(owners)));owners.append(p)
assert len(owners)>5000
for n,p in enumerate(owners):
 if n%3!=2:call('ppc_code_cache_release',p)
live=call('ppc_code_cache_used');assert live<2300000
# Admission no longer asks fragmented executable storage for the 8196-byte
# translation scratch. A real emitted ADDIU body needs only a few words.
context=0x81760000;assert call('ppc_dynarec_init',context,16)==0,('worst-case reservation failed with MiB free',live)
assert call('ppc_dynarec_translate_one',context,(9<<26)|(2<<21)|(2<<16)|1)==0
fn=call('ppc_dynarec_finalize',context);assert fn
assert 0x80000000<=fn<0x81200000
u.mem_write(state+32,struct.pack('>QQ',41,0))
u.reg_write(UC_PPC_REG_1,0x81780000);u.reg_write(UC_PPC_REG_3,state);u.reg_write(UC_PPC_REG_LR,0x817ff000)
u.emu_start(fn,0x817ff000,count=1000);assert reg2()==42
call('ppc_code_cache_release',fn)
for n,p in enumerate(owners):
 if n%3==2:
  assert int.from_bytes(u.mem_read(p,4),'big')==n
  call('ppc_code_cache_release',p)
assert call('ppc_code_cache_used')==0
print('PASS fragmented arena admits/executes exact-size compiled body; all pinned owner bytes preserved; arena coalesces; live_bytes='+str(live))
