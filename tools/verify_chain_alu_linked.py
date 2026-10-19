#!/usr/bin/env python3
"""Exercise SPEED quiet batching in native resident scalar ALU companions."""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))
program=[]
for n in range(128):
 if n&1:program.append((13<<26)|(20<<21)|(22<<16)|0x4321)
 else:program.append((9<<26)|(20<<21)|(20<<16)|7)
checks=selected=0
for budget in (7,8,9,16,33,128):
 for change,irq in ((0,0),(4,0),(5,0),(0,16)):
  reference,_=run(False,budget,change,irq,(20,21,22),program_words=program)
  actual,n=run(True,budget,change,irq,(20,21,22),program_words=program)
  assert actual==reference,(budget,change,irq)
  checks+=1;selected+=n
assert selected>0,'scalar ALU native successors never executed'
print(f'PASS {checks} linked native scalar ALU quiet-batch, fused boundaries, scheduler, mapping and real DMA-SMC comparisons; successors {selected}')
