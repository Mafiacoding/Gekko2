#!/usr/bin/env python3
"""Synthetic PMODE-off display deadlines through actual linked EE PPC execution."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
frame=4921488
checks=0
for mask in [0,0x00787dff,0x01787dff]:
 for before in [1,2,7,8,15,16,31,32,63,64]:
  setup();word(syms['gekko2_optimization_mask'],mask);call('gs_init')
  # More than one translated block; no game instructions or MMIO bypasses.
  u.mem_write(ram+base,struct.pack('<128I',*([0x24420001]*128)))
  u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',frame-before))
  word(syms['g_ee_display_phase'],frame-before)
  u.mem_write(syms['g_ee_vblank_events'],bytes(8));u.mem_write(syms['g_ee_irq'],bytes(8))
  word(state+off['cop0']+12*4,0)
  got=call('ee_core_step_n',before+2)
  phase=int.from_bytes(u.mem_read(syms['g_ee_display_phase'],4),'big')
  events=int.from_bytes(u.mem_read(syms['g_ee_vblank_events'],8),'big')
  assert got==before+2 and phase==2 and events==1,(hex(mask),before,got,phase,events)
  checks+=1
print('PASS',checks,'linked PPC frame crossings with PMODE off: interpreter/JIT/native-window masks')
