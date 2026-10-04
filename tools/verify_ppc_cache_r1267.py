"""Execute actual Wii JIT cache + real translator, with controlled platform allocation.
The inherited suite checks 188 shared-core behaviors; this adds frontend tests.
Allocator and cache-maintenance calls are emulated; guest computation is real PPC.
Works on R1266 as a baseline for repeated unsupported-opcode allocation counts.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_filter_r1266.py').read_text(),'verify_ppc_filter_r1266.py','exec'))
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_4
heap=0x81600000;heap_limit=0x81700000;allocs=frees=0;fail_alloc=False
hooks={syms[n]:n for n in ['memalign','free','DCFlushRange','ICInvalidateRange']}
def platform(uc,address,size,user):
 global heap,allocs,frees,fail_alloc
 name=hooks.get(address)
 if name is None:return
 if name=='memalign':
  allocs+=1
  if fail_alloc:ret=0
  else:
   align=uc.reg_read(UC_PPC_REG_3);n=uc.reg_read(UC_PPC_REG_4)
   heap=(heap+align-1)&~(align-1);ret=heap;heap+=n
   assert heap<heap_limit,'test heap exhausted'
  uc.reg_write(UC_PPC_REG_3,ret)
 elif name=='free':frees+=1
 elif name=='ICInvalidateRange':
  start=uc.reg_read(UC_PPC_REG_3);n=uc.reg_read(UC_PPC_REG_4)
  uc.ctl_remove_cache(start,start+n)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,platform)
u.ctl_remove_cache(0x80000000,0x81800000)
modern='g_pc_l0_rejected_hits' in syms
def reset_frontend():
 for name,n in [('g_cache',8192*8),('g_l1',2048*8),('g_pc_l0',16384*(16 if modern else 12)),('g_ee_rejected',64*8),('g_cache_count',4),('g_jit_executed',8),('g_pc_l0_hits',8),('g_pc_l0_misses',8)]:
  if name in syms:u.mem_write(syms[name],bytes(n))
 for name in ['g_pc_l0_rejected_hits','g_compile_attempts']:
  if name in syms:u.mem_write(syms[name],bytes(8))
def counter(name):return int.from_bytes(bytes(u.mem_read(syms[name],8)),'big')
reset_frontend();u.mem_write(state,bytes(544))
pc=0x200000;bad=(28<<26)|0x09;good=(9<<26)|(2<<16)|7
if 'Interpreter' in Path(a.elf).name:
 for iw in [bad,good]:assert call('ee_jit_try_execute_one_at',state,pc,iw)==0
 assert allocs==0
 print('PASS 1 actual Wii ELF JIT-disabled allocation guard')
else:
 for n in range(1000):
  assert call('ee_jit_try_execute_one_at',state,pc,bad)==0
 assert bytes(u.mem_read(state,544))==bytes(544),'unsupported opcode mutated register file'
 assert allocs==(1 if modern else 1000) and frees==allocs,(allocs,frees)
 print('MEASURE unsupported opcode: 1000 calls,',allocs,'code allocations,',frees,'frees')
 if modern:
  assert counter('g_pc_l0_rejected_hits')==999
  assert counter('g_compile_attempts')==1
  assert call('ee_jit_try_execute_one_at',state,pc,good)==1
  assert int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==7
  assert call('ee_jit_try_execute_one_at',state,pc,bad)==0 and allocs==(2 if 'g_ee_rejected' in syms else 3)
  assert call('ee_jit_try_execute_one_at',state,pc,good)==1 and allocs==(2 if 'g_ee_rejected' in syms else 3)
  reset_frontend();before=allocs;fail_alloc=True
  assert call('ee_jit_try_execute_one_at',state,pc,good)==0
  fail_alloc=False
  assert call('ee_jit_try_execute_one_at',state,pc,good)==1 and allocs==before+2
  assert counter('g_pc_l0_rejected_hits')==0
  before=allocs;word(syms['g_cache_count'],8192)
  for n in range(1000):assert call('ee_jit_try_execute_one_at',state,pc+4,good+1)==0
  assert allocs==before,'full cache allocated unowned code'
  assert call('ee_jit_try_execute_one_at',state,pc+8,good)==1 and allocs==before
  print('PASS 7 actual Wii ELF JIT frontend groups: unsupported reuse, unchanged registers, word replacement, owned reuse, transient allocation retry, full-cache bounds, full-cache positive reuse')

# Verify that a negative frontend hit still executes the interpreter and the
# normal guest PC epilogue through the real ee_core_step_n entry point.
u.mem_write(state+off['halted'],b'\0');word(state+off['cop0']+12*4,0)
guest(0x80007000,bad)
for n in range(2):
 word(state+off['pc'],0x80007000);word(state+off['next_pc'],0x80007004)
 u.mem_write(state+off['branch_pending'],b'\0')
 call('ee_core_step_n',1)
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80007004
 assert bytes(u.mem_read(state+off['halted'],1))==b'\0'
assert int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==(0 if 'Interpreter' in Path(a.elf).name else 7)
print('PASS 1 actual Wii ELF EE fallback integration group: rejected instruction retires twice, PC advances, no halt, registers preserved')
