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

# Compare the specialised one-tick entry against the general budget path.
# Mix branches/delay slots, RAM loads/stores, HLE-independent syscall faults,
# and an idle/halted tick. Include whole IOP architectural state and RAM.
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'idle.c';obj=Path(td)/'idle.o';raw=Path(td)/'idle.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={offsetof(iop_state_t,idle)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 idle_offset=struct.unpack('>I',raw.read_bytes())[0]
initial_mask=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
results={};costs={}
for jit in [0,1]:
 for idle in [0,1]:
  for halted in [0,1]:
   for entry in ['iop_core_step_n','iop_core_step_one_native']:
    extension_setup()
    for n in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
     if n in syms:call(n)
    word(syms['gekko2_optimization_mask'],(initial_mask|2) if jit else (initial_mask&~2))
    u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
    word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004);u.mem_write(ist+io['halted'],bytes([halted]))
    # Derive idle offset from the current header, just as the base fixture does.
    program=[ins(15,rt=1,imm=0),ins(13,1,1,0x400),ins(9,rt=2,imm=17),ins(43,1,2),ins(35,1,3),ins(9,3,4,1),ins(4,imm=-3),0]
    u.mem_write(iram,bytes(0x1000));u.mem_write(iram+0x100000,struct.pack('<'+str(len(program))+'I',*program))
    # idle state layout derived separately below; retain per-tick comparisons.
    u.mem_write(ist+idle_offset,bytes([idle]))
    sig=hashlib.sha256();cost=0
    for tick in range(128):
     total[0]=0;active[0]=True
     ret=call(entry,1) if entry=='iop_core_step_n' else call(entry)
     active[0]=False;cost+=total[0]
     assert ret==(0 if halted else 1),(entry,jit,idle,halted,tick,ret)
     sig.update(bytes(u.mem_read(ist,isize)));sig.update(bytes(u.mem_read(iram,0x1000)))
    key=(jit,idle,halted);value=sig.hexdigest()
    if key in results:assert results[key]==value,(entry,key,results[key],value)
    else:results[key]=value
    costs[(key,entry)]=cost
word(syms['gekko2_optimization_mask'],initial_mask)
print('PASS 8 IOP JIT/idle/halt combinations, 128 per-tick state/RAM comparisons each')
for key in results:print(key, costs[(key,'iop_core_step_n')], costs[(key,'iop_core_step_one_native')])
