"""Execute the real precise EE block cache and native PPC code with real retirement."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_mapped_r1293.py').read_text(),'measure_ee_mapped_r1293.py','exec'))
from unicorn import UC_HOOK_CODE
ram=0x91000000;u.mem_map(ram,0x400000);word(state+off['ram'],ram);word(state+off['ram_size'],0x400000)
base=0x200000
# Existing virtual TLB pair now maps to actual main-RAM executable pages.
u.mem_write(state+tlb,struct.pack('>4I',0,base|7,(0x200<<6)|2,(0x201<<6)|2))
enabled='Interpreter'not in Path(a.elf).name

def setup(pc=base,budget=8):
 global heap
 reset_frontend();
 if 'ee_jit_reset_stats_for_test'in syms:
  call('ee_jit_reset_stats_for_test')
  heap=0x81600000 # mock allocator reuses buffers after the real frontend frees them
 call('ee_timers_init');call('ee_intc_init');call('dma_init');call('gif_init')
 u.mem_write(state,bytes(544));word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
 word(state+off['cop0']+13*4,0);word(state+off['cop0']+9*4,100);word(state+off['cop0']+11*4,0);word(state+off['cop0']+12*4,0);word(state+off['cop0']+10*4,7)
 u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
 # idle offset is derived from ABI, not guessed.
 u.mem_write(state+idle_offset,b'\0')
 for n in range(16):u.mem_write(ram+pc+4*n,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))

# Derive the two additional real target ABI fields.
with tempfile.TemporaryDirectory()as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={offsetof(ee_state_t,idle),offsetof(ee_state_t,instructions_executed)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 idle_offset,instructions_offset=struct.unpack('>2I',raw.read_bytes())

def reg2():return int.from_bytes(bytes(u.mem_read(state+32,8)),'big')
def executed():return int.from_bytes(bytes(u.mem_read(state+instructions_offset,8)),'big')
for budget in [2,3,5,8]:
 setup();before=executed();assert call('ee_core_step_n',budget)==budget
 assert reg2()==budget and executed()==before+budget
 assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==100+budget
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+budget*4
 assert bytes(u.mem_read(state,16))==bytes(16)
# Expanded nontrapping integer/HI-LO policy; compare the FULL 544-byte state.
extra=[('SLTI',(10<<26)|(3<<21)|(2<<16)|0x8000,0),('SLTIU',(11<<26)|(3<<21)|(2<<16)|0xffff,0),('DADDIU',(25<<26)|(3<<21)|(2<<16)|0xffff,0)]
extra += [(f'FN{f:02x}',(3<<21)|(2<<16)|(4<<11)|(17<<6)|f,0)for f in [10,11,16,17,18,19,20,22,23,24,25,26,27,56,58,59,60,62,63]]
cases += extra
# Complete state comparison across every currently eligible ALU encoding.
import hashlib,random
rng=random.Random(1294);alu_hash=hashlib.sha256()
for name,iw,expected in cases:
 setup()
 initial=bytearray(rng.randbytes(544));initial[:16]=bytes(16);u.mem_write(state,bytes(initial))
 for j in range(8):u.mem_write(ram+base+4*j,struct.pack('<I',iw))
 before=executed();assert call('ee_core_step_n',8)==8 and executed()==before+8
 alu_hash.update(bytes(u.mem_read(state,544)))
# Division zero/overflow and sign-extension cases, including HI/LO lanes.
for fn in [24,25,26,27]:
 for lhs in [0,1,0x7fffffff,0x80000000,0xffffffff]:
  for rhs in [0,1,0xffffffff,0x80000000]:
   setup();u.mem_write(state+3*16,struct.pack('>QQ',lhs,0));u.mem_write(state+2*16,struct.pack('>QQ',rhs,0))
   iw=(3<<21)|(2<<16)|(4<<11)|fn
   for j in range(8):u.mem_write(ram+base+4*j,struct.pack('<I',iw))
   assert call('ee_core_step_n',8)==8
   alu_hash.update(bytes(u.mem_read(state,544)))
print('EE_BLOCK_ALU_SIGNATURE '+alu_hash.hexdigest())
# Baseline/interpreter comparison uses exactly the same eligible mapped code.
def warm_bench():
 for length in [2,3,4]:
  setup();assert call('ee_core_step_n',length)==length
  short=[]
  for k in range(3):
   word(state+off['pc'],base);word(state+off['next_pc'],base+4);u.mem_write(state+32,bytes(16))
   total[0]=0;active[0]=True;assert call('ee_core_step_n',length)==length;active[0]=False;short.append(total[0]);assert reg2()==length
  print('EE_SHORT_BLOCK_BENCH '+json.dumps({'length':length,'PPC':short,'scope':'full retirement, mocked allocation/cache; no Wii FPS claim'}))
 setup();assert call('ee_core_step_n',8)==8
 samples=[]
 for k in range(4):
  word(state+off['pc'],base);word(state+off['next_pc'],base+4);u.mem_write(state+32,bytes(16))
  total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;samples.append(total[0]);assert reg2()==8
 print('EE_PRECISE_BLOCK_BENCH '+json.dumps({'elf':Path(a.elf).name,'warm_full_retirement_PPC':samples,'scope':'eight mapped ADDIU; real native code/CPU retirement, mocked platform allocation/cache; no Wii FPS claim'}))
if 'ee_jit_try_execute_block'not in syms:
 warm_bench();print('PASS R1293 mapped baseline budget/Count/ALU parity');raise SystemExit(0)
if not enabled:
 assert call('ee_jit_try_execute_block',state,8)==0
 warm_bench();print('PASS interpreter budget/retirement parity and disabled block allocation');raise SystemExit(0)
# A code patch between retired instructions stops before stale code executes.
setup();before=executed();seen=[0]
def patch(uc,address,size,user):
 if address==syms['ee_core_block_prepare'] or address==syms.get('ee_core_block_advance'):
  seen[0]+=1
  if uc.reg_read(UC_PPC_REG_4)==base+8:uc.mem_write(ram+base+8,struct.pack('<I',(9<<26)|(2<<16)|77))
h=u.hook_add(UC_HOOK_CODE,patch)
assert call('ee_jit_try_execute_block',state,8)==2
u.hook_del(h);assert reg2()==2 and executed()==before+2
assert call('ee_core_step_n',1)==1 and reg2()==77
# A warm cached first word cannot execute after replacement.
setup();assert call('ee_jit_try_execute_block',state,8)==8
word(state+off['pc'],base);word(state+off['next_pc'],base+4)
u.mem_write(ram+base,struct.pack('<I',(9<<26)|(2<<16)|77));before=executed()
assert call('ee_jit_try_execute_block',state,8)==0 and executed()==before and reg2()==8
assert call('ee_core_step_n',8)==8 and reg2()==84
# Actual Count/Compare interrupt must end the block at the second boundary.
setup();word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,2);word(state+off['cop0']+12*4,0x18001)
before=executed();n=call('ee_jit_try_execute_block',state,8)
assert n==2 and reg2()==2 and executed()==before+2,(n,reg2(),executed(),before)
assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80000200
assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+8
# Check every possible interrupt boundary, including the final instruction.
for boundary in range(1,9):
 setup();word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,boundary);word(state+off['cop0']+12*4,0x18001)
 before=executed();assert call('ee_jit_try_execute_block',state,8)==boundary
 assert reg2()==boundary and executed()==before+boundary
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80000200
 assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+boundary*4
# A mapping replacement by retirement work is visible at the next prepare.
setup();seen=[0]
def remap(uc,address,size,user):
 if address==syms['ee_core_block_prepare'] or address==syms.get('ee_core_block_advance'):
  seen[0]+=1
  if uc.reg_read(UC_PPC_REG_4)==base+8:
   word(state+tlb+8,(0x204<<6)|2)
   u.mem_write(ram+0x204008,struct.pack('<I',(9<<26)|(2<<16)|99))
h=u.hook_add(UC_HOOK_CODE,remap)
assert call('ee_jit_try_execute_block',state,8)==2 and reg2()==2
u.hook_del(h);assert call('ee_core_step_n',1)==1 and reg2()==99
word(state+tlb+8,(0x200<<6)|2)
# Native code and callbacks preserve the complete PPC callee-saved GPR set.
setup();sentinels={r:0x12940000+r for r in range(14,32)}
for r,value in sentinels.items():u.reg_write(UC_PPC_REG_3+r-3,value)
assert call('ee_jit_try_execute_block',state,8)==8
for r,value in sentinels.items():assert u.reg_read(UC_PPC_REG_3+r-3)==value,(r,value)
# Delay-slot, idle and halted entries decline without retiring.
for field in [off['branch_pending'],idle_offset,off['halted']]:
 setup();u.mem_write(state+field,b'\1');before=executed()
 assert call('ee_jit_try_execute_block',state,8)==0 and reg2()==0 and executed()==before
# Source-page boundary truncates formation; cache collision remains exact tagged.
setup(base+0xff8);assert call('ee_jit_try_execute_block',state,8)==2 and reg2()==2
setup();assert call('ee_jit_try_execute_block',state,8)==8
word(state+off['pc'],base+0x400);word(state+off['next_pc'],base+0x404)
for n in range(8):u.mem_write(ram+base+0x400+4*n,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|2))
assert call('ee_jit_try_execute_block',state,8)==8 and reg2()==24
word(state+off['pc'],base);word(state+off['next_pc'],base+4)
assert call('ee_jit_try_execute_block',state,8)==8 and reg2()==32
# Allocation failure may retry and must not advance the guest.
setup();fail_alloc=True;before=executed()
assert call('ee_jit_try_execute_block',state,8)==0 and executed()==before
fail_alloc=False;assert call('ee_jit_try_execute_block',state,8)==8 and reg2()==8
# Compare warm complete retirement costs with the original scalar path.
warm_bench()
print('PASS precise native EE blocks: budgets, Count/PC, source SMC exit, actual IRQ exit/EPC, delay/idle/halt, page boundary, cache collision, ownership and allocation retry')
