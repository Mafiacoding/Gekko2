"""Count actual linked PPC instructions; platform services mocked, no Wii FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
results={}
for mode in ['retire_idle','aot_miss']:
 extension_setup()
 for name in ['vif_init','vu1_init','sif_reset']:
  if name in syms:call(name)
 # Every guest line masked: exercise retirement without taking a CPU IRQ.
 word(state+off['cop0']+12*4,0)
 if mode=='retire_idle':
  function='ee_core_block_commit';args=(state,);count=128
 else:
  word(state+off['pc'],0x00555554);word(state+off['next_pc'],0x00555558)
  function='ee_aot_try_execute_fetched';args=(state,1,0);count=128
  assert call(function,*args)==0
 total[0]=0;active[0]=True
 for _ in range(count):
  value=call(function,*args)
  if mode=='aot_miss':assert value==0
 active[0]=False
 call('ee_timers_snapshot',0x817e0000)
 snapshot=bytes(u.mem_read(state,size))+bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
 results[mode]={'calls':count,'PPC_instructions':total[0],'state_sha256':hashlib.sha256(snapshot).hexdigest()}
print(json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Linked PPC instruction count including helper entry/return; mocked platform; not Broadway cycles or game FPS.'},indent=2))
