"""Small linked-PPC fixture for focused internal differential tests.
The broader historical suites remain separate regression gates. Platform
allocation/cache maintenance and libc memset are mocked; CPU/retirement execute
ELF code. memset uses exact byte semantics to avoid emulating cache clearing.
"""
from pathlib import Path
import struct,subprocess,tempfile,json,hashlib
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import *
loader=Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0]
loader=loader.replace('count=10000000','count=(50000000 if name=="system_run_interleaved" else 10000000)')
exec(compile(loader,'verify_ppc_gs_memory.py','exec'))
root=Path(__file__).resolve().parents[1]
cc=str(Path(a.nm).with_name('powerpc-eabi-gcc'));oc=str(Path(a.nm).with_name('powerpc-eabi-objcopy'))
fields=['pc','next_pc','cop0','ram','ram_size','halted','branch_pending','idle','instructions_executed','tlb','fpr','exc_this_pc','vu0_vf','gpr_generation']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={sizeof(ee_state_t),'+','.join('offsetof(ee_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 size,*offsets=struct.unpack('>'+str(1+len(fields))+'I',raw.read_bytes());off=dict(zip(fields,offsets))
 # Derive private slot ABI from the actual source, never a historical stride.
 src.write_text('#include <stddef.h>\n#include "core/recompiler/ee_jit.c"\nconst unsigned layout[] __attribute__((section(".test_layout")))={sizeof(ee_precise_slot),offsetof(ee_precise_slot,fn),offsetof(ee_precise_slot,count)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-I'+str(root/'source'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.test_layout',str(obj),str(raw)],check=True)
 precise_slot_size,precise_fn_offset,precise_count_offset=struct.unpack('>3I',raw.read_bytes())
# Read the linked array size rather than assuming a historical cache index.
cache_bytes=None
for row in subprocess.check_output([a.nm,'-S',a.elf],text=True).splitlines():
 parts=row.split()
 if len(parts)==4 and parts[3]=='precise_cache':cache_bytes=int(parts[1],16)
assert cache_bytes and cache_bytes%precise_slot_size==0
precise_cache_entries=cache_bytes//precise_slot_size
def precise_native_for_pc(pc,count=None):
 for index in range(precise_cache_entries):
  slot=syms['precise_cache']+index*precise_slot_size
  if int.from_bytes(u.mem_read(slot,4),'big')!=pc:continue
  if count is not None and int.from_bytes(u.mem_read(slot+precise_count_offset,4),'big')!=count:continue
  fn=int.from_bytes(u.mem_read(slot+precise_fn_offset,4),'big')
  if fn:return fn
 return 0
state=call('ee_core_get_state');ram=0x91000000;base=0x200000
u.mem_map(ram,0x400000);u.reg_write(UC_PPC_REG_MSR,0x2000)
def word(address,value):u.mem_write(address,struct.pack('>I',value))
heap=0x81600000;heap_limit=0x81700000;fail_alloc=False;allocs=frees=0
platforms={syms[n]:n for n in ['memalign','free','DCFlushRange','ICInvalidateRange','memset']}
def platform(uc,address,size,user):
 global heap,allocs,frees
 name=platforms.get(address)
 if name is None:return
 if name=='memset':
  dest=uc.reg_read(UC_PPC_REG_3);value=uc.reg_read(UC_PPC_REG_4)&255;n=uc.reg_read(UC_PPC_REG_5)
  if n:uc.mem_write(dest,bytes([value])*n)
  uc.reg_write(UC_PPC_REG_3,dest)
 elif name=='memalign':
  allocs+=1
  if fail_alloc:ret=0
  else:
   align=uc.reg_read(UC_PPC_REG_3);n=uc.reg_read(UC_PPC_REG_4)
   heap=(heap+align-1)&~(align-1);ret=heap;heap+=n;assert heap<heap_limit
  uc.reg_write(UC_PPC_REG_3,ret)
 elif name=='free':frees+=1
 elif name=='ICInvalidateRange':
  start=uc.reg_read(UC_PPC_REG_3);n=uc.reg_read(UC_PPC_REG_4);uc.ctl_remove_cache(start,start+n)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,platform)
active=[False];total=[0]
def count_hook(uc,address,size,user):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,count_hook)
enabled='Interpreter' not in Path(a.elf).name
if 'gekko2_optimization_mask' in syms:
 mask=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
 enabled=bool(mask&1 and mask&8)
fused=enabled and 'ee_core_block_prepare_delay' in syms
fpr=off['fpr'];exc_offset=off['exc_this_pc'];tlb=off['tlb'];vf=off['vu0_vf']
def executed():return int.from_bytes(bytes(u.mem_read(state+off['instructions_executed'],8)),'big')
def reg2():return int.from_bytes(bytes(u.mem_read(state+32,8)),'big')
def extension_setup():
 global heap
 if 'ee_jit_reset_stats_for_test' in syms:call('ee_jit_reset_stats_for_test')
 if 'ee_fastmem_invalidate' in syms:call('ee_fastmem_invalidate')
 heap=0x81600000
 for name in ['ee_timers_init','ee_intc_init','dma_init','gif_init']:call(name)
 u.mem_write(state,bytes(size));word(state+off['ram'],ram);word(state+off['ram_size'],0x400000)
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 word(state+off['cop0']+9*4,100);word(state+off['cop0']+10*4,7)
 u.mem_write(state+tlb,struct.pack('>4I',0,base|7,(0x200<<6)|2,(0x201<<6)|2))
 u.mem_write(state+tlb+16,struct.pack('>4I',0,0x300007,(0x300<<6)|6,(0x301<<6)|6))
 for n in range(16):u.mem_write(ram+base+4*n,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
setup=extension_setup
