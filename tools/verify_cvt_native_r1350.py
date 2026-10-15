"""Actual emitted PPC CVT.S.W: signed extremes, rounding ties and aliases."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
import random
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|1)
setup();rng=random.Random(1350)
values=[0,1,-1,0x7fffffff,-0x80000000,16777215,16777216,16777217,16777219,-16777217,-16777219]
values += [rng.randrange(-0x80000000,0x80000000) for _ in range(512)]
checks=0
for fs,fd in [(2,3),(2,2),(0,31),(31,0)]:
 iw=(0x11<<26)|(0x14<<21)|(fs<<11)|(fd<<6)|0x20
 for v in values:
  word(state+fpr+fs*4,v&0xffffffff)
  assert call('ee_jit_try_execute_one_at',state,base,iw)==1
  expected=struct.pack('>f',float(v));got=bytes(u.mem_read(state+fpr+fd*4,4));assert got==expected,(fs,fd,v,got.hex(),expected.hex())
  checks+=1
print('PASS',checks,'native CVT.S.W signed int32 conversion, precision boundaries and source/destination aliases')
