"""Execute emitted PPC memory operations in Unicorn and check MIPS results.
Requires: python3 -m pip install unicorn==2.1.4
Runs the real source translator, including real helper-call ABI and clobbers.
"""
import ctypes as C,struct,random,subprocess,json,os
from pathlib import Path
from unicorn import *
from unicorn.ppc_const import *
root=Path(__file__).resolve().parents[1]
out=root/'outputs/verification';out.mkdir(parents=True,exist_ok=True)
class Ctx(C.Structure):
    _fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
libpath=out/'libcodegen.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(libpath)],check=True)
lib=C.CDLL(os.environ.get('CODEGEN_LIBRARY',str(libpath)))
lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Ctx),C.c_uint32]
rng=random.Random(1251);errors=[];u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for a,n in [(0,0x1000),(0x10000,0x1000),(0x20000,0x4000),(0x30000,0x4000),(0x40000,0x10000)]:u.mem_map(a,n)
mem=bytearray();calls=[]
def helper(uc,addr,size,_):
 if addr>=0x1000:return
 addr=uc.reg_read(UC_PPC_REG_CTR)
 calls.append(addr);ea=uc.reg_read(UC_PPC_REG_4)&0x1fffffff
 sizes={0x101:4,0x102:4,0x103:1,0x104:2,0x105:1,0x106:2,0x107:8,0x108:8}
 if addr not in sizes:raise RuntimeError(hex(addr))
 n=sizes[addr]
 if addr in [0x101,0x103,0x104,0x107]:v=int.from_bytes(u.mem_read(0x40000+ea,n),'little');ret=v
 else:
  v=uc.reg_read(UC_PPC_REG_5)
  if n==8:v=(v<<32)|uc.reg_read(UC_PPC_REG_6)
  u.mem_write(0x40000+ea,(v&((1<<(n*8))-1)).to_bytes(n,'little'));ret=0
 for r in range(3,13):uc.reg_write(UC_PPC_REG_0+r,0xdecaf000+r)
 if addr==0x107:uc.reg_write(UC_PPC_REG_3,ret>>32);uc.reg_write(UC_PPC_REG_4,ret&0xffffffff)
 else:uc.reg_write(UC_PPC_REG_3,ret&0xffffffff)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,helper)
count=0
for op in [0x20,0x24,0x21,0x25,0x23,0x27,0x37,0x28,0x29,0x2b,0x3f,0x1e,0x1f]:
 for i in range(80):
  rs=rng.randrange(1,32);rt=rs if i%4==0 else rng.randrange(32);imm=rng.randrange(-128,128);ea=0x1000+(i%7)*16
  if op not in [0x1e,0x1f]:ea+=(i%4)
  addr=ea+([0,0x80000000,0xa0000000][i%3]);iw=(op<<26)|(rs<<21)|(rt<<16)|(imm&65535)
  c=Ctx();lib.ppc_dynarec_init(C.byref(c),1);assert lib.ppc_dynarec_translate_one(C.byref(c),iw)==0;lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  regs=[rng.getrandbits(64) for _ in range(68)];regs[0]=regs[1]=0;regs[rs*2]=(addr-imm)&0xffffffffffffffff;expected=regs[:]
  payload=bytes(rng.randrange(256) for _ in range(0x3000));expected_mem=bytearray(payload);pa=ea
  if op in [0x1e,0x1f]:pa&=~15
  n={0x20:1,0x24:1,0x21:2,0x25:2,0x23:4,0x27:4,0x37:8,0x28:1,0x29:2,0x2b:4,0x3f:8,0x1e:16,0x1f:16}[op]
  if op in [0x20,0x24,0x21,0x25,0x23,0x27,0x37,0x1e]:
   v=int.from_bytes(payload[pa:pa+n],'little',signed=op in [0x20,0x21,0x23])&((1<<128)-1)
   if rt:expected[rt*2]=v&0xffffffffffffffff
   if rt and n==16:expected[rt*2+1]=v>>64
  else:
   v=regs[rt*2]|(regs[rt*2+1]<<64);expected_mem[pa:pa+n]=(v&((1<<(n*8))-1)).to_bytes(n,'little')
  for r in range(32):u.reg_write(UC_PPC_REG_0+r,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x10800)
  u.mem_write(0x20000,struct.pack('>68Q',*regs)+bytes(0x3000));u.mem_write(0x20000+10480,struct.pack('>II',0x40000,0x10000));u.mem_write(0x40000,payload);u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x11000);calls.clear()
  try:u.emu_start(0x10000,0x10800,count=400)
  except (UcError,RuntimeError) as e:errors.append({'iw':hex(iw),'error':str(e),'calls':calls[:]});continue
  got=struct.unpack('>68Q',u.mem_read(0x20000,544));gm=u.mem_read(0x40000,len(payload));count+=1
  if tuple(expected)!=got or expected_mem!=gm:errors.append({'iw':hex(iw),'op':hex(op),'rs':rs,'rt':rt,'imm':imm,'addr':hex(addr),'calls':calls[:],'diff':[(k,hex(a),hex(b)) for k,(a,b) in enumerate(zip(got,expected)) if a!=b],'memory_diff':sum(a!=b for a,b in zip(gm,expected_mem))})
print('MEMORY_CASES',count,'ERRORS',len(errors));print(json.dumps(errors[:15],indent=2));(out/'jit_memory_results.json').write_text(json.dumps({'cases':count,'errors':errors},indent=2));raise SystemExit(bool(errors))
