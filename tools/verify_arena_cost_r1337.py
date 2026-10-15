"""Synthetic linked PPC allocator cost with irrelevant small free holes.
PPC instruction counts are not physical-Wii timings or FPS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<12))
setup();holes=[];owners=[]
for i in range(256):
 holes.append(call('ppc_code_cache_alloc',64));p=call('ppc_code_cache_alloc',512);assert p;u.mem_write(p,struct.pack('>I',i));owners.append(p)
for p in holes:call('ppc_code_cache_release',p)
total[0]=0;active[0]=True
large=[]
for i in range(32):
 p=call('ppc_code_cache_alloc',4096);assert p;large.append(p)
for p in large:call('ppc_code_cache_release',p)
active[0]=False;cost=total[0]
for i,p in enumerate(owners):assert int.from_bytes(u.mem_read(p,4),'big')==i;call('ppc_code_cache_release',p)
assert call('ppc_code_cache_used')==0
print('ARENA_PPC_INSTRUCTIONS',cost,'holes=256 requests=32 size=4096; owner bytes intact')
