"""Linked PPC native integer windows against the existing precise path."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
pc=base|0x80000000
programs=[[(9<<26)|(2<<21)|(2<<16)|1]*8,
 [(13<<26)|(1<<16)|123,(9<<26)|(1<<21)|(2<<16)|7,(2<<21)|(3<<11),
  (2<<21)|(3<<16)|(4<<11)|37,(4<<21)|(5<<16)|(6<<11)|35,(12<<26)|(4<<21)|(7<<16)|15,
  (7<<21)|(6<<16)|(8<<11)|38,(9<<26)|(8<<21)|(8<<16)|1]]
programs += [[(1<<21)|(2<<16)|(3<<11)|37]*8,[(35<<26)|(1<<21)|(2<<16)]*8]
signatures={};costs=[0,0];checks=0
for enabled in [0,1]:
 word(mask_addr,(initial|1|8|8192|512|(1<<20))&~(1<<24)|(enabled<<24))
 for kind,program in enumerate(programs):
  for grant in range(2,9):
   setup();u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));u.mem_write(ram+base,struct.pack('<8I',*program));word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   u.mem_write(state+16,struct.pack('>QQ',0x80300000,0));u.mem_write(state+32,struct.pack('>QQ',0x123456789abcdef0,0));u.mem_write(ram+0x300000,struct.pack('<I',123))
   call('ee_timers_tick') # establish a materialized event-free timer horizon
   hi=call('ee_core_native_window_stat',0);before_native=(hi<<32)|u.reg_read(UC_PPC_REG_4)
   assert call('ee_jit_try_execute_block',state,grant)==grant
   if enabled:
    hi=call('ee_core_native_window_stat',0);assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==before_native+1,(kind,grant,'window not used')
   snapshot=bytes(u.mem_read(state,size));call('ee_timers_snapshot',0x817f0000);snapshot+=bytes(u.mem_read(0x817f0000,64))
   snapshot+=bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))
   snapshot+=bytes(u.mem_read(syms['g_ee_irq'],8))+bytes(u.mem_read(syms['g_bus_tick_counter'],8))
   if not enabled:signatures[kind,grant]=snapshot
   else:assert snapshot==signatures[kind,grant],(kind,grant)
   checks+=1
 # Warm count. Cache code is generated once, timer horizon stays unexposed.
 setup();u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));u.mem_write(ram+base,struct.pack('<8I',*programs[0]));call('ee_timers_tick')
 def run():
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  assert call('ee_jit_try_execute_block',state,8)==8
 run();total[0]=0;active[0]=True
 for iteration in range(100):run()
 active[0]=False;costs[enabled]=total[0]
 if enabled:
  call('ee_core_native_window_stat',1);assert u.reg_read(UC_PPC_REG_4)>0
print('PASS',checks,'native/precise complete EE, timer, IRQ and display states for grants2..8')
print('NATIVE_WINDOW_COST',json.dumps({'precise':costs[0],'native_window':costs[1],'blocks':100}))

word(mask_addr,initial|1|8|8192|512|(1<<20)|(1<<24))
for mode in ['timer','vblank','count_irq','intc','bad_source','changed_base','mmio']:
 setup();word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
 u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000)
 u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick')
 program=programs[0][:]
 if mode=='timer':word(syms['ee_timers_batch_clock'],8)
 if mode=='vblank':word(syms['g_ee_display_phase'],4921488//12-1)
 if mode=='count_irq':word(state+off['cop0']+11*4,104);word(state+off['cop0']+12*4,0x18001)
 if mode=='intc':word(syms['gekko2_intc_live'],1);word(syms['gekko2_intc_live']+4,1)
 if mode=='changed_base':program=[(9<<26)|(1<<21)|(1<<16)|4]+[(35<<26)|(1<<21)|(2<<16)]*7
 if mode=='mmio':program=[(35<<26)|(1<<21)|(2<<16)]*8;u.mem_write(state+16,struct.pack('>QQ',0x10000000,0))
 u.mem_write(ram+base,struct.pack('<8I',*program));u.mem_write(0x81750000,struct.pack('>8I',*program))
 if mode=='bad_source':u.mem_write(ram+base,struct.pack('<I',0))
 before=bytes(u.mem_read(state,size))+bytes(u.mem_read(syms['ee_timers_batch_clock'],8))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))
 assert call('ee_core_native_window_begin',state,pc,0x81750000,8,8)==0,mode
 after=bytes(u.mem_read(state,size))+bytes(u.mem_read(syms['ee_timers_batch_clock'],8))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))
 assert before==after,mode
print('PASS seven pure admission declines: timer, vblank, Count IRQ, INTC, source mutation, changed base and MMIO')
