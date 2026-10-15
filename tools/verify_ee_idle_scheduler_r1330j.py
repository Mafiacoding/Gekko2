"""Parked EE grants must return and allow the actual IOP to run.
CPU/timers/scheduler execute linked PPC; only platform services are mocked.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
fields=['pc','next_pc','cop0','ram','ram_size','instructions_executed','halted','idle','sched_ticks']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),'+','.join('offsetof(iop_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 isize,*ioff=struct.unpack('>'+str(1+len(fields))+'I',raw.read_bytes());io=dict(zip(fields,ioff))
ist=call('iop_core_get_state');iram=0x92000000;u.mem_map(iram,0x200000)
callback=0x817fe000;u.mem_write(callback,struct.pack('>I',0x4e800020))
callbacks=0;wake=False
def boundary(uc,address,size,user):
 global callbacks
 if address!=callback:return
 callbacks+=1
 if wake and callbacks==1:u.mem_write(state+off['idle'],b'\x00')
 uc.reg_write(UC_PPC_REG_3,1);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,boundary)
def reset_idle():
 extension_setup();call('ee_hle_thread_init');u.mem_write(state+off['idle'],b'\x01')
 for n in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
  if n in syms:call(n)
 u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
 word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004)
 # Actual IOP keeps executing while EE is idle (no mock IOP callback).
 u.mem_write(iram+0x100000,struct.pack('<8I',*([0]*8)))
checks=0
for grant in [1,2,3,8,31]:
 reset_idle();before=executed()
 assert call('system_run_interleaved',0,grant)==0
 ticks=int.from_bytes(bytes(u.mem_read(ist+io['sched_ticks'],8)),'big')
 assert ticks==grant and executed()==before,(grant,ticks,executed())
 assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==100+8*grant
 assert int.from_bytes(bytes(u.mem_read(ist+io['instructions_executed'],8)),'big')==grant
 checks+=1
reset_idle();assert call('ee_core_step_n',8)==0 and executed()==0;checks+=1
if 'ee_core_step_interleaved_n' in syms:
 for grant in [1,2,3,8]:
  reset_idle();callbacks=0;wake=False
  assert call('ee_core_step_interleaved_n',grant,callback)==grant
  assert callbacks==grant and executed()==0;checks+=1
 reset_idle();callbacks=0;wake=True
 assert call('ee_core_step_interleaved_n',2,callback)==2
 assert callbacks==2 and executed()==8
 assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==116
 checks+=1
 reset_idle();callbacks=0;wake=False;u.mem_write(state+off['idle'],b'\x00')
 u.mem_write(state+3*16,struct.pack('>QQ',100,0))
 u.mem_write(ram+base,struct.pack('<16I',12,*([0]*15)))
 assert call('ee_core_step_interleaved_n',2,callback)==2
 assert callbacks==2 and executed()==15
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+64
 checks+=1
print(f'PASS {checks} linked PPC idle grants: bounded park time, actual IOP progress, no fabricated EE retirement and boundary wake')
