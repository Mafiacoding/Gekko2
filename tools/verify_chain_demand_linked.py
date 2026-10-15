#!/usr/bin/env python3
"""Additional linked native map-selection and actual DMA-SMC comparisons."""
from pathlib import Path
# Reuse the same snapshot/retirement oracle without running its full matrix.
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))
checks=selected=0
for registers in ((4,5,6),(20,21,22)):
 for budget in (33,128):
  for change in (0,4,5):
   reference,_=run(False,budget,change,0,registers)
   actual,n=run(True,budget,change,0,registers)
   assert actual==reference,(registers,budget,change)
   address=syms['precise_resident_map']
   count=int.from_bytes(u.mem_read(address+24,4),'big')
   offsets=struct.unpack('>'+str(count)+'H',u.mem_read(address,count*2))
   assert count==12 and {off//16 for off in offsets}==set(registers),(registers,offsets)
   checks+=1;selected+=n
assert selected>0
print(f'PASS {checks} linked demand-map / mapping-epoch / actual FromSPR DMA-SMC full EE/timer/display/IRQ comparisons; selected successors {selected}')
