"""Linked PPC frontend latch and actual SIO2 sample counter/protocol."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
latch=ram+0x1000;u.mem_write(latch,bytes(8))
assert call('frontend_pad_latch_update',latch,0x4008,0)==0x4008
assert call('frontend_pad_latch_update',latch,0,0)==0x4008
assert call('frontend_pad_latch_update',latch,0,1)==0
assert call('frontend_pad_latch_update',latch,8,1)==8
assert call('frontend_pad_latch_update',latch,8,2)==8
assert call('frontend_pad_latch_update',latch,0,2)==0
call('iop_sio2_pad_connect');call('iop_sio2_pad_set_buttons',0x4008)
before=call('iop_sio2_pad_sample_count')
assert call('iop_sio2_pad_get_buttons')==0x4008
assert call('iop_sio2_pad_sample_count')==before
assert call('iop_sio2_pad_sample_buttons')==0x4008
assert call('iop_sio2_pad_sample_count')==(before+1)&0xffffffff
for b in [1,0x42,0,0,0]:call('iop_sio2_mmio_write8',0x1f808260,b)
call('iop_sio2_mmio_write32',0x1f808268,1)
assert call('iop_sio2_pad_sample_count')==(before+2)&0xffffffff
print('PASS linked PPC short press, consumed/held release, pure getter and real SIO2 read acknowledgement')
