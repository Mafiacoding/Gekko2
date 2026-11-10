#!/usr/bin/env python3
"""Focused linked checks with nonzero untouched HI/LO/JAL upper lanes."""
from pathlib import Path
for name in ('memory','branch'):
 source=Path(__file__).with_name(f'verify_chain_{name}_linked.py').read_text()
 exec(compile(source.split('checks=successors=0')[0],f'verify_chain_{name}_linked.py','exec'))
 if name=='memory':
  for scenario in range(6):
   reference,_=memory_run(False,128,scenario);actual,_=memory_run(True,128,scenario)
   assert actual==reference,scenario
  for alias in (1,2):
   for budget in (7,33,128):
    reference,_=memory_run(False,budget,0,alias);actual,n=memory_run(True,budget,0,alias)
    assert actual==reference,(alias,budget)
    if budget>=33:assert n>0,('alias companion unused',alias,budget)
 else:
  for kind in range(4):
   for scenario in (0,1,3):
    reference,_=branch_run(False,33,kind,scenario);actual,_=branch_run(True,33,kind,scenario)
    assert actual==reference,(kind,scenario)
  for kind in range(4):
   for scenario in (0,1):
    reference,_=branch_run(False,33,kind,scenario,0);actual,n=branch_run(True,33,kind,scenario,0)
    assert actual==reference,('first-branch',kind,scenario)
    if scenario==0:assert n>0,('first-branch companion unused',kind)
print('PASS 32 additional linked nonzero 128-bit HI/LO/SA, RAM address/destination alias and JAL upper-lane comparisons')
