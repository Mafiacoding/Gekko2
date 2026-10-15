"""RAM stores must not synchronize SIF; actual PPC synthetic comparison."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_scheduler_window_r1349.py').read_text().split('for grant in [')[0], 'scheduler_fixture', 'exec'))
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<25)|(1<<27))
extension_setup()
for name in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
 if name in syms:call(name)
u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004)
import os
mmio=bool(os.environ.get("GEKKO2_TEST_FALSE_MMIO"))
ee=[ins(15,rt=1,imm=0x8000),ins(13,1,1,0x4000),ins(9,2,2,1),ins(43,1,2),ins(4,imm=-3),0]
if mmio:ee=[ins(15,rt=1,imm=0x1000),ins(13,1,1,0xf000),ins(35,1,2),ins(9,3,3,1),ins(4,imm=-3),0]
iop=[ins(9,4,4,1),ins(4,imm=-2),0]
u.mem_write(ram+base,struct.pack('<'+str(len(ee))+'I',*ee));u.mem_write(iram+0x100000,struct.pack('<'+str(len(iop))+'I',*iop))
assert call('system_run_interleaved',0,512)==0
def stat(index):
 call('system_event_stat',index)
 return (u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4)
before=[stat(i) for i in range(5)]
total[0]=0;active[0]=True
assert call('system_run_interleaved',0,256)==0
active[0]=False;cost=total[0]
after=[stat(i) for i in range(5)]
assert executed()==768*8
assert int.from_bytes(u.mem_read(ist+io['sched_ticks'],8),'big')==768
snapshot=bytes(u.mem_read(state,size))+bytes(u.mem_read(ist,isize))+bytes(u.mem_read(ram+0x4000,4))
print('RAM_STORE_RESULT',json.dumps({'elf':Path(a.elf).name,'PPC_instructions':cost,'event_delta':[a-b for a,b in zip(after,before)],'state_sha256':hashlib.sha256(snapshot).hexdigest(),'measured_IOP_ticks':256,'measured_EE_retired':2048,'scope':('synthetic warm INTC-read loop' if mmio else 'synthetic warm RAM-store loop')+'; not Wii BIOS FPS'}))
