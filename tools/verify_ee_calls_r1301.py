"""Linked PPC block calls: near direct BL and far CTR fallback, real retirement.
No real Wii performance claim; allocator/cache services are synthetic.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ee_cop1_blocks_r1297.py').read_text(),'verify_ee_cop1_blocks_r1297.py','exec'))
if enabled:
 # The normal suite allocates in MEM1 and executes near BL callbacks.
 # Move generated code beyond +/-32 MiB to exercise the absolute fallback.
 u.mem_map(0x93000000,0x200000)
 setup();heap=0x93000000;heap_limit=0x93200000
 before=executed();assert call('ee_jit_try_execute_block',state,8)==8
 assert executed()==before+8 and reg2()==8
 words=struct.unpack('>256I',bytes(u.mem_read(0x93000000,1024)))
 assert 0x4e800421 in words # bctrl on the out-of-range callback path
 print('PASS far PPC block callback fallback: 8 precise retirements beyond BL reach')
