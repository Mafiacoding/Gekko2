"""Warm bounded EE owner versus interpreter at branch/delay budget edges."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big');pc=base|0x80000000
checks=0
for branch in [4,5,20,21]:
 for at in [1,2,3,4]:
  program=[(9<<26)|(3<<21)|(3<<16)|1]*at+[branch<<26|1<<21|2<<16|2,(9<<26)|(4<<21)|(4<<16)|7]+[0]*(6-at)
  for grant in range(2,at+2):
   snapshots=[]
   for native in [0,1]:
    setup();word(mask_addr,(initial|1|8|512|(1<<19)|(1<<20))&~(1<<24) if native else initial&~(1|8|(1<<24)))
    u.mem_write(ram+base,struct.pack('<8I',*program));u.mem_write(state+16,struct.pack('>QQ',123,0));u.mem_write(state+32,struct.pack('>QQ',456,0))
    word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
    initial_state=bytes(u.mem_read(state,size))
    if native:
     assert call('ee_jit_try_execute_block',state,8)>0
     u.mem_write(state,initial_state)
    u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_init');call('ee_timers_tick')
    assert call('ee_core_step_n',grant)==grant
    data=bytes(u.mem_read(state,size));call('ee_timers_snapshot',0x817f0000)
    snapshots.append(data+bytes(u.mem_read(0x817f0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8)))
   assert snapshots[0]==snapshots[1],(branch,at,grant)
   checks+=1
print('PASS',checks,'warm bounded prefix/interpreter full-state cases at normal/likely branch and delay-slot grant edges')
