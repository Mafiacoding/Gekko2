"""GPL-3.0-or-later. Linked-PPC differential for ELF-bound PS2Recomp AOT blocks.

Runs the real Wii ELF (built by tools/build_wii_private_aot.sh) under the
Unicorn PowerPC model: for every function in functions.private.json the
interpreter (optimizations off) and the generated AOT block (on) execute the
same guest words from the same randomised 128-bit GPR state and the complete
ee_state_t plus the guest code window must be byte-identical. Modelled
hardware only: this is not a Wii or Dolphin run.

  verify_wii_ps2recomp_aot.py --nm powerpc-eabi-nm ELF --functions functions.private.json
"""
import sys,json,random,hashlib
from pathlib import Path
# Reuse the audited linked-PPC fixture (state layout, allocator/cache mocks).
argv=sys.argv[1:];fx_index=argv.index('--functions');functions_path=argv[fx_index+1];del argv[fx_index:fx_index+2]
sys.argv=[sys.argv[0]]+argv
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
functions=json.loads(Path(functions_path).read_text())['blocks']
assert 'ee_aot_block_count' in syms and 'retired_count' in syms
mask_addr=syms['gekko2_optimization_mask']
def set_mask(value):word(mask_addr,value)
def retired_native():return int.from_bytes(bytes(u.mem_read(syms['retired_count'],8)),'big')
DATA=0x300000;DATA_SIZE=0x20000
def fill_data(seed):
 r=random.Random(seed);blob=b''.join(struct.pack('<I',0x80300200+(r.getrandbits(8)&0xfc)) for _ in range(DATA_SIZE//4))
 u.mem_write(ram+DATA,blob)
BASES={0:(0x80300100,0x80310008),1:(0x10003000,0x10003000),2:(0x00700000,0x00700000)}  # valid, MMIO, unmapped KUSEG
def is_memory(words):return any(w>>26 in (0x23,0x2b) for w in words)
def setup_function(f,words,gprs,ra):
 extension_setup()
 start=f['pc']
 # Map the function's page; page 0x100000 is not used by the base fixture.
 u.mem_write(state+tlb,struct.pack('>4I',0,(start&~0x1fff)|7,((start>>12)<<6)|6,(((start>>12)+1)<<6)|6))
 for i,w in enumerate(words):u.mem_write(ram+start+4*i,struct.pack('<I',w))
 u.mem_write(ram+start+4*len(words),bytes(16))
 for r in range(32):u.mem_write(state+16*r,struct.pack('>QQ',gprs[r][0],gprs[r][1]))
 word(state+off['pc'],start);word(state+off['next_pc'],start+4)
 u.mem_write(state+off['instructions_executed'],bytes(8))
 return start
def run_mode(f,words,gprs,budget,aot,seed=0,tolerant=False):
 set_mask(9 if aot else 0)
 start=setup_function(f,words,gprs,None)
 if is_memory(words):fill_data(seed)
 before=retired_native();done=0;count=len(words);gots=[]
 while done<count:
  n=min(budget,count-done);got=call('ee_core_step_n',n);gots.append(got)
  if not tolerant:assert got==n,(f['name'],budget,n,got)
  done+=n if tolerant else got
  if tolerant:break
 snapshot=(bytes(u.mem_read(state,size)),bytes(u.mem_read(ram+start,4*count+16)),bytes(u.mem_read(ram+DATA,DATA_SIZE)),gots)
 return snapshot,retired_native()-before
rng=random.Random(0xa15)
patterns=[lambda r:0,lambda r:(1<<64)-1,lambda r:0x8000000080000000,lambda r:0x7fffffff,lambda r:rng.getrandbits(64)]
sig=hashlib.sha256();cases=0;native_total=0
for f in functions:
 words=f['words'];count=len(words)
 for trial in range(12):
  gprs=[(0,0)]+[(patterns[trial%5](r),rng.getrandbits(64)) for r in range(1,32)]
  if is_memory(words):gprs[4]=(BASES[0][0],gprs[4][1]);gprs[5]=(BASES[0][1],gprs[5][1])
  if f.get('terminal')=='jr_ra':gprs[31]=(0x80000000|(rng.getrandbits(21)&~3),rng.getrandbits(64))
  for budget in sorted({count,1,2,max(1,count-1)}):
   old,_=run_mode(f,words,gprs,budget,False,trial)
   new,native=run_mode(f,words,gprs,budget,True,trial)
   assert native>0,('AOT block did not execute natively',f['name'],budget)
   assert old==new,('PPC interpreter/AOT state mismatch',f['name'],trial,budget)
   native_total+=native;sig.update(new[0]);cases+=1
 # Device and faulting addresses defer to the interpreter with identical results.
 if is_memory(words):
  for mode in (1,2):
   g=list(gprs);g[4]=(BASES[mode][0],g[4][1]);g[5]=(BASES[mode][1],g[5][1])
   old,_=run_mode(f,words,g,count,False,7,True);new,_=run_mode(f,words,g,count,True,7,True)
   assert old==new,('PPC fault/device mismatch',f['name'],mode);cases+=1
   if mode==2:assert (int.from_bytes(new[0][off['cop0']+13*4:off['cop0']+13*4+4],'big')>>2)&31,('unmapped KUSEG access did not fault',f['name'])
 # Live-source guard on PPC: a changed word must fall back to the interpreter.
 mutated=list(words);mutated[count//2]=0x24090007
 set_mask(0);start=setup_function(f,mutated,gprs,None);fill_data(3);call('ee_core_step_n',count);ref=bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+DATA,DATA_SIZE))
 set_mask(9);setup_function(f,mutated,gprs,None);fill_data(3);call('ee_core_step_n',count)
 assert bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+DATA,DATA_SIZE))==ref,('stale AOT executed',f['name']);cases+=1
print('PS2RECOMP_AOT_PPC_SIGNATURE '+sig.hexdigest())
print(f'PASS {cases} PPC interpreter/AOT comparisons over {len(functions)} ELF-bound functions ({native_total} natively retired instructions), under Unicorn PPC model')
