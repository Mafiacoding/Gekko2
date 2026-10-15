"""Exact linked IOP idle state, timers and IRQ boundaries vs tick stepping."""
from pathlib import Path
text=Path(__file__).with_name('verify_event_scheduler_r1353.py').read_text()
exec(compile(text.split("ee=[ins(9")[0],'idle_fixture','exec'))
fields2=['idle','branch_delay_pending','load_delay_reg','exception_pending']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={'+','.join('offsetof(iop_state_t,'+f+')' for f in fields2)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 extra=dict(zip(fields2,struct.unpack('>4I',raw.read_bytes())))
def snapshot():
 for index,addr in enumerate([0x1f801100,0x1f801110,0x1f801120,0x1f801480,0x1f801490,0x1f8014a0]):
  for k in range(3):call('iop_timers_mmio_read32',addr+4*k,0x817e0100+index*12+4*k)
 return bytes(u.mem_read(ist,isize))+bytes(u.mem_read(0x817e0100,72))
checks=0
for case in ['plain','vblank_start','vblank_end','timer_irq','pending_irq','thread_hint','load_delay']:
 for grant in [2,3,7,8,31,64]:
  for enabled in [0,1]:
   start([0]*32,[0]*128,(1<<26) if enabled else 0)
   word(mask,(initial|(1<<20)|(1<<21))&~(1<<26)|(enabled<<26))
   u.mem_write(ist+extra['idle'],b'\1')
   if case=='vblank_start':u.mem_write(ist+io['sched_ticks'],struct.pack('>Q',4921488//8-3))
   if case=='vblank_end':u.mem_write(ist+io['sched_ticks'],struct.pack('>Q',(4921488//8)//12-3))
   if case=='timer_irq':
    call('iop_timers_mmio_write32',0x1f801108,3);call('iop_timers_mmio_write32',0x1f801104,0x50)
    call('iop_intc_mmio_write32',0x1f801074,1<<4);word(ist+io['cop0']+12*4,0x401)
   if case=='pending_irq':
    call('iop_intc_raise',4);call('iop_intc_mmio_write32',0x1f801074,1<<4);word(ist+io['cop0']+12*4,0x401)
   if case=='thread_hint':
    word(ist+124,0x100000)
    assert call('iop_hle_thread_try_handle',ist,0x100)==1
   if case=='load_delay':u.mem_write(ist+extra['load_delay_reg'],b'\2')
   assert call('iop_core_step_n',grant)==grant
   state_bytes=snapshot()
   if not enabled:expected=state_bytes
   else:assert state_bytes==expected,(case,grant,next((i for i,(a,b) in enumerate(zip(state_bytes,expected)) if a!=b),None))
   checks+=1
print('PASS',checks,'full parked IOP / timer states, VBlank and interrupt wake boundaries')
call('iop_core_idle_batch_stat',0);assert u.reg_read(UC_PPC_REG_4)>0
print('PASS idle batching actually exercised without synthetic retirement')
