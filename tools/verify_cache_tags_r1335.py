"""Warm dispatch cache-line accesses, not hardware cache misses/FPS.
The same script accepts R1334/R1335 ELFs for an owner/tag footprint comparison.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
from unicorn import UC_HOOK_MEM_READ
setup();mask_addr=syms['gekko2_optimization_mask'];word(mask_addr,int.from_bytes(u.mem_read(mask_addr,4),'big')|8192)
pcs=[base+i*32 for i in range(8)]
for pc in pcs:u.mem_write(ram+pc,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
for pc in pcs:
 word(state+off['pc'],pc);word(state+off['next_pc'],pc+4);assert call('ee_core_step_n',8)==8
ranges={'owners':(syms['precise_cache'],cache_bytes),'tags':(syms['precise_dispatch'],4096*32)}
reads={k:0 for k in ranges};lines={k:set() for k in ranges}
def read_hook(uc,access,address,size,value,user):
 for k,(start,n) in ranges.items():
  if start<=address<start+n:
   reads[k]+=1
   for line in range(address//32,(address+size-1)//32+1):lines[k].add(line)
u.hook_add(UC_HOOK_MEM_READ,read_hook)
for n in range(128):
 pc=pcs[n%8];word(state+off['pc'],pc);word(state+off['next_pc'],pc+4);assert call('ee_core_step_n',8)==8
assert reg2()==1088 and executed()==1088
print('CACHE_METADATA_READS',json.dumps({'reads':reads,'unique_lines':{k:len(v) for k,v in lines.items()}},sort_keys=True))
print('PASS same 128 warmed grants and guest result across tag/owner layouts')
