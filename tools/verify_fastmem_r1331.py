"""RAM translation cache permission/mapping tests on the actual linked PPC.
Only allocator and platform cache maintenance are mocked.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
word(mask_addr,initial|512)
setup()
iw=(0x23<<26)|(3<<21)|(2<<16)
def resolve(addr=0x300000):
 u.mem_write(state+48,struct.pack('>QQ',addr,0))
 return call('ee_core_block_memory_resolve',state,iw)
def stat(n):
 hi=call('ee_fastmem_stat',n);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
hit=stat(0);assert resolve()==0x300001 and resolve()==0x300001 and stat(0)==hit+1
# A changed selected entry is validated even before explicit invalidation.
word(state+tlb+24,(0x302<<6)|6);assert resolve()==0x302001
word(state+tlb+24,(0x302<<6)|4);assert resolve()==0 # V revoked
word(state+tlb+24,(0x302<<6)|2);assert resolve()==0x302001
iw=(0x2b<<26)|(3<<21)|(2<<16);assert resolve()==0 # D revoked
word(state+tlb+24,(0x302<<6)|6);assert resolve()==0x302001
word(state+off['cop0']+10*4,8);assert resolve()==0 # ASID changed
word(state+off['cop0']+10*4,7);assert resolve()==0x302001
word(state+off['ram_size'],0x302000);assert resolve()==0 # live backing bounds
word(state+off['ram_size'],0x400000)
# Cache is positive-only: device translations never become direct RAM.
word(state+tlb+24,(0x10000<<6)|6);assert resolve()==0
word(state+tlb+24,(0x302<<6)|6);assert resolve()==0x302001
# Earlier overlapping entries take priority after architectural notification.
u.mem_write(state+tlb,struct.pack('>4I',0,0x300007,(0x303<<6)|6,(0x304<<6)|6))
call('ee_jit_notify_mapping_change');assert resolve()==0x303001
assert resolve(0x70000000)==0 # scratchpad remains its dedicated path
# Invalid code mappings never expose an uninitialized physical address.
setup();word(state+tlb+8,(0x200<<6)|0)
u.mem_write(ram+base,struct.pack('<I',(9<<26)|(2<<16)|77))
assert call('ee_core_block_peek',state,base,0x81750000)==0
word(mask_addr,initial)
print('PASS RAM page hits, V/D/ASID changes, bounds, MMIO/scratch exclusion, TLB priority and invalid code fetch')
# Run complete mapped execution with cache off and on; compare whole guest digests.
import contextlib,io
body=Path(__file__).with_name('verify_ee_mapped_blocks_r1305.py').read_text();body=body[body.index('mapped_digest='):]
ops=[0x20,0x24,0x21,0x25,0x23,0x28,0x29,0x2b];digests=[]
for flag in [0,512]:
 word(mask_addr,(initial&~512)|flag)
 captured=io.StringIO()
 with contextlib.redirect_stdout(captured):exec(compile(body,'mapped_fastmem_oracles','exec'))
 output=captured.getvalue();print('FASTMEM',bool(flag),output.strip())
 digests.append(mapped_digest.hexdigest())
assert digests[0]==digests[1]
word(mask_addr,initial)
print('PASS complete mapped guest state parity with fastmem OFF / ON')
costs={}
for index in [1,47]:
 samples=[]
 for flag in [0,512]:
  word(mask_addr,(initial&~512)|flag);setup()
  if index!=1:
   u.mem_write(state+tlb+16,bytes(16))
   u.mem_write(state+tlb+index*16,struct.pack('>4I',0,0x300007,(0x300<<6)|6,(0x301<<6)|6))
  u.mem_write(state+48,struct.pack('>QQ',0x300000,0));u.mem_write(ram+0x300000,struct.pack('<I',77))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x23<<26)|(3<<21)|(2<<16)))
  assert call('ee_core_step_n',8)==8
  word(state+off['pc'],base);word(state+off['next_pc'],base+4)
  total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False
  assert reg2()==77;samples.append(total[0])
 costs[index]={'OFF':samples[0],'ON':samples[1]}
print('FASTMEM_TLB_COST',json.dumps(costs),'mocked PPC instruction counts, not Wii timings')
assert costs[47]['ON']<costs[47]['OFF']
word(mask_addr,initial)
