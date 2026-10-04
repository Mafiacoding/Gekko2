"""Direct R3000A PPC code, all 32 initial operations, independent uint32 model."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libiop_r1268.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libiop_r1268.so'));lib.ppc_dynarec_translate_iop_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x2000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
rng=random.Random(1268);edges=[0,1,0xffffffff,0x80000000,0x7fffffff,32,63];mask=0xffffffff
signed=lambda x:x-(1<<32) if x&0x80000000 else x
ops=[('immediate',n) for n in range(8,16)]+[('special',n) for n in [0,2,3,4,6,7,0x10,0x11,0x12,0x13,0x18,0x19,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b]]+[('cop0',n) for n in [0,4]]
cases=0
for group,fn in ops:
 for n in range(500):
  rs=n%32;rt=rs if n%5==0 else rng.randrange(32);rd=[0,rs,rt,rng.randrange(32)][n%4];sa=n%32;imm=[-32768,-1,0,1,32767][n%5] if n<200 else rng.randrange(-32768,32768)
  words=[rng.getrandbits(32) for k in range(256)];words[0]=0
  if rs:words[rs]=edges[n%7] if n<200 else rng.getrandbits(32)
  if rt:words[rt]=edges[(n//7)%7] if n<200 else rng.getrandbits(32)
  a=words[rs];b=words[rt];expected=words[:];dest=0;value=0
  if group=='immediate':
   iw=(fn<<26)|(rs<<21)|(rt<<16)|(imm&65535);dest=rt
   value={8:lambda:(a+imm)&mask,9:lambda:(a+imm)&mask,10:lambda:int(signed(a)<imm),11:lambda:int(a<(imm&mask)),12:lambda:a&(imm&65535),13:lambda:a|(imm&65535),14:lambda:a^(imm&65535),15:lambda:(imm&65535)<<16}[fn]()
  elif group=='cop0':
   iw=(0x10<<26)|(fn<<21)|(rt<<16)|(rd<<11)
   if fn==0:dest=rt;value=words[36+rd]
   else:expected[36+rd]=b
  else:
   iw=(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|fn;dest=rd
   if fn in [0,2,3,4,6,7]:
    k=sa if fn<4 else a&31;value=((b<<k)&mask) if fn in [0,4] else (b>>k) if fn in [2,6] else signed(b)>>k
   elif fn in [0x10,0x12]:value=words[34 if fn==0x10 else 35]
   elif fn in [0x11,0x13]:expected[34 if fn==0x11 else 35]=a;dest=0
   elif fn in [0x18,0x19]:
    p=(signed(a)*signed(b) if fn==0x18 else a*b)&((1<<64)-1);expected[34]=p>>32;expected[35]=p&mask;dest=0
   else:value={0x20:lambda:a+b,0x21:lambda:a+b,0x22:lambda:a-b,0x23:lambda:a-b,0x24:lambda:a&b,0x25:lambda:a|b,0x26:lambda:a^b,0x27:lambda:~(a|b),0x2a:lambda:int(signed(a)<signed(b)),0x2b:lambda:int(a<b)}[fn]()
  if dest:expected[dest]=value&mask
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_iop_one(C.byref(c),iw)==0,(group,fn,hex(iw));lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>256I',*words))
  for k in range(32):u.reg_write(UC_PPC_REG_0+k,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=100);got=struct.unpack('>256I',u.mem_read(0x20000,1024));assert tuple(expected)==got,(group,fn,n,hex(a),hex(b));cases+=1
# HLE-sensitive calls, RFE, exceptions and reserved COP3 decline.
for iw in [0x0c000000,0x0020f809,0x0000000c,0x42000010,0x4c000000]:
 c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_iop_one(C.byref(c),iw)==-1;lib.ppc_dynarec_free(C.byref(c))
print('PASS',cases,'generated PPC IOP cases across',len(ops),'operations, plus 5 deliberate fallbacks')
(out/'jit_iop_r1268.json').write_text(json.dumps({'cases':cases,'errors':0,'operations':ops,'fallbacks':5,'scope':'Initial IOP scalar backend; memory/control/exception instructions remain interpreter; no FPS measurement.'},indent=2))
