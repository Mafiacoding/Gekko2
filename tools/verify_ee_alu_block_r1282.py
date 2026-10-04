"""Experimental EE ALU block compiler; no CPU retirement or hardware FPS claim."""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
so=out/'libee_alu_block_r1282.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(so)],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(so));lib.ppc_dynarec_translate_ee_alu_block.argtypes=[C.POINTER(Context),C.POINTER(C.c_uint32),C.c_uint]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
u.mem_map(0x10000,0x10000);u.mem_map(0x20000,0x10000)
rng=random.Random(1282);mask=(1<<64)-1
funcs=[0,2,3,4,6,7,0x21,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b,0x2d,0x2f]
def signed(v,bits):return (v&((1<<bits)-1))-(1<<bits) if v&(1<<(bits-1)) else v&((1<<bits)-1)
def sext(v):return signed(v,32)&mask
def ref(g,w):
 op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;rd=(w>>11)&31;sa=(w>>6)&31;fn=w&63;imm=w&65535;a=g[rs][0];b=g[rt][0]
 dst=rt
 if op==9:v=sext(a+signed(imm,16))
 elif op==12:v=a&imm
 elif op==13:v=a|imm
 elif op==14:v=a^imm
 elif op==15:v=sext(imm<<16)
 else:
  dst=rd
  if fn==0:v=sext((b&0xffffffff)<<sa)
  elif fn==2:v=sext((b&0xffffffff)>>sa)
  elif fn==3:v=sext(signed(b,32)>>sa)
  elif fn==4:v=sext((b&0xffffffff)<<(a&31))
  elif fn==6:v=sext((b&0xffffffff)>>(a&31))
  elif fn==7:v=sext(signed(b,32)>>(a&31))
  elif fn==0x21:v=sext(a+b)
  elif fn==0x23:v=sext(a-b)
  elif fn==0x24:v=a&b
  elif fn==0x25:v=a|b
  elif fn==0x26:v=a^b
  elif fn==0x27:v=~(a|b)&mask
  elif fn==0x2a:v=int(signed(a,64)<signed(b,64))
  elif fn==0x2b:v=int(a<b)
  elif fn==0x2d:v=(a+b)&mask
  elif fn==0x2f:v=(a-b)&mask
  else:raise AssertionError(fn)
 if dst:g[dst][0]=v
checks=0
for case in range(1200):
 count=2+case%7;words=[];g=[[rng.getrandbits(64),rng.getrandbits(64)] for n in range(32)];g[0]=[0,0]
 original=[x[:] for x in g]
 for i in range(count):
  rs=rng.randrange(8);rt=rng.randrange(8);rd=rng.randrange(8)
  op=[9,12,13,14,15,0][(case+i)%6]
  w=(op<<26)|(rs<<21)|(rt<<16)|rng.randrange(65536) if op else (rs<<21)|(rt<<16)|(rd<<11)|(rng.randrange(32)<<6)|funcs[(case+i)%len(funcs)]
  words.append(w);ref(g,w)
 buf=(C.c_uint32*4096)();ctx=Context(buf,4096,0);wa=(C.c_uint32*count)(*words)
 assert lib.ppc_dynarec_translate_ee_alu_block(C.byref(ctx),wa,count)==0,(case,words)
 code=struct.pack('>'+str(ctx.used+1)+'I',*list(buf)[:ctx.used],0x4e800020)
 u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x1ffff)
 u.mem_write(0x20000,b''.join(struct.pack('>QQ',*x) for x in original))
 u.reg_write(UC_PPC_REG_1,0x2f000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x1fff0)
 for k in range(14,32):u.reg_write(UC_PPC_REG_0+k,0xd00d0000+k)
 u.emu_start(0x10000,0x1fff0,count=10000)
 expected=b''.join(struct.pack('>QQ',*x) for x in g)
 assert bytes(u.mem_read(0x20000,512))==expected,(case,[hex(w) for w in words])
 assert u.reg_read(UC_PPC_REG_1)==0x2f000
 for k in range(14,32):assert u.reg_read(UC_PPC_REG_0+k)==0xd00d0000+k,(case,k)
 checks+=1
for bad in [0x8c220000,0x10000000,0x0000000c,0x40024800]:
 buf=(C.c_uint32*4096)();buf[0]=0x12345678;ctx=Context(buf,4096,1);wa=(C.c_uint32*2)(0x24420001,bad)
 assert lib.ppc_dynarec_translate_ee_alu_block(C.byref(ctx),wa,2)!=0
 assert ctx.used==1 and buf[0]==0x12345678
print('PASS',checks,'experimental native EE ALU blocks: 2..8 instructions, 21 forms, full 128-bit GPR bytes, zero/alias/sign behavior and EABI preservation; transactional unsupported decline. Not enabled in CPU loop.')
