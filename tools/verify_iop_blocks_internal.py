"""Linked PPC IOP block / scalar differential and byte-memory oracle.
Platform allocation/cache maintenance are mocked; IOP hooks and ticks run.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
import random
fields=['pc','next_pc','hi','lo','cop0','ram','ram_size','bios','instructions_executed','halted','sched_ticks','idle','exception_pending','devtable_pending_image']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),'+','.join('offsetof(iop_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 isize,*offsets=struct.unpack('>'+str(1+len(fields))+'I',raw.read_bytes());io=dict(zip(fields,offsets))
ist=call('iop_core_get_state');ibase=0x100000;iram=ram
rng=random.Random(0x1307);sig=hashlib.sha256();cases=0
# Logs are not CPU semantics; suppress libc console/device waits.
def console(uc,address,size,user):
 if address in consoles:
  uc.reg_write(UC_PPC_REG_3,0);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
consoles={syms[n] for n in ['printf','puts','fprintf'] if n in syms};u.hook_add(UC_HOOK_CODE,console)
def setup(words,regs=None):
 global heap
 call('iop_jit_reset_for_test');heap=0x81600000
 for name in ['iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init']:
  call(name)
 u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
 word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
 u.mem_write(iram+ibase,struct.pack('<'+str(len(words))+'I',*words)+bytes(64))
 u.mem_write(iram+0x180000,bytes(range(128)))
 if regs is not None:u.mem_write(ist,struct.pack('>32I',*regs))
def snapshot():
 return bytes(u.mem_read(ist,isize))+bytes(u.mem_read(iram+0x180000,128))+struct.pack('>I',call('iop_mem_read32',ist,0x1f801070))
def run_case(words,budget,regs=None,extra=None):
 global cases
 states=[]
 for native in [False,True]:
  setup(words,regs)
  if extra:extra()
  if native:done=call('iop_core_step_n',budget);assert done==budget or bytes(u.mem_read(ist+io['halted'],1))!=b'\0'
  else:
   done=0
   for n in range(budget):
    if bytes(u.mem_read(ist+io['halted'],1))!=b'\0':break
    call('iop_core_step');done+=1
  states.append((done,snapshot()))
 assert states[0]==states[1],('state mismatch',cases,[hex(w) for w in words],budget,states[0][0],states[1][0])
 sig.update(states[0][1]);cases+=1
 if cases%100==0:print('IOP_BLOCK_PROGRESS',cases,flush=True)
# Mixed instruction sequences, all supported scalar body classes.
imm_ops=[8,9,10,11,12,13,14,15]
functs=[0,2,3,4,6,7,0x10,0x11,0x12,0x13,0x18,0x19,0x1a,0x1b,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b]
for n in range(160):
 regs=[rng.getrandbits(32) for _ in range(32)];regs[0]=0
 words=[]
 for j in range(16):
  rs,rt,rd=[rng.randrange(32) for _ in range(3)]
  if j%2:words.append((rng.choice(imm_ops)<<26)|(rs<<21)|(rt<<16)|rng.randrange(65536))
  else:words.append((rs<<21)|(rt<<16)|(rd<<11)|(rng.randrange(32)<<6)|rng.choice(functs))
 run_case(words,1+n%16,regs)
# Every load/store form, alignment, zero destination and base/data alias.
for op in [0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2b,0x22,0x26,0x2a,0x2e]:
 for k in range(4):
  for rt in [0,2,3]:
   regs=[0]*32;regs[2]=0x89abcdef;regs[3]=0x80180000+k
   words=[(op<<26)|(3<<21)|(rt<<16)]+[(9<<26)|(4<<21)|(4<<16)|1]*7
   run_case(words,8,regs)
# Taken/not-taken controls with real delay slots, including entering a
# block already inside a pending slot. HLE link calls remain scalar slots.
for op,rt in [(1,0),(1,1),(2,0),(3,0),(4,2),(5,2),(6,0),(7,0)]:
 for rsval in [0,1,0xffffffff,0x80000000]:
  for budget in [1,2,3,8]:
   regs=[0]*32;regs[1]=rsval;regs[2]=0
   w=(op<<26)|(1<<21)|(rt<<16)|3
   if op in [2,3]:w=(op<<26)|((ibase+16)>>2)
   run_case([w,(9<<26)|(5<<16)|17]+[(9<<26)|(6<<21)|(6<<16)|1]*12,budget,regs)
for fn in [8,9]:
 regs=[0]*32;regs[1]=ibase+16
 run_case([(1<<21)|(31<<11)|fn,(9<<26)|(5<<16)|19]+[(9<<26)|(6<<21)|(6<<16)|1]*12,8,regs)
for rt in [0x10,0x11]:
 regs=[0]*32;regs[1]=0xffffffff
 run_case([(1<<26)|(1<<21)|(rt<<16)|3,(9<<26)|(5<<16)|21]+[(9<<26)|(6<<21)|(6<<16)|1]*12,8,regs)
# COP0 transfers, RFE, real SYSCALL and reserved instruction exceptions.
for w in [(0x10<<26)|(2<<16)|(12<<11),(0x10<<26)|(4<<21)|(2<<16)|(12<<11),0x42000010,12,0xfc000000]:
 regs=[0]*32;regs[2]=0x400000
 run_case([w]+[(9<<26)|(4<<21)|(4<<16)|1]*7,1,regs)
# Idle tick and pending external IRQ, repeated budgets across boundaries.
for idle in [0,1]:
 for boundary in range(1,9):
  def extra():
   word(ist+io['cop0']+12*4,0x401);u.mem_write(ist+io['idle'],bytes([idle]))
   call('iop_mem_write32',ist,0x1f801074,1);call('iop_mem_write32',ist,0x1f801078,1)
   # Vblank tick 0 raises IRQ source zero exactly at this position.
   u.mem_write(ist+io['sched_ticks'],struct.pack('>Q',615186-boundary))
  run_case([(9<<26)|(4<<21)|(4<<16)|1]*16,8,None,extra)
# Self-modifying next word: live guard must execute the new scalar body
# in the same tick and discard stale block only after returning from PPC.
regs=[0]*32;replacement=(9<<26)|(2<<16)|77;regs[3]=0x80100004;regs[5]=replacement
run_case([(0x2b<<26)|(3<<21)|(5<<16),(9<<26)|(2<<16)|1]+[(9<<26)|(4<<21)|(4<<16)|1]*14,8,regs)
# Source rewrite at entry, PC aliases and budget-zero no effects.
setup([(9<<26)|(2<<16)|1]*16);before=snapshot();assert call('iop_core_step_n',0)==0 and snapshot()==before
for pc in [ibase,ibase|0x80000000,ibase|0xa0000000]:
 setup([(9<<26)|(2<<21)|(2<<16)|1]*16);word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4)
 assert call('iop_core_step_n',8)==8;assert int.from_bytes(u.mem_read(ist+8,4),'big')==8
 if enabled:assert call('iop_jit_get_block_cache_size')>0
# Allocation failure is a clean scalar fallback, not a lost tick.
setup([(9<<26)|(2<<21)|(2<<16)|1]*16);fail_alloc=True
assert call('iop_core_step_n',8)==8 and int.from_bytes(u.mem_read(ist+8,4),'big')==8
fail_alloc=False
print('PASS',cases,'linked PPC IOP block/scalar programs: ALU, HI/LO, all memory forms, controls, exceptions, IRQ/idle, source mutation and allocation failure')
print('IOP_BLOCK_SIGNATURE',sig.hexdigest())
# HLE BIOS entry is handled before native instruction fetch (including a
# halt); budgets count the consumed scheduler tick exactly once.
for fn in [0x06,0xffffffff]:
 regs=[0]*32;regs[9]=fn;regs[31]=ibase;regs[4]=5
 def extra():word(ist+io['pc'],0xa0);word(ist+io['next_pc'],0xa4)
 run_case([(9<<26)|(2<<16)|1]*16,8,regs,extra)
# RFE stack-bit combinations and link operand aliases.
for status in [0,1,4,0xf,0x3f,0x40003c,0xffffffff]:
 def extra():word(ist+io['cop0']+12*4,status);u.mem_write(ist+io['exception_pending'],b'\1')
 run_case([0x42000010]+[(9<<26)|(4<<21)|(4<<16)|1]*15,1,None,extra)
for rt in [0x10,0x11]:
 for rs in [0,1,31]:
  regs=[0]*32;regs[1]=0xffffffff;regs[31]=0xffffffff
  run_case([(1<<26)|(rs<<21)|(rt<<16)|3,(9<<26)|(5<<16)|21]+[(9<<26)|(6<<21)|(6<<16)|1]*12,8,regs)
for rs,rd in [(1,1),(1,0),(0,31),(31,31)]:
 regs=[0]*32;regs[rs]=ibase+16 if rs else 0
 run_case([(rs<<21)|(rd<<11)|9,(9<<26)|(5<<16)|19]+[(9<<26)|(6<<21)|(6<<16)|1]*12,2,regs)
# Actual EE/IOP scheduler integration: observe EE count at each IOP
# pre-fetch boundary, proving that batching preserves 8:1 visibility.
observed=[]
def observe(uc,address,size,user):
 if address==syms['iop_prepare']:observed.append(executed())
h=u.hook_add(UC_HOOK_CODE,observe)
u.mem_map(0x90000000,0x800000);heap_limit=0x90800000
for budget in [1,7,8,9,17,257]:
 extension_setup();setup([(9<<26)|(2<<21)|(2<<16)|1]*300)
 heap=0x90000000
 word(state+off['pc'],base|0x80000000);word(state+off['next_pc'],(base|0x80000000)+4)
 for n in range(8*budget+16):u.mem_write(ram+base+n*4,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
 observed.clear();assert call('system_run_interleaved',0,budget)==0
 assert observed==[8*n for n in range(1,budget+1)],(budget,observed)
 assert executed()==8*budget
 assert int.from_bytes(u.mem_read(ist+io['sched_ticks'],8),'big')==budget
u.hook_del(h)
print('PASS exact EE8/IOP1 ordering across block boundaries and profiling intervals')
print('IOP_BLOCK_FINAL_SIGNATURE',sig.hexdigest(),'programs',cases)
if enabled:
 # Warm entry source changes and active-code reset pinning.
 setup([(9<<26)|(2<<16)|1]*16);assert call('iop_core_step_n',8)==8
 assert call('iop_jit_get_block_cache_size')>0
 size_before=call('iop_jit_get_block_cache_size');free_before=frees
 word(syms['block_active'],1);call('iop_jit_reset_for_test')
 assert call('iop_jit_get_block_cache_size')==size_before and frees==free_before
 word(syms['block_active'],0)
 word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
 u.mem_write(iram+ibase,struct.pack('<I',(9<<26)|(2<<16)|77))
 assert call('iop_core_step_n',1)==1 and int.from_bytes(u.mem_read(ist+8,4),'big')==77
 call('iop_jit_reset_for_test');assert call('iop_jit_get_block_cache_size')==0
 # Live retail ROM classifier hook must run before native JAL emission.
 # Provide synthetic ROM words, not a copyrighted BIOS dump.
 regs=[0]*32;regs[4]=0xbfc12340
 setup([0]*16,regs);bios_ptr=0x81708000;rom=iram+0x300000
 u.mem_write(bios_ptr,struct.pack('>II',rom,0x100000)+bytes(104));word(ist+io['bios'],bios_ptr)
 pc=0xbfc4a300;jump=(3<<26)|((0xbfc4a600>>2)&0x3ffffff)
 u.mem_write(rom+0x4a300,struct.pack('<II',jump,(9<<26)|(2<<16)|11))
 word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4);word(ist+io['cop0']+15*4,0x1f)
 assert call('iop_core_step_n',1)==1
 assert int.from_bytes(u.mem_read(ist+io['devtable_pending_image'],4),'big')==0xbfc12340
 assert int.from_bytes(u.mem_read(ist+31*4,4),'big')==pc+8
 assert call('iop_core_native_call_safe',ist,0xbfc4a39c,(1<<21)|(31<<11)|9)==0
 print('PASS warm source replacement, active-code reset pinning and retail ROM HLE guards')
