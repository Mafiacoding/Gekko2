"""Linked-PPC event batching: exact independent states and shared SIF order."""
from pathlib import Path
text=Path(__file__).with_name('verify_scheduler_window_r1349.py').read_text()
exec(compile(text.split('for grant in [')[0], 'scheduler_fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
newbits=(1<<25)|(1<<26)|(1<<27)
def start(ee,iop,flags):
 word(mask,(initial|1|8|(1<<20)|(1<<24))&~newbits|flags)
 extension_setup()
 for name in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init','sif_reset']:
  if name in syms:call(name)
 u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
 word(ist+io['pc'],0x100000);word(ist+io['next_pc'],0x100004)
 u.mem_write(ram+base,struct.pack('<'+str(len(ee))+'I',*ee));u.mem_write(iram+0x100000,struct.pack('<'+str(len(iop))+'I',*iop))
 u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick');call('iop_timers_tick')
def run(grant):
 assert call('system_run_interleaved',0,grant)==0
 assert int.from_bytes(u.mem_read(ist+io['sched_ticks'],8),'big')==grant
 assert executed()==grant*8
 call('ee_timers_snapshot',0x817e0000)
 for index,addr in enumerate([0x1f801100,0x1f801110,0x1f801120,0x1f801480,0x1f801490,0x1f8014a0]):
  for k in range(3):assert call('iop_timers_mmio_read32',addr+4*k,0x817e0100+index*12+4*k)==1
 return bytes(u.mem_read(state,size))+bytes(u.mem_read(ist,isize))+bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(0x817e0100,72))
ee=[ins(9,2,2,1)]*32+[ins(4,imm=-33),0]
iop=[ins(9,4,4,1),ins(4,imm=-2),0]
checks=0;costs={}
for grant in [1,2,3,7,8,9,16,17,31,63,64,65,257]:
 for mode in [0,newbits]:
  start(ee,iop,mode);total[0]=0;active[0]=True;snapshot=run(grant);active[0]=False
  costs[mode,grant]=total[0]
  if not mode:expected=snapshot
  else:
   assert snapshot==expected,('independent full state',grant,next((i for i,(a,b) in enumerate(zip(snapshot,expected)) if a!=b),None))
  checks+=1
print('PASS',checks,'full EE/IOP/timer state comparisons against exact 8:1 interleave')
print('BATCH_COST', {g:{'legacy':costs[0,g],'event':costs[newbits,g]} for g in [64,65,257]})
# EE publishes then polls IOP echo; barriers catch up before register accesses.
ee=[ins(15,rt=1,imm=0x1000),ins(13,1,1,0xf200),ins(9,rt=2,imm=0x1234)]+[ins(9,5,5,1)]*29+[ins(43,1,2),ins(35,1,3,0x10),ins(9,4,4,1),ins(4,imm=-3),0]
iop=[ins(15,rt=1,imm=0x1d00),ins(35,1,2),0,ins(43,1,2,0x10),ins(9,4,4,1),ins(4,imm=-5),0]
for grant in [8,9,16,31,64,65]:
 start(ee,iop,newbits);run(grant)
 if grant>=16:
  assert int.from_bytes(u.mem_read(ist+8,4),'big')==0x1234
  assert int.from_bytes(u.mem_read(state+48,8),'big')==0x1234
print('PASS 6 shared mailbox publication / echo grants with exact tick ratio')
call('system_event_stat',0);assert u.reg_read(UC_PPC_REG_4)>0
call('system_event_stat',2);assert u.reg_read(UC_PPC_REG_4)>0
print('PASS event batches and shared barriers actually exercised')
# Exclude first-time translation from the warm dispatch comparison.
warm={}
ee=[ins(9,2,2,1)]*32+[ins(4,imm=-33),0]
iop=[ins(9,4,4,1),ins(4,imm=-2),0]
for mode in [0,1<<25,newbits]:
 start(ee,iop,mode)
 assert call('system_run_interleaved',0,512)==0
 total[0]=0;active[0]=True
 assert call('system_run_interleaved',0,256)==0
 active[0]=False;warm[mode]=total[0]
print('WARM_SCHEDULER_COST', {'legacy':warm[0],'event_only':warm[1<<25],'event_long_idle':warm[newbits],'slices':256})
