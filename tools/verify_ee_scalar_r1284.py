"""Independent full-register EE oracle and full-retirement PPC costs."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_cache_r1267.py').read_text(),'verify_ppc_cache_r1267.py','exec'))
import random,json
from unicorn import UC_HOOK_CODE
active=[False];total=[0]
def count_scalar(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,count_scalar);u.ctl_remove_cache(0x80000000,0x81800000)
rng=random.Random(1284);mask=(1<<64)-1
names=['SLTI','SLTIU','MOVZ','MOVN','MFHI','MTHI','MFLO','MTLO'];results={};checks=0
for name in names:
 samples=[]
 for n in range(64):
  reset_frontend();rs=rng.choice([0,2,3]);rt=rng.choice([0,2,3]);rd=rng.choice([0,2,3]);imm=rng.choice([0,1,0x7fff,0x8000,0xffff]);signed=imm if imm<32768 else imm-65536
  vals=[rng.getrandbits(128) for _ in range(34)];vals[0]=0
  for reg in [2,3]:vals[reg]=(vals[reg]&~mask)|rng.choice([0,1,mask,1<<63,rng.getrandbits(64)])
  expected=vals[:];aa=vals[rs]&mask;bb=vals[rt]&mask
  if name in ('SLTI','SLTIU'):
   iw=((10 if name=='SLTI' else 11)<<26)|(rs<<21)|(rt<<16)|imm
   result=int((aa if aa<1<<63 else aa-(1<<64))<signed) if name=='SLTI' else int(aa<(signed&mask))
   dest=rt
  else:
   fn={'MOVZ':10,'MOVN':11,'MFHI':16,'MTHI':17,'MFLO':18,'MTLO':19}[name]
   iw=(rs<<21)|(rt<<16)|(rd<<11)|fn;dest=rd
   if name=='MOVZ':result=aa if bb==0 else vals[rd]&mask
   elif name=='MOVN':result=aa if bb!=0 else vals[rd]&mask
   elif name=='MFHI':result=vals[32]&mask
   elif name=='MFLO':result=vals[33]&mask
   else:dest=32 if name=='MTHI' else 33;result=aa
  if dest:expected[dest]=(expected[dest]&~mask)|result
  blob=b''.join((v&mask).to_bytes(8,'big')+(v>>64).to_bytes(8,'big') for v in vals)
  u.mem_write(state,blob)
  word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
  word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
  before=int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')
  guest(0x80006000,iw);total[0]=0;active[0]=True;retired=call('ee_core_step_n',1);active[0]=False
  want=b''.join((v&mask).to_bytes(8,'big')+(v>>64).to_bytes(8,'big') for v in expected)
  assert retired==1 and bytes(u.mem_read(state,544))==want,(name,n,rs,rt,rd)
  assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==(before+1)&0xffffffff
  assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006004
  checks+=1
 # fixed warm eight-retirement benchmark, separate from randomized oracle.
 reset_frontend()
 iw=((10 if name=='SLTI' else 11)<<26)|(3<<21)|(2<<16)|0xffff if name in ('SLTI','SLTIU') else (3<<21)|(3<<16)|(2<<11)|{'MOVZ':10,'MOVN':11,'MFHI':16,'MTHI':17,'MFLO':18,'MTLO':19}[name]
 for k in range(8):guest(0x80006000+k*4,iw)
 for iteration in range(4):
  u.mem_write(state,bytes(544));u.mem_write(state+48,(1).to_bytes(8,'big')+bytes(8))
  word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
  word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
  total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;samples.append(total[0])
 results[name]={'cold':samples[0],'warm':samples[1:]}
print('PASS',checks,'EE scalar register-byte oracles: upper halves, zero/alias, signed64 predicates, HI/LO and Count/PC retirement')
print('EE_SCALAR_R1284_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Eight real retirements, PPC instruction counts under Unicorn; not Wii cycles/FPS.'}))
