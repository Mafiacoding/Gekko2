"""Proven RAM-load successors preserve evolving address proofs."""
from pathlib import Path
text=Path(__file__).with_name('verify_ee_event_chain_r1369.py').read_text().split('checks=0;costs=')[0]
exec(compile(text,'event_chain_fixture','exec'))
event_program_override=[(35<<26)|(1<<21)|(2<<16),(9<<26)|(1<<21)|(1<<16)|4,(13<<26)|(2<<21)|(3<<16)|0x3210,(3<<21)|(2<<16)|(4<<11)|33]*32
flags|=1<<9 # Fastmem is required for proven RAM-load window admission.
checks=selected=0
for budget in [7,8,16,17,64,65,128]:
 reference,_,_=run(0,budget,0,0)
 for mode in [1,2]:
  got,cost,n=run(mode,budget,0,0);assert got==reference,('RAM load chain',budget,mode);selected+=n;checks+=1
if 'ee_jit_get_event_chain_stat' in syms:assert selected>0
print('PASS',checks,'full state, evolving proven RAM loads, partial/full budgets and callee-saved ABI; selected',selected)
