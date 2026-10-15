"""Linked PPC native integer windows against the existing precise path."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
pc=base|0x80000000
programs=[[(9<<26)|(2<<21)|(2<<16)|1]*32,
 [(9<<26)|(1<<21)|(1<<16)|4]+[(35<<26)|(1<<21)|(2<<16)]*31,
 [(15<<26)|(1<<16)|0x8030,(13<<26)|(1<<21)|(1<<16)|64]+[(35<<26)|(1<<21)|(2<<16)]*30]
checks=0
for kind,program in enumerate(programs):
 for grant in [9,16,24,31,32]:
  for mode in [0,1,2]:
   setup();word(mask_addr,(initial|1|8|8192|512|(1<<20)|(1<<27))&~(1<<24)|(1<<24 if mode==2 else 0))
   u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick')
   u.mem_write(ram+base,struct.pack('<32I',*program));word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   u.mem_write(state+16,struct.pack('>QQ',0x80300000,0));u.mem_write(ram+0x300000,struct.pack('<I',0x80300040))
   if mode==0:
    word(mask_addr,int.from_bytes(u.mem_read(mask_addr,4),'big')&~1)
    assert call('ee_core_step_n',grant)==grant
   else:assert call('ee_jit_try_execute_block',state,grant)==grant
   snapshot=bytes(u.mem_read(state,size));call('ee_timers_snapshot',0x817f0000);snapshot+=bytes(u.mem_read(0x817f0000,64))
   snapshot+=bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
   if mode==0:expected=snapshot
   else:assert snapshot==expected,(kind,grant,mode,next((i for i,(a,b) in enumerate(zip(snapshot,expected)) if a!=b),None))
   checks+=1
print('PASS',checks,'complete long interpreter / precise / event EE states for grants9..32')
