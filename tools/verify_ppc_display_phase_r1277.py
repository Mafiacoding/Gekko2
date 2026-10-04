"""Actual Wii ELF display-edge checks, including 64-bit oscillator wrap.
The console discards checkpoint setters; fixtures seed clock and derived phase.
Native checkpoint regressions separately test the actual load API.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_frontend_r1276.py').read_text(),'verify_ppc_frontend_r1276.py','exec'))
ee_intc=call('ee_intc_get_state')
word(state+off['cop0']+12*4,0)
for ticks in [(1<<64)-1,((1<<48)//4921488)*4921488+4921487]:
 call('gs_init');call('ee_intc_init');seed_clock(ticks)
 call('ee_core_park_tick',state)
 assert int.from_bytes(bytes(u.mem_read(syms['g_ee_display_ticks'],8)),'big')==((ticks+1)&((1<<64)-1))
 assert int.from_bytes(bytes(u.mem_read(ee_intc,4)),'big')&4
 call('gs_mmio_read64',0x12001000,0x81741000)
 assert int.from_bytes(bytes(u.mem_read(0x81741000,8)),'big')&8
 call('gs_init');call('ee_intc_init');call('ee_core_park_tick',state)
 assert not(int.from_bytes(bytes(u.mem_read(ee_intc,4)),'big')&4)
 call('gs_mmio_read64',0x12001000,0x81741000)
 assert not(int.from_bytes(bytes(u.mem_read(0x81741000,8)),'big')&8)
print('PASS actual Wii ELF cached display phase: high-clock edges and 64-bit wrap, no repeated VSYNC/VBLANK')
