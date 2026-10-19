#!/usr/bin/env python3
"""C.F.S in actual allocated native blocks and clean cross-block chains."""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))
base_setup=extension_setup
def extension_setup():
 base_setup();word(state+off['fcr31'],0xffabcdef)
cf=(17<<26)|(16<<21)|(2<<16)|(3<<11)|0x30
xor=(28<<26)|(20<<21)|(21<<16)|(22<<11)|(19<<6)|9
program=[cf if n%8==0 else xor for n in range(128)]
checks=successors=0
for budget in (1,7,8,9,17,33,128):
 reference,_=run(False,budget,0,0,(20,21,22),program_words=program)
 for mode in (0,1):
  actual,n=run(True,budget,0,0,(20,21,22),program_words=program,chain_mode=mode)
  assert actual==reference,(budget,mode)
  assert int.from_bytes(actual[off['fcr31']:off['fcr31']+4],'big')==0xff2bcdef
  checks+=1;successors+=n
assert successors>0,'C.F.S-containing native blocks did not chain'
print(f'PASS {checks} linked C.F.S/MMI full-state and EABI block comparisons; chain OFF/ON, precise budgets, successors {successors}')
