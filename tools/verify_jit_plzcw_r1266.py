"""Run generated PLZCW PPC blocks, including aliasing/zero and all untouched lanes."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libplzcw.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libplzcw.so'));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x2000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
rng=random.Random(1266);edges=[0,1,0x7fffffff,0x80000000,0xffffffff,0xfffffffe]+[1<<n for n in range(32)]+[~(1<<n)&0xffffffff for n in range(32)]
def count(value):
 bits=f'{value:032b}';return len(bits)-len(bits.lstrip(bits[0]))-1
cases=0
for i in range(2200):
 rs=i%32;rd=rs if i%3==0 else (0 if i%3==1 else rng.randrange(32));words=[edges[i%len(edges)],edges[(i*17)%len(edges)]] if i<1400 else [rng.getrandbits(32),rng.getrandbits(32)]
 regs=[rng.getrandbits(64) for _ in range(68)];regs[:2]=[0,0]
 if rs:regs[rs*2]=words[0]|words[1]<<32
 source=regs[rs*2];expect=regs[:]
 if rd:expect[rd*2]=count(source&0xffffffff)|(count(source>>32)<<32)
 ctx=Context();assert lib.ppc_dynarec_init(C.byref(ctx),1)==0
 assert lib.ppc_dynarec_translate_one(C.byref(ctx),(28<<26)|(rs<<21)|(rd<<11)|4)==0
 lib.ppc_dynarec_finalize(C.byref(ctx));code=struct.pack('>'+str(ctx.used)+'I',*ctx.code[:ctx.used]);lib.ppc_dynarec_free(C.byref(ctx))
 u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>68Q',*regs)+bytes(0x2000))
 for n in range(32):u.reg_write(UC_PPC_REG_0+n,0)
 u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
 u.emu_start(0x10000,0x11000,count=100)
 got=struct.unpack('>68Q',u.mem_read(0x20000,544));assert tuple(expect)==got,(i,rs,rd,hex(source));cases+=1
print('PASS',cases,'generated PPC PLZCW cases')
(out/'jit_plzcw_r1266.json').write_text(json.dumps({'cases':cases,'errors':0,'scope':'Generated PPC instructions under Unicorn; no Wii FPS measurement.'},indent=2))
