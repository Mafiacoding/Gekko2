"""Independent generated-PPC continuation oracle with hostile EABI calls."""
import ctypes as C,struct,subprocess
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libchain_internal.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libchain_internal.so'))
lib.ppc_dynarec_translate_ee_cached_chain.argtypes=[C.POINTER(Context),C.c_uint32]
c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0
assert lib.ppc_dynarec_translate_ee_cached_chain(C.byref(c),0x12000)==0
lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for addr,size in [(0x10000,0x5000),(0x20000,0x4000),(0x30000,0x4000)]:u.mem_map(addr,size)
u.mem_write(0x10000,code)
def hook(uc,address,size,user):
 if address not in [0x11000,0x11004,0x12000]:return
 args=[uc.reg_read(UC_PPC_REG_3+n) for n in range(6)]
 assert args[0]==0x20000
 lr=uc.reg_read(UC_PPC_REG_LR);sp=uc.reg_read(UC_PPC_REG_1)
 uc.mem_write(sp+4,struct.pack('>I',lr));uc.mem_write(sp+8,bytes.fromhex('cafebabe')*8)
 if address==0x12000:
  remaining=args[1];resolves.append(remaining)
  if remaining<3 or len(blocks)>=3:ret,count=0,0
  else:ret,count=0x11004,3
 else:
  if address==0x11000:assert args[1:3]==[1,0x300001];ret=first_result
  else:assert args[1:3]==[0,0];ret=3
  blocks.append(ret);count=0
 for n in range(3,13):uc.reg_write(UC_PPC_REG_0+n,0xcafe0000+n)
 uc.reg_write(UC_PPC_REG_3,ret);uc.reg_write(UC_PPC_REG_4,count);uc.reg_write(UC_PPC_REG_PC,lr)
u.hook_add(UC_HOOK_CODE,hook)
cases=0
for budget in range(2,17):
 for first_result in [0,1,2]:
  blocks=[];resolves=[];saved=[0xa1000000+n for n in range(18)]
  for n,val in enumerate(saved):u.reg_write(UC_PPC_REG_14+n,val)
  for n,val in enumerate([0x20000,1,0x300001,budget,0x11000,2]):u.reg_write(UC_PPC_REG_3+n,val)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_LR,0x14000)
  u.emu_start(0x10000,0x14000,count=2000)
  expected=first_result
  if first_result==2:
   for n in range(2):
    if budget-expected<3:break
    expected+=3
  assert u.reg_read(UC_PPC_REG_3)==expected and sum(blocks)==expected,(budget,first_result,blocks,resolves)
  assert expected<=budget
  assert [u.reg_read(UC_PPC_REG_14+n) for n in range(18)]==saved
  assert u.reg_read(UC_PPC_REG_1)==0x32000
  if first_result!=2:assert not resolves
  cases+=1
print(f'PASS {cases} native continuation oracle cases, arguments/budgets/partial exits and hostile EABI')
