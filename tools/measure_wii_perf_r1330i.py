"""Warm linked-PPC instruction/call counts; platform services are mocked.
These are dispatch-cost measurements, never Wii cycle or FPS predictions.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
callback=0x817fe000;u.mem_write(callback,struct.pack('>2I',0x38600001,0x4e800020))
transitions={};targets={syms[n]:n for n in ['ee_core_block_prepare','ee_core_block_prepare_memory_resolved','ee_core_block_prepare_delay','ee_core_block_commit','ee_core_block_boundary','ee_core_block_memory_boundary','ee_core_block_delay_boundary'] if n in syms}
def track(uc,address,size,user):
 if active[0] and address in targets:transitions[targets[address]]=transitions.get(targets[address],0)+1
u.hook_add(UC_HOOK_CODE,track)
def reset():
 extension_setup()
 for n in range(64):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
 call('ee_core_step_n',128)
 word(state+off['pc'],base);word(state+off['next_pc'],base+4);u.mem_write(state+off['branch_pending'],b'\0')
 word(state+off['cop0']+9*4,100);u.mem_write(state+off['instructions_executed'],bytes(8));u.mem_write(state,bytes(512))
results={}
for kind in ['block8','grant64']:
 reset();transitions={};total[0]=0;active[0]=True
 if kind=='block8':assert call('ee_core_step_n',8)==8
 elif 'ee_core_step_interleaved_n' in syms:assert call('ee_core_step_interleaved_n',8,callback)==8
 else:
  syms['_callback']=callback
  for n in range(8):assert call('ee_core_step_n',8)==8;assert call('_callback')==1
 active[0]=False
 results[kind]={'ppc_instructions':total[0],'boundary_calls':sum(transitions.values()),'calls':transitions,'retired':executed(),'state_sha256':hashlib.sha256(bytes(u.mem_read(state,size))).hexdigest()}
print('WII_PERF_MEASURE '+json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'linked PPC with mocked allocation/cache services and two-instruction IOP callback; not hardware cycles/FPS'}))
