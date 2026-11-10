#!/usr/bin/env python3
"""Actual PPC OFF/ON experiment combinations; interpreter is the EE oracle."""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))
checks=0
for chain in (0,1):
 for psm in (0,1):
  selected=0
  for change,irq in ((0,0),(3,0),(4,0),(5,0),(0,16)):
   reference,_=run(False,128,change,irq,chain_mode=0,psm_mode=psm)
   actual,n=run(True,128,change,irq,chain_mode=chain,psm_mode=psm)
   assert actual==reference,(chain,psm,change,irq)
   if not chain:assert stat(1)==0 and n==0
   selected+=n;checks+=1
  assert not chain or selected>0
  assert call('gekko2_experimental_enabled',0)==chain
  assert call('gekko2_experimental_enabled',1)==psm
  assert call('gs_mem_get_local_extended_conversion')==psm
  # Menu requests cannot change the active gate while paused/running.
  call('gekko2_experimental_request',0,1-chain)
  call('gekko2_experimental_request',1,1-psm)
  assert call('gekko2_experimental_enabled',0)==chain
  assert call('gekko2_experimental_enabled',1)==psm
  print(f'PASS linked modes chain={chain} psm={psm}: exact EE/timer/display/IRQ, notified write, real DMA-SMC, mapping/IRQ, selected={selected}',flush=True)
print(f'PASS {checks} four-mode linked comparisons; selected successors are counters, not Wii FPS')
