"""Real generated PPC MULT1/MULTU1/DIV1/DIVU1 with independent integer oracle."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libpipe1_r1268.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libpipe1_r1268.so'));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x2000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
rng=random.Random(1268);edges=[0,1,0xffffffff,0x80000000,0x7fffffff,2,0xfffffffe];mask=(1<<64)-1
signed=lambda x:x-(1<<32) if x&0x80000000 else x
sx=lambda x:signed(x&0xffffffff)&mask
cases=0
for op in range(0x18,0x1c):
 for n in range(1200):
  rs=n%32;rt=rs if n%5==0 else rng.randrange(32);rd=[0,rs,rt,rng.randrange(32)][n%4]
  regs=[rng.getrandbits(64) for k in range(68)];regs[:2]=[0,0]
  if rs:regs[rs*2]=edges[n%7] if n<500 else rng.getrandbits(64)
  if rt:regs[rt*2]=edges[(n//7)%7] if n<500 else rng.getrandbits(64)
  a=regs[rs*2]&0xffffffff;b=regs[rt*2]&0xffffffff;expected=regs[:]
  if op<0x1a:
   v=(signed(a)*signed(b) if op==0x18 else a*b)&mask;lo=v&0xffffffff;hi=v>>32
   if rd:expected[rd*2]=sx(lo)
  elif op==0x1b:lo=a//b if b else 0xffffffff;hi=a%b if b else a
  elif b==0:lo=1 if signed(a)<0 else 0xffffffff;hi=a
  elif a==0x80000000 and b==0xffffffff:lo=0x80000000;hi=0
  else:
   aa=signed(a);bb=signed(b);q=abs(aa)//abs(bb);q=-q if (aa<0)!=(bb<0) else q;lo=q&0xffffffff;hi=(aa-q*bb)&0xffffffff
  expected[65]=sx(hi);expected[67]=sx(lo)
  iw=(0x1c<<26)|(rs<<21)|(rt<<16)|(rd<<11)|op
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_one(C.byref(c),iw)==0;lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>68Q',*regs)+bytes(0x2000))
  for k in range(32):u.reg_write(UC_PPC_REG_0+k,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=200);got=struct.unpack('>68Q',u.mem_read(0x20000,544));assert tuple(expected)==got,(op,n,hex(a),hex(b));cases+=1
print('PASS',cases,'generated PPC pipe1 multiply/divide cases')
(out/'jit_pipe1_r1268.json').write_text(json.dumps({'cases':cases,'errors':0,'scope':'Pipe1 multiply/divide, zero divisors, signed overflow, aliasing, pipe0 preservation; no FPS measurement.'},indent=2))
