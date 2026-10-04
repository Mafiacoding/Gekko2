"""Real emitted PPC for ten additional EE integer operations; independent oracles."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libint64_r1268.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libint64_r1268.so'));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x2000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
rng=random.Random(1268);mask=(1<<64)-1;edges=[0,1,mask,1<<63,(1<<63)-1,0xffffffff,0x100000000,0x80000000]
names=['DADDI','DADDIU','DSLL32','DSRL32','DSRA32','DSLLV','DSRLV','DSRAV','ADD','SUB'];cases=0
for name in names:
 for i in range(900):
  rs=i%32;rt=rs if i%5==0 else rng.randrange(32);rd=[0,rt,rs,rng.randrange(32)][i%4];sa=i%32;imm=[-32768,-1,0,1,32767][i%5] if i<300 else rng.randrange(-32768,32768)
  regs=[rng.getrandbits(64) for _ in range(68)];regs[:2]=[0,0]
  if rt:regs[rt*2]=edges[i%len(edges)] if i<300 else rng.getrandbits(64)
  if rs:regs[rs*2]=(regs[rs*2]&~255)|(i%128) # Include counts 64..127, then mask to six bits.
  a=regs[rs*2];b=regs[rt*2];signed=b-(1<<64) if b>>63 else b;expect=regs[:]
  if name.startswith('DADDI'):
   iw=({'DADDI':0x18,'DADDIU':0x19}[name]<<26)|(rs<<21)|(rt<<16)|(imm&65535);dest=rt;value=(a+imm)&mask
  else:
   fun={'DSLL32':0x3c,'DSRL32':0x3e,'DSRA32':0x3f,'DSLLV':0x14,'DSRLV':0x16,'DSRAV':0x17,'ADD':0x20,'SUB':0x22}[name]
   iw=(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|fun;dest=rd;n=sa+32 if name.endswith('32') else a&63
   if name.startswith('DSLL'):value=(b<<n)&mask
   elif name.startswith('DSRL'):value=b>>n
   elif name.startswith('DSRA'):value=(signed>>n)&mask
   else:
    raw=((a+b) if name=='ADD' else (a-b))&0xffffffff;value=(raw if raw<0x80000000 else raw-(1<<32))&mask
  if dest:expect[dest*2]=value
  ctx=Context();assert lib.ppc_dynarec_init(C.byref(ctx),1)==0
  assert lib.ppc_dynarec_translate_one(C.byref(ctx),iw)==0,(name,hex(iw))
  lib.ppc_dynarec_finalize(C.byref(ctx));code=struct.pack('>'+str(ctx.used)+'I',*ctx.code[:ctx.used]);lib.ppc_dynarec_free(C.byref(ctx))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>68Q',*regs)+bytes(0x2000))
  for k in range(32):u.reg_write(UC_PPC_REG_0+k,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=150);got=struct.unpack('>68Q',u.mem_read(0x20000,544))
  assert tuple(expect)==got,(name,i,rs,rt,rd,sa,hex(a),hex(b),[(k,hex(x),hex(y)) for k,(x,y) in enumerate(zip(expect,got)) if x!=y]);cases+=1
print('PASS',cases,'generated PPC cases for',','.join(names))
(out/'jit_int64_r1268.json').write_text(json.dumps({'cases':cases,'errors':0,'operations':names,'scope':'Generated PPC; signed ADD/DADDI follow current interpreter overflow convention; no FPS measurement.'},indent=2))
