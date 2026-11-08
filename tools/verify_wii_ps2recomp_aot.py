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
image_path=None
if '--image' in argv:
 image_index=argv.index('--image');image_path=argv[image_index+1];del argv[image_index:image_index+2]
sys.argv=[sys.argv[0]]+argv
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
functions=json.loads(Path(functions_path).read_text())['blocks']
image=json.loads(Path(image_path).read_text()) if image_path else []  # all guest code, incl. callees left to the interpreter
assert 'ee_aot_block_count' in syms and 'retired_count' in syms
mask_addr=syms['gekko2_optimization_mask']
def set_mask(value):word(mask_addr,value)
def retired_native():return int.from_bytes(bytes(u.mem_read(syms['retired_count'],8)),'big')
DATA=0x300000;DATA_SIZE=0x20000
def fill_data(seed):
 r=random.Random(seed);blob=b''.join(struct.pack('<I',0x80300200+(r.getrandbits(8)&0xfc)) for _ in range(DATA_SIZE//4))
 u.mem_write(ram+DATA,blob)
BASES={0:(0x80300100,0x80310008,0x80320010),1:(0x10003000,)*3,2:(0x00700000,)*3,  # valid, MMIO, unmapped KUSEG
       3:(0x80300101,0x80310009,0x80320011),4:(0x00400100,0x00400200,0x00400300)}  # misaligned, write-protected page
MEM_OPS=(0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2b)
def is_memory(words):return any(w>>26 in MEM_OPS for w in words)
def has_store(words):return any(w>>26 in (0x28,0x29,0x2b) for w in words)
def setup_function(f,words,gprs,ra):
 extension_setup()
 start=f['pc']
 # Map the function's page; page 0x100000 is not used by the base fixture.
 u.mem_write(state+tlb,struct.pack('>4I',0,(start&~0x1fff)|7,((start>>12)<<6)|6,(((start>>12)+1)<<6)|6))
 u.mem_write(state+tlb+32,struct.pack('>4I',0,0x400000|7,(0x340<<6)|2,(0x341<<6)|2))  # V, not D (phys inside the fixture's 4 MiB RAM): stores raise TLB Modified
 for im in image:
  for i,w in enumerate(im['words']):u.mem_write(ram+im['pc']+4*i,struct.pack('<I',w))
 for i,w in enumerate(words):u.mem_write(ram+start+4*i,struct.pack('<I',w))
 u.mem_write(ram+start+4*len(words),bytes(16))
 for r in range(32):u.mem_write(state+16*r,struct.pack('>QQ',gprs[r][0],gprs[r][1]))
 word(state+off['pc'],start);word(state+off['next_pc'],start+4)
 u.mem_write(state+off['instructions_executed'],bytes(8))
 return start
def exit_pc(f,words,gprs):return gprs[31][0]&0xffffffff if f.get('terminal')=='jr_ra' else f['pc']+4*len(words)
def probe_total(f,words,gprs,seed=0):
 # Interpreter single-steps until the function exits (loops make the count dynamic).
 set_mask(0);setup_function(f,words,gprs,None)
 if is_memory(words):fill_data(seed)
 target=exit_pc(f,words,gprs);n=0
 while int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')!=target:
  assert call('ee_core_step_n',1)==1 and n<200000,('function did not exit',f['name']);n+=1
 return n
def run_mode(f,words,gprs,budget,aot,seed=0,tolerant=False,total=None):
 set_mask(9 if aot else 0)
 start=setup_function(f,words,gprs,None)
 if is_memory(words):fill_data(seed)
 before=retired_native();done=0;count=len(words) if total is None else total;gots=[]
 while done<count:
  n=min(budget,count-done);got=call('ee_core_step_n',n);gots.append(got)
  if not tolerant:assert got==n,(f['name'],budget,n,got)
  done+=n if tolerant else got
  if tolerant:break
 snapshot=(bytes(u.mem_read(state,size)),bytes(u.mem_read(ram+start,4*count+16)),bytes(u.mem_read(ram+DATA,DATA_SIZE)),gots)
 return snapshot,retired_native()-before
rng=random.Random(0xa15)
patterns=[lambda r:0,lambda r:(1<<64)-1,lambda r:0x8000000080000000,lambda r:0x7fffffff,lambda r:rng.getrandbits(64)]
sig=hashlib.sha256();cases=0;native_total=0;dyn=0;mod_faults=0;align_faults=0
for f in functions:
 words=f['words'];count=len(words)
 for trial in range(12):
  gprs=[(0,0)]+[(patterns[trial%5](r),rng.getrandbits(64)) for r in range(1,32)]
  if f.get('loop'):gprs[8]=(1+(rng.getrandbits(3)),gprs[8][1])  # bounded trip count
  if is_memory(words):gprs[4]=(BASES[0][0],gprs[4][1]);gprs[5]=(BASES[0][1],gprs[5][1]);gprs[6]=(BASES[0][2],gprs[6][1]);gprs[29]=(BASES[0][2]+0x40,gprs[29][1])
  if f.get('terminal')=='jr_ra':gprs[31]=(0x80000000|(rng.getrandbits(21)&~3),rng.getrandbits(64))
  total=probe_total(f,words,gprs,trial);dyn=max(dyn,total)
  for budget in sorted({total,1,2,max(1,total-1)}):
   old,_=run_mode(f,words,gprs,budget,False,trial,False,total)
   new,native=run_mode(f,words,gprs,budget,True,trial,False,total)
   assert native>0,('AOT block did not execute natively',f['name'],budget)
   assert old==new,('PPC interpreter/AOT state mismatch',f['name'],trial,budget)
   native_total+=native;sig.update(new[0]);cases+=1
 # Device, faulting, misaligned and write-protected addresses: AOT defers to the
 # interpreter and both must end in the identical state.
 if is_memory(words):
  for mode in (1,2,3,4):
   g=list(gprs)
   for reg,addr in zip((4,5,6),BASES[mode]):g[reg]=(addr,g[reg][1])
   g[29]=((BASES[mode][2]+0x40)&0xffffffff,g[29][1])
   old,_=run_mode(f,words,g,count,False,7,True);new,_=run_mode(f,words,g,count,True,7,True)
   assert old==new,('PPC fault/device mismatch',f['name'],mode);cases+=1
   code=(int.from_bytes(new[0][off['cop0']+13*4:off['cop0']+13*4+4],'big')>>2)&31
   if mode==2:assert code,('unmapped KUSEG access did not fault',f['name'])
   if mode==4 and code==1:mod_faults+=1
   if mode==3 and code in (4,5):align_faults+=1
 # Live-source guard on PPC: a changed word must fall back to the interpreter.
 mutated=list(words);mutated[count//2]=0x24090007
 set_mask(0);start=setup_function(f,mutated,gprs,None);fill_data(3);call('ee_core_step_n',count);ref=bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+DATA,DATA_SIZE))
 set_mask(9);setup_function(f,mutated,gprs,None);fill_data(3);call('ee_core_step_n',count)
 assert bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+DATA,DATA_SIZE))==ref,('stale AOT executed',f['name']);cases+=1
assert mod_faults or not any(has_store(f['words']) for f in functions),'no TLB Modified fault observed'
print(f'max dynamic retirement per run: {dyn}')
print(f'observed: {mod_faults} TLB-Modified faults, {align_faults} address-error faults on misaligned access (PPC run)')
print('PS2RECOMP_AOT_PPC_SIGNATURE '+sig.hexdigest())
print(f'PASS {cases} PPC interpreter/AOT comparisons over {len(functions)} ELF-bound functions ({native_total} natively retired instructions), under Unicorn PPC model')
