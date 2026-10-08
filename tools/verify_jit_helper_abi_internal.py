"""Generated PPC helper calls under deliberately hostile legal EABI callees.
The helper math is an independent oracle; actual linked libm parity is tested
separately in verify_ee_fpu_links_internal.py. No hardware speed claim.
"""
import ctypes as C, math, struct, subprocess
from pathlib import Path
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_PPC32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import *
root=Path(__file__).resolve().parents[1]
out=root/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libhelper_abi_internal.so')],check=True)
class Context(C.Structure):
 _fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libhelper_abi_internal.so'))
lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0,0x1000),(0x10000,0x4000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(address,size)
u.reg_write(UC_PPC_REG_MSR,0x2000)
def floating(bits):return struct.unpack('>f',struct.pack('>I',bits))[0]
def f32(x):return struct.unpack('>I',struct.pack('>f',x))[0]
def helper(uc,address,size,user):
 if address not in [0x100,0x104,0x108,0x120,0x124]:return
 if family=='memory':
  memory_calls.append(tuple(uc.reg_read(UC_PPC_REG_3+n) for n in range(4)))
  if address==0x120:uc.mem_write(uc.reg_read(UC_PPC_REG_5),struct.pack('>QQ',0x90abcdef11223344,0x90abcdef11223344))
  result=None
 elif address!=0x108:return # host sentinels share CTR-aligned address
 if family=='memory':pass
 elif family=='cvt':
  arg=uc.reg_read(UC_PPC_REG_3);arg=arg-0x100000000 if arg&0x80000000 else arg
  result=floating(f32(float(arg)))
 else:
  arg=struct.unpack('>d',struct.pack('>Q',uc.reg_read(UC_PPC_REG_FPR1)))[0]
  result=floating(f32(math.sqrt(arg)))
 # Linkage plus register argument homes belong to the callee. Poison them
 # as well as all volatile integer/FPR registers, retaining only f1 result.
 sp=uc.reg_read(UC_PPC_REG_1);lr=uc.reg_read(UC_PPC_REG_LR)
 uc.mem_write(sp+4,struct.pack('>I',lr))
 uc.mem_write(sp+8,bytes.fromhex('cafebabe')*8)
 for n in range(3,13):uc.reg_write(UC_PPC_REG_0+n,0xcafe0000+n)
 for n in list(range(14)):
  uc.reg_write(UC_PPC_REG_FPR0+n,struct.unpack('>Q',struct.pack('>d',-42.0))[0])
 if family=='memory':
  uc.reg_write(UC_PPC_REG_3,0x90abcdef);uc.reg_write(UC_PPC_REG_4,0x11223344)
 else:uc.reg_write(UC_PPC_REG_FPR1,struct.unpack('>Q',struct.pack('>d',result))[0])
 uc.reg_write(UC_PPC_REG_PC,lr)
u.hook_add(UC_HOOK_CODE,helper)
cases=0
for family in ['sqrt','rsqrt','cvt','vsqrt','vrsqrt']:
 for bits in [0,0x80000000,0x3f800000,0x40800000,0xc0800000,0x7f7fffff]:
  mem=bytearray(0x3000);dest=1464+12
  struct.pack_into('>I',mem,1464+4,f32(9.0));struct.pack_into('>I',mem,1464+8,bits)
  if family=='cvt':
   iw=(17<<26)|(20<<21)|(2<<11)|(3<<6)|32
   signed=bits-0x100000000 if bits&0x80000000 else bits;expected=f32(float(signed))
  elif family in ['sqrt','rsqrt']:
   fn=4 if family=='sqrt' else 22;iw=(17<<26)|(16<<21)|(2<<16)|(1<<11)|(3<<6)|fn
   if bits&0x7f800000==0:expected=(bits&0x80000000)|(0 if family=='sqrt' else 0x7f7fffff)
   else:expected=f32(math.sqrt(abs(floating(bits))) if family=='sqrt' else 9.0/floating(f32(math.sqrt(abs(floating(bits))))))
  else:
   idx=57 if family=='vsqrt' else 58
   iw=(18<<26)|(16<<21)|(2<<16)|(1<<11)|((idx>>2)<<6)|0x3c|(idx&3)
   struct.pack_into('>I',mem,1728+16,f32(9.0));struct.pack_into('>I',mem,1728+32,bits);dest=1600+22*4
   if family=='vsqrt':expected=f32(math.sqrt(abs(floating(bits))))
   elif bits&0x7fffffff==0:expected=(bits&0x80000000)|0x7f7fffff
   else:expected=f32(9.0/floating(f32(math.sqrt(abs(floating(bits))))))
  ctx=Context();assert lib.ppc_dynarec_init(C.byref(ctx),1)==0
  assert lib.ppc_dynarec_translate_one(C.byref(ctx),iw)==0
  lib.ppc_dynarec_finalize(C.byref(ctx));code=struct.pack('>'+str(ctx.used)+'I',*ctx.code[:ctx.used]);lib.ppc_dynarec_free(C.byref(ctx))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x14000);u.mem_write(0x20000,bytes(mem))
  saved=[0xa1000000+n for n in range(18)]
  for n,val in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,val)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x13000)
  u.emu_start(0x10000,0x13000,count=1000)
  actual=int.from_bytes(bytes(u.mem_read(0x20000+dest,4)),'big')
  assert actual==expected,(family,hex(bits),hex(actual),hex(expected))
  assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved,(family,'nonvolatile registers')
  assert u.reg_read(UC_PPC_REG_1)==0x32000,(family,'stack pointer')
  cases+=1
print(f'PASS {cases} generated PPC helper cases with hostile EABI linkage/argument/volatile writes')

# Scalar MMIO fallbacks use helpers even when direct RAM paths are available.
# Exercise all load/store widths, merge words and the atomic quad paths.
family='memory';memory_cases=0
for op in [0x20,0x24,0x21,0x25,0x23,0x27,0x37,0x1e,0x28,0x29,0x2b,0x3f,0x1f,0x22,0x26,0x2a,0x2e]:
 for offset in range(4):
  mem=bytearray(0x3000);old=0x1234567887654321
  struct.pack_into('>Q',mem,3*16,0x10000000);struct.pack_into('>QQ',mem,2*16,old,0x9988776655443322)
  iw=(op<<26)|(3<<21)|(2<<16)|offset
  ctx=Context();assert lib.ppc_dynarec_init(C.byref(ctx),1)==0
  assert lib.ppc_dynarec_translate_one(C.byref(ctx),iw)==0
  lib.ppc_dynarec_finalize(C.byref(ctx));code=struct.pack('>'+str(ctx.used)+'I',*ctx.code[:ctx.used]);lib.ppc_dynarec_free(C.byref(ctx))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x14000);u.mem_write(0x20000,bytes(mem))
  saved=[0xa1000000+n for n in range(18)]
  for n,val in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,val)
  memory_calls=[];u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x13000)
  u.emu_start(0x10000,0x13000,count=1000)
  assert memory_calls,(hex(op),'no helper called')
  assert all(c[0]==0x20000 for c in memory_calls),(hex(op),'context argument')
  assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved,(hex(op),'nonvolatile registers')
  assert u.reg_read(UC_PPC_REG_1)==0x32000,(hex(op),'stack pointer')
  got=int.from_bytes(bytes(u.mem_read(0x20000+32,8)),'big')
  def sext(v):return v|0xffffffff00000000 if v&0x80000000 else v
  if op==0x20:expected=0xffffffffffffffef
  elif op==0x24:expected=0xef
  elif op==0x21:expected=0xffffffffffffcdef
  elif op==0x25:expected=0xcdef
  elif op==0x23:expected=sext(0x90abcdef)
  elif op==0x27:expected=0x90abcdef
  elif op in [0x37,0x1e]:expected=0x90abcdef11223344
  elif op==0x22:expected=sext(((old&0xffffffff)&[0xffffff,0xffff,0xff,0][offset])|((0x90abcdef<<((3-offset)*8))&0xffffffff))
  elif op==0x26:
   low=((old&0xffffffff)&[0,0xff000000,0xffff0000,0xffffff00][offset])|(0x90abcdef>>(offset*8))
   expected=sext(low) if offset==0 else (old&0xffffffff00000000)|low
  else:expected=old
  assert got==expected,(hex(op),offset,hex(got),hex(expected))
  memory_cases+=1
print(f'PASS {memory_cases} scalar memory helper EABI/argument/result cases')
