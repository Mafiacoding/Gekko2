"""Actual linked PPC: rejected SIF probes, register barriers, sampled time."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_scheduler_window_r1349.py').read_text().split('for grant in [')[0], 'scheduler_fixture', 'exec'))
seen=[]
def observe(uc,address,size,user):
 if address==syms.get('system_shared_sync_reason',syms['system_shared_sync']):
  seen.append(uc.reg_read(UC_PPC_REG_3))
u.hook_add(UC_HOOK_CODE,observe)
checks=0
for addr in [0,0x200000,0x80001000,0x1000f1f0,0x1000f270,0x1000f204,0x1000f261,0xffffffff]:
 before=len(seen);word(0x817e0000,0xdeadbeef)
 assert call('sif_mmio_read32',addr,0x817e0000)==0
 assert int.from_bytes(u.mem_read(0x817e0000,4),'big')==0xdeadbeef
 assert call('sif_mmio_write32',addr,0)==0
 assert len(seen)==before,('false barrier',hex(addr))
 checks+=2
for index in range(7):
 addr=0x1000f200+16*index
 before=len(seen)
 assert call('sif_mmio_read32',addr,0x817e0000)==1
 assert seen[before:]==[index]
 before=len(seen)
 assert call('sif_mmio_write32',addr,0)==1
 assert seen[before:]==[index+7]
 checks+=2
print('PASS',checks,'SIF claimed/rejected dispatch and exact read/write reasons')
# A sampled EE-only tail must not subtract a stale IOP timestamp. Force a
# sample at every quantum and use deterministic increasing host ticks.
clock=[0]
def advancing_clock(uc,address,size,user):
 if address in {syms['system_profile_clock'],syms['gekko2_profile_clock']}:
  clock[0]+=37;uc.reg_write(UC_PPC_REG_3,clock[0])
  uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
 if address==syms.get('system_event_catch_up'):
  word(syms['g_profile_remaining'],1)
u.hook_add(UC_HOOK_CODE,advancing_clock)
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<25)|(1<<27))
for grant in [1,2,7,8,9,64,65]:
 extension_setup()
 for name in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
  if name in syms:call(name)
 u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
 word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004)
 ee=[ins(9,2,2,1)]*32+[ins(4,imm=-33),0]
 iop=[ins(9,4,4,1),ins(4,imm=-2),0]
 u.mem_write(ram+base,struct.pack('<'+str(len(ee))+'I',*ee));u.mem_write(iram+0x100000,struct.pack('<'+str(len(iop))+'I',*iop))
 call('system_profile_reset');word(syms['g_profile_remaining'],1);before=clock[0]
 assert call('system_run_interleaved',0,grant)==0
 call('system_profile_get',0x817e0000)
 samples,ee_tb,iop_tb=struct.unpack('>3Q',u.mem_read(0x817e0000,24))
 assert samples>0 and 0<ee_tb+iop_tb<=clock[0]-before,(grant,samples,ee_tb,iop_tb,clock[0]-before)
 assert int.from_bytes(u.mem_read(ist+io['sched_ticks'],8),'big')==grant
 assert executed()==grant*8
print('PASS 7 forced-sample event runs: bounded EE/IOP ticks and exact retirements')
# Observe real active-EE barriers, not just the entry-point dispatch hook.
extension_setup()
for name in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
 if name in syms:call(name)
u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004)
ee=[ins(15,rt=1,imm=0x1000),ins(13,1,1,0xf200),ins(9,rt=2,imm=0x1234),0,0,0,0,ins(43,1,2),ins(35,1,3,0x10),ins(9,4,4,1),ins(4,imm=-3),0]
iop=[ins(15,rt=1,imm=0x1d00),ins(35,1,2),0,ins(43,1,2,0x10),ins(9,4,4,1),ins(4,imm=-5),0]
u.mem_write(ram+base,struct.pack('<'+str(len(ee))+'I',*ee));u.mem_write(iram+0x100000,struct.pack('<'+str(len(iop))+'I',*iop))
assert call('system_run_interleaved',0,64)==0
def wide(name,index):
 call(name,index)
 return (u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4)
reasons=[wide('system_sync_reason_stat',i) for i in range(19)]
assert reasons[7]>0 and reasons[1]>0,reasons
assert sum(reasons)==wide('system_event_stat',2),reasons
print('PASS actual active SIF read/write reason counters sum exactly to shared barriers')
