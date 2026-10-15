"""Mixed-owner prefix windows preserve complete EE retirement and RAM writes."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask']
initial=int.from_bytes(u.mem_read(mask_addr,4),'big');pc=base|0x80000000
signatures={};checks=0;costs={}
for enabled in [0,1]:
 word(mask_addr,initial|1|8|8192|512|(1<<20)|(enabled<<24))
 for prefix in range(2,8):
  setup();u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8))
  program=[(9<<26)|(2<<21)|(2<<16)|1]*prefix+[(43<<26)|(1<<21)|(2<<16)]+[(9<<26)|(3<<21)|(3<<16)|1]*(7-prefix)
  u.mem_write(ram+base,struct.pack('<8I',*program));u.mem_write(ram+0x300000,bytes(4));u.mem_write(state+16,struct.pack('>QQ',0x80300000,0))
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4);call('ee_timers_tick')
  call('ee_jit_try_execute_block',state,8) # establish a full warm mixed owner
  u.mem_write(state,bytes(size));word(state+off['ram'],ram);word(state+off['ram_size'],0x400000);word(state+off['cop0']+9*4,100)
  u.mem_write(state+16,struct.pack('>QQ',0x80300000,0));u.mem_write(ram+0x300000,bytes(4))
  u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8))
  call('ee_timers_init');call('ee_timers_tick');word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  before=call('ee_core_native_window_stat',0);before=(before<<32)|u.reg_read(UC_PPC_REG_4)
  retired=call('ee_jit_try_execute_block',state,8)
  assert retired==(prefix if enabled else 8),(enabled,prefix,retired)
  if enabled:
   after=call('ee_core_native_window_stat',0);assert ((after<<32)|u.reg_read(UC_PPC_REG_4))==before+1
  if retired<8:call('ee_core_step_n',8-retired)
  assert executed()==8 and int.from_bytes(u.mem_read(ram+0x300000,4),'little')==prefix
  snapshot=bytes(u.mem_read(state,size));call('ee_timers_snapshot',0x817f0000);snapshot+=bytes(u.mem_read(0x817f0000,64))
  snapshot+=bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
  if enabled:assert snapshot==signatures[prefix],prefix
  else:signatures[prefix]=snapshot
  total[0]=0;active[0]=True
  for iteration in range(100):
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   assert call('ee_jit_try_execute_block',state,prefix)==prefix
  active[0]=False;costs[enabled,prefix]=total[0]
  if enabled:
   cap=call('ppc_code_cache_capacity');used=call('ppc_code_cache_used');hold=call('ppc_code_cache_alloc',cap-used-32);assert hold
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   assert call('ee_jit_try_execute_block',state,prefix)==prefix
   call('ppc_code_cache_release',hold);call('ee_jit_reset_stats_for_test');assert call('ppc_code_cache_used')==0
  checks+=1
print('PASS',checks,'mixed owner prefixes length2..7; complete EE/timer/IRQ/display state and following RAM store')
print('PASS cached prefix windows under full foreign arena; all owner variants released at teardown')
print('WARM_PREFIX_COST',json.dumps({str(n):{'precise':costs[0,n],'native':costs[1,n]} for n in range(2,8)}))
