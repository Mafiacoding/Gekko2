"""Actual ELF scheduler profile accounting and FPS window; mocked core calls/time.
Real guest retirement and IRQ semantics are covered by the inherited ELF chain.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_word_reads_r1278.py').read_text(),'verify_ppc_word_reads_r1278.py','exec'))
# Replay Remote/timeout tests too; that file starts a fresh emulator instance.
ee_offsets=off.copy();io_offsets=ioff.copy()
exec(compile(Path(__file__).with_name('verify_ppc_runtime_r1280.py').read_text(),'verify_ppc_runtime_r1280.py','exec'))
from unicorn.ppc_const import UC_PPC_REG_LR,UC_PPC_REG_PC
window=0x81743000
u.mem_write(window,bytes(296));u.mem_write(window-4,b'\x12\x34\x56\x78');u.mem_write(window+296,b'\x87\x65\x43\x21')
for k in range(12):call('frontend_fps_push',window,0,0,5000,0,int(k%2==0),0,10)
assert call('frontend_fps_guest',window)==100
assert call('frontend_fps_output',window)==2000
call('frontend_fps_push',window,0,0,5000,0,0,0,10)
assert call('frontend_fps_guest',window)==83
call('frontend_fps_push',window,0,0,0,0,999,0,999)
assert call('frontend_fps_guest',window)==83
assert bytes(u.mem_read(window-4,4))==b'\x12\x34\x56\x78' and bytes(u.mem_read(window+296,4))==b'\x87\x65\x43\x21'
# Instrument only the scheduler's callees, not its control flow.
ee=call('ee_core_get_state');io=call('iop_core_get_state')
u.mem_write(ee+ee_offsets['halted'],b'\0');u.mem_write(io+io_offsets['halted'],b'\0')
sequence=[];clock_calls=[0]
def mock(uc,address,size,data):
 if address==syms['ee_core_step_n']:
  assert uc.reg_read(UC_PPC_REG_3)==8;sequence.append('EE');uc.reg_write(UC_PPC_REG_3,8)
 elif address==syms['iop_core_step']:sequence.append('IOP')
 elif address==syms['system_profile_clock']:
  uc.reg_write(UC_PPC_REG_3,[0xfffffffa,5,12][clock_calls[0]%3]);clock_calls[0]+=1
 else:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,mock);u.ctl_remove_cache(0x80000000,0x81800000)
call('system_profile_reset');assert call('system_run_interleaved',0,2048)==0
assert sequence==['EE','IOP']*2048
result=0x81744000;call('system_profile_get',result)
samples,ee_ticks,io_ticks=struct.unpack('>3Q',bytes(u.mem_read(result,24)))
assert samples>=5 and clock_calls[0]==samples*3
assert ee_ticks==samples*11 and io_ticks==samples*7,(samples,ee_ticks,io_ticks)
call('system_profile_reset');call('system_profile_get',result)
assert bytes(u.mem_read(result,24))==bytes(24)
print('PASS actual Wii ELF 60-second FPS window rollover/bounds and scheduler interleave with randomized sampling and 32-bit host-TB wrap')
