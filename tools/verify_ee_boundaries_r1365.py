"""Linked-PPC admission at special PCs and 32-bit address wrap.
Run unchanged on R1364 and R1365 and compare full resulting EE state.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
word(mask,initial|1|8|512|8192|(1<<20)|(1<<24)|(1<<27))
special=[0x81fc0,0x82000,0x8000cfcc,0x8000fd74,0x8000fe2c]
cases=[]
for target in special:
 for count in [2,8,32]:
  for delta in [-4*count-4,-4*count,-4,0,4]:
   cases.append(((target+delta)&0xffffffff,count))
for pc in [0xfffffff8,0xfffffffc,0,4,0x8000cfcb]:
 cases.append((pc,8))
results=[]
for pc,count in cases:
 setup();word(state+off['pc'],pc);word(state+off['next_pc'],(pc+4)&0xffffffff)
 u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8))
 call('ee_timers_init');call('ee_timers_tick')
 result=call('ee_core_native_window_begin_impl',state,pc,ram+base,count,count,0,1)
 expected=0 if any(((pc+4*(i+1))&0xffffffff) in special for i in range(count)) else count
 assert result==expected,(hex(pc),count,result,expected)
 results.append([pc,count,result,hashlib.sha256(bytes(u.mem_read(state,size))).hexdigest()])
print(json.dumps(results,separators=(',',':')))
