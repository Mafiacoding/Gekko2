"""Masked IRQ event distance and full EE/IOP/latch parity vs 8:1."""
from pathlib import Path
text=Path(__file__).with_name('verify_iop_catchup_r1354.py').read_text()
exec(compile(text.split('ee=[ins(9')[0],'scheduler_fixture','exec'))
ee=[ins(9,2,2,1)]*32+[ins(4,imm=-33),0]
iop=[ins(9,4,4,1),ins(4,imm=-2),0]
checks=0
for status in [0,1,0x10001,0x10403,0x10405,0x10801]:
 for grant in [8,9,16,65,257]:
  for mode in [0,newbits]:
   start(ee,iop,mode)
   word(state+off['cop0']+12*4,status)
   call('ee_intc_mmio_write32',0x1000f010,4);call('ee_intc_raise',2)
   assert call('ee_core_event_distance',64)==64,hex(status)
   snapshot=run(grant)+bytes(u.mem_read(syms['g_ee_irq'],8))
   if not mode:expected=snapshot
   else:assert snapshot==expected,(hex(status),grant)
   checks+=1
start(ee,iop,newbits)
call('ee_intc_mmio_write32',0x1000f010,4);call('ee_intc_raise',2)
word(state+off['cop0']+12*4,0x10401)
assert call('ee_core_event_distance',64)==8
word(state+off['cop0']+12*4,0x10403)
assert call('ee_core_event_distance',64)==64
word(state+off['cop0']+12*4,0x10401)
assert call('ee_core_event_distance',64)==8
print('PASS',checks,'masked pending INTC full EE/IOP/timer/latch comparisons; unmask/EXL deadline 8/64/8')
