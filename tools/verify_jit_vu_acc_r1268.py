"""Generated PPC VU0 ACC macro operations against independent float32 oracle."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libacc_r1268.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libacc_r1268.so'));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x2000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
u.reg_write(UC_PPC_REG_MSR,0x2000)
rng=random.Random(1268);values=[0.,-0.,1.,-1.,2.5,-3.25,1e-10,-1e-10,1e10,-1e10];indices=list(range(16))+list(range(24,29))+[30]+list(range(32,43))+list(range(44,48));cases=0
f32=lambda x:struct.unpack('>f',struct.pack('>f',x))[0]
for idx in indices:
 for n in range(64):
  fs=[0,1,2,31][n%4];ft=fs if n%3==0 else [0,3,4,30][(n//4)%4];mask=n%16
  mem=bytearray(rng.randbytes(0x3000));vf=[[f32(rng.choice(values)) for k in range(4)] for j in range(32)];acc=[f32(rng.choice(values)) for k in range(4)];q=f32(rng.choice(values));ii=f32(rng.choice(values))
  for j in range(32):struct.pack_into('>4f',mem,1728+j*16,*vf[j])
  struct.pack_into('>4f',mem,10464,*acc);struct.pack_into('>f',mem,1600+22*4,q);struct.pack_into('>f',mem,1600+21*4,ii)
  expected=mem[:]
  def read(j,k):return vf[j][k] if j else (1. if k==3 else 0.)
  if idx==46:
   for k in range(3):struct.pack_into('>f',expected,10464+k*4,f32(read(fs,(k+1)%3)*read(ft,(k+2)%3)))
  elif idx!=47:
   broadcast=idx<=15 or 24<=idx<=28 or idx==30 or 32<=idx<=39
   if idx<=15:kind=idx//4
   elif idx<=30:kind=6
   elif idx<=39:kind=3 if idx&4 and idx&1 else 1 if idx&4 else 2 if idx&1 else 0
   else:kind={40:0,41:2,42:6,44:1,45:3}[idx]
   for k in range(4):
    if not mask&(8>>k):continue
    a=read(fs,k)
    if broadcast:b=ii if idx==30 or idx>=32 and idx&2 else q if idx==28 or idx>=32 else read(ft,idx&3)
    else:b=read(ft,k)
    if kind==0:z=f32(a+b)
    elif kind==1:z=f32(a-b)
    elif kind==2:z=f32(acc[k]+f32(a*b))
    elif kind==3:z=f32(acc[k]-f32(a*b))
    else:z=f32(a*b)
    struct.pack_into('>f',expected,10464+k*4,z)
  iw=(0x12<<26)|((0x10|mask)<<21)|(ft<<16)|(fs<<11)|((idx>>2)<<6)|0x3c|(idx&3)
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_one(C.byref(c),iw)==0,(idx,hex(iw));lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,bytes(mem))
  for k in range(32):u.reg_write(UC_PPC_REG_0+k,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=200);got=bytes(u.mem_read(0x20000,len(mem)))
  assert got==expected,(idx,n,fs,ft,mask,[(k,hex(struct.unpack_from('>I',expected,10464+k*4)[0]),hex(struct.unpack_from('>I',got,10464+k*4)[0])) for k in range(4)])
  assert u.reg_read(UC_PPC_REG_1)==0x32000;cases+=1
print('PASS',cases,'generated PPC VU0 ACC macro cases across',len(indices),'operations')
(out/'jit_vu_acc_r1268.json').write_text(json.dumps({'cases':cases,'errors':0,'indices':indices,'scope':'VU0 macro-mode ACC, finite float32 arithmetic and VF00 constants; not VU micro JIT or full PS2 float/flag accuracy.'},indent=2))
