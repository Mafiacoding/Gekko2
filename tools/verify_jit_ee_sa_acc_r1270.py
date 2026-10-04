"""Generated PPC SA/funnel and multiply-add against independent integer models."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libsa_r1270.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libsa_r1270.so'));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x2000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
rng=random.Random(1270);mask64=(1<<64)-1;mask128=(1<<128)-1
signed=lambda x:x-(1<<32) if x&0x80000000 else x
sx=lambda x:signed(x&0xffffffff)&mask64
cases=0
for kind in range(9):
 for n in range(1600):
  rs=n%32;rt=rs if n%5==0 else rng.randrange(32);rd=[0,rs,rt,rng.randrange(32)][n%4]
  mem=bytearray(rng.randbytes(4096));regs=[rng.getrandbits(64) for k in range(68)];regs[:2]=[0,0]
  if n<500:
   if rs:regs[rs*2]=[0,1,0xffffffff,0x80000000,0x7fffffff][n%5]
   if rt:regs[rt*2]=[0,1,0xffffffff,0x80000000,0x7fffffff][(n//5)%5]
  struct.pack_into('>68Q',mem,0,*regs);sa=(rng.getrandbits(28)<<4)|(n%16);struct.pack_into('>I',mem,552,sa)
  expected=mem[:];a=regs[rs*2]&0xffffffff;b=regs[rt*2]&0xffffffff
  if kind==0:
   iw=(rd<<11)|0x28
   if rd:struct.pack_into('>Q',expected,rd*16,sa)
  elif kind==1:
   iw=(rs<<21)|0x29;struct.pack_into('>I',expected,552,a)
  elif kind<4:
   imm=rng.getrandbits(16);iw=(1<<26)|(rs<<21)|((0x18+kind-2)<<16)|imm
   z=(a&(15 if kind==2 else 7))^(imm&(15 if kind==2 else 7))
   struct.pack_into('>I',expected,552,z if kind==2 else z<<1)
  elif kind==4:
   iw=(0x1c<<26)|(rs<<21)|(rt<<16)|(rd<<11)|(27<<6)|0x28
   src=(regs[rs*2]|(regs[rs*2+1]<<64))<<128 | regs[rt*2]|(regs[rt*2+1]<<64)
   value=(src>>((sa&15)*8))&mask128
   if rd:struct.pack_into('>2Q',expected,rd*16,value&mask64,value>>64)
  else:
   fn=[0,1,0x20,0x21][kind-5];iw=(0x1c<<26)|(rs<<21)|(rt<<16)|(rd<<11)|fn;pipe=1 if fn>=0x20 else 0
   acc=(regs[66+pipe]&0xffffffff)|((regs[64+pipe]&0xffffffff)<<32)
   result=(acc+(a*b if fn&1 else signed(a)*signed(b)))&mask64
   struct.pack_into('>Q',expected,(66+pipe)*8,sx(result));struct.pack_into('>Q',expected,(64+pipe)*8,sx(result>>32))
   if rd:struct.pack_into('>Q',expected,rd*16,sx(result))
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_one(C.byref(c),iw)==0,(kind,hex(iw))
  lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,bytes(mem))
  for j in range(32):u.reg_write(UC_PPC_REG_0+j,0xabcd0000+j)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=300)
  got=bytes(u.mem_read(0x20000,4096));assert got==expected,(kind,n,hex(iw),[(i,hex(x),hex(y)) for i,(x,y) in enumerate(zip(got,expected)) if x!=y][:24])
  assert u.reg_read(UC_PPC_REG_1)==0x32000
  for j in range(14,32):assert u.reg_read(UC_PPC_REG_0+j)==0xabcd0000+j
  cases+=1
print('PASS',cases,'generated PPC cases across 9 SA/funnel/multiply-add encodings; all byte shifts, aliasing, pipes, ABI')
(out/'jit_ee_sa_acc_r1270.json').write_text(json.dumps({'cases':cases,'encodings':9,'errors':0,'scope':'Dedicated SA register, byte-masked QFSRV and scalar pipe0/pipe1 multiply-add; full-state/ABI preservation.'},indent=2))
