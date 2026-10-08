"""Linked PPC profiler accounting with a controlled time-base oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
clock=[0xfffffff0]
def fake_clock(uc,address,size,user):
 if address==syms['gekko2_profile_clock']:
  uc.reg_write(UC_PPC_REG_3,clock[0]);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
hook=u.hook_add(UC_HOOK_CODE,fake_clock);dest=0x81760000
call('gekko2_profile_reset');call('gekko2_profile_start',0)
clock[0]=0x10;assert call('gekko2_profile_enter',5)==0
clock[0]=0x20;assert call('gekko2_profile_enter',8)==5
clock[0]=0x30;call('gekko2_profile_leave',5)
clock[0]=0x40;call('gekko2_profile_leave',0)
clock[0]=0x50;call('gekko2_profile_enter',1)
clock[0]=0x60;call('gekko2_profile_stop');call('gekko2_profile_get',dest)
expected=[48,16,0,0,0,32,0,0,16,0,0,1,0]
assert list(struct.unpack('>13Q',u.mem_read(dest,104)))==expected
# Exact presentation uses a separate accumulator: never added to CPU samples.
clock[0]=0x100;call('gekko2_profile_start_host')
clock[0]=0x110;assert call('gekko2_profile_enter',6)==9
clock[0]=0x120;call('gekko2_profile_leave',9)
clock[0]=0x130;call('gekko2_profile_stop');call('gekko2_profile_get_host',dest)
values=list(struct.unpack('>13Q',u.mem_read(dest,104)));assert values[6]==16 and values[9]==32 and values[10:]==[0,1,0]
call('gekko2_profile_get',dest);assert list(struct.unpack('>13Q',u.mem_read(dest,104)))==expected
call('gekko2_profile_stop');u.hook_del(hook)
print('PASS linked PPC exclusive nested timing, TB wrap, completed sample and separate exact presentation totals')
