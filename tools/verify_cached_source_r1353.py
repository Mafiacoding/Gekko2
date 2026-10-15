"""Warm event windows must observe real CPU writes and mapping epochs."""
from pathlib import Path
text=Path(__file__).with_name('verify_long_ee_r1353.py').read_text()
exec(compile(text.split('programs=')[0],'source_fixture','exec'))
word(mask_addr,initial|1|8|8192|512|(1<<20)|(1<<24)|(1<<27))
setup();u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick')
one=(9<<26)|(2<<21)|(2<<16)|1;two=one+1
u.mem_write(ram+base,struct.pack('<32I',*([one]*32)))
def run(expected):
 word(state+off['pc'],pc);word(state+off['next_pc'],pc+4);u.mem_write(state+32,bytes(16))
 assert call('ee_jit_try_execute_block',state,32)==32
 assert int.from_bytes(u.mem_read(state+32,8),'big')==expected
run(32)
call('ee_mem_write32',state,pc,two)
run(33)
call('ee_jit_notify_mapping_change');run(33)
call('ee_mem_write32',state,pc,one);run(32)
call('ee_jit_notify_physical_write',base,128);run(32)
print('PASS warm cached event windows after actual CPU code writes, restored bytes, mapping epoch and source generation changes')
