"""Real EE/IOP/SIF code in the linked Wii ELF; platform services mocked.
Compare signatures with the uploaded-source r32 baseline to verify scheduler
order through actual IOP execution and shared mailbox transfers.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
fields=['pc','next_pc','cop0','ram','ram_size','instructions_executed','halted','sched_ticks']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),'+','.join('offsetof(iop_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 isize,*ioff=struct.unpack('>9I',raw.read_bytes());io=dict(zip(fields,ioff))
ist=call('iop_core_get_state');iram=0x92000000;u.mem_map(iram,0x200000)
consoles={syms[n] for n in ['printf','puts','fprintf','write'] if n in syms}
def console(uc,address,size,user):
 if address in consoles:uc.reg_write(UC_PPC_REG_3,0);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,console)
def ins(op,rs=0,rt=0,imm=0):return (op<<26)|(rs<<21)|(rt<<16)|(imm&65535)
sig=hashlib.sha256();checks=0
for grant in [512]:
 extension_setup()
 for n in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
  if n in syms:call(n)
 u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
 word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004)
 # EE publishes MSCOM at instruction 8, then polls the IOP's echo.
 ee=[ins(15,rt=1,imm=0x1000),ins(13,1,1,0xf200),ins(9,rt=2,imm=0x1234),0,0,0,0,ins(43,1,2),ins(35,1,3,0x10),ins(9,4,4,1),ins(4,imm=-3),0]
 iop=[ins(15,rt=1,imm=0x1d00),ins(35,1,2),0,ins(43,1,2,0x10),ins(9,4,4,1),ins(4,imm=-5),0]
 u.mem_write(ram+base,struct.pack('<'+str(len(ee))+'I',*ee))
 u.mem_write(iram+0x100000,struct.pack('<'+str(len(iop))+'I',*iop))
 assert call('system_run_interleaved',0,64)==0
 total[0]=0;active[0]=True
 assert call('system_run_interleaved',0,grant)==0 # uint64 argument in r3/r4
 active[0]=False;print('SYSTEM_COST',json.dumps({'PPC_instructions':total[0],'IOP_ticks':grant,'EE_slots':grant*8}))
 ticks=int.from_bytes(bytes(u.mem_read(ist+io['sched_ticks'],8)),'big')
 assert ticks==grant+64 and executed()==8*(grant+64),(grant,ticks,executed())
 if grant>=9:
  assert int.from_bytes(bytes(u.mem_read(ist+8,4)),'big')==0x1234
  assert int.from_bytes(bytes(u.mem_read(state+48,8)),'big')==0x1234
 snapshot=bytes(u.mem_read(state,size))+bytes(u.mem_read(ist,isize))
 sig.update(snapshot);checks+=1
print('WII_SYSTEM_SIGNATURE '+sig.hexdigest())
print(f'PASS {checks} linked EE/IOP/SIF grants with exact tick/retirement totals and mailbox echo')
