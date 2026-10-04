"""Actual Wii ELF frontend acceptance after inherited EE/IOP/VU checks."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_iop_control_r1269.py').read_text(),'verify_ppc_iop_control_r1269.py','exec'))
reset_frontend()
new=[(1<<21)|(3<<11)|0x28,(1<<21)|0x29]
new += [(1<<26)|(1<<21)|(rt<<16)|7 for rt in (0x18,0x19)]
new += [(0x1c<<26)|(1<<21)|(2<<16)|(3<<11)|(0x1b<<6)|0x28]
new += [(0x1c<<26)|(1<<21)|(2<<16)|(3<<11)|fn for fn in (0,1,0x20,0x21)]
for n,iw in enumerate(new):assert call('ee_jit_try_execute_one_at',state,0x300000+4*n,iw)==int(enabled),hex(iw)
print('PASS actual Wii ELF gate for nine EE SA/QFSRV/MADD encodings')
if enabled:word(syms['owned_count'],4) # undo inherited synthetic full-cache fixture
new_upper=[(15<<21)|(2<<16)|(1<<11)|(3<<6)|fn for fn in list(range(16,24))+[29,31,43,47]]
new_upper += [(15<<21)|(2<<16)|(1<<11)|((idx>>2)<<6)|0x3c|(idx&3) for idx in range(16,24)]
for iw in new_upper:assert call('vu_jit_try_upper',vf,vi,vs+voff['acc'],iw)==int(enabled),hex(iw)
print('PASS actual Wii ELF gate for 20 VU MIN/MAX and conversion encodings')
