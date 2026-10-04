"""Execute generated VU micro arithmetic on PPC and compare the C interpreter.
The reference excludes the JIT hooks; finite floats, raw register transfers,
all destination masks/aliasing, VI16 aliases, and ABI preservation are tested.
"""
import ctypes as C,struct,random,subprocess,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
r=Path(__file__).resolve().parents[1];out=r/'outputs/verification';out.mkdir(exist_ok=True)
s=(r/'source/hw/vu.c').read_text().replace('    if (vu_jit_try_upper(vf, vi, acc, w)) return 1;','').replace('    if (vu_jit_try_lower(vf, vi, mem, mem_mask, w, pc, branch_delay, branch_target)) return 1;','')
s+='''\nint reference_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint32_t w){return vu_exec_upper(vf,vi,acc,w);}
int reference_lower(uint32_t vf[32][4],uint32_t *vi,uint8_t *m,uint32_t mask,uint32_t w,uint32_t pc,uint32_t *d,uint32_t *t){return vu_exec_lower(vf,vi,m,mask,w,pc,d,t);}
static vif_state_t stub_vif;
vif_state_t *vif0_get_state(void){return &stub_vif;}
vif_state_t *vif1_get_state(void){return &stub_vif;}
void gif_process_quadwords(int c,const uint8_t*d,uint32_t q){(void)c;(void)d;(void)q;}
'''
ref=out/'vu_reference_r1271_pair.c';ref.write_text(s)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),'-I'+str(r/'source/hw'),str(ref),str(r/'source/core/recompiler/vu_jit.c'),'-lm','-o',str(out/'libvu_ref_r1271_pair.so')],check=True)
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(r/'include'),str(r/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libvu_jit_r1271_pair.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libvu_jit_r1271_pair.so'));ref=C.CDLL(str(out/'libvu_ref_r1271_pair.so'))
for name in ['ppc_dynarec_translate_vu_upper','ppc_dynarec_translate_vu_lower']:getattr(lib,name).argtypes=[C.POINTER(Context),C.c_uint32]
lib.ppc_dynarec_translate_vu_pair.argtypes=[C.POINTER(Context),C.c_uint32,C.c_uint32]
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
for address,size in [(0x10000,0x4000),(0x20000,0x4000),(0x30000,0x4000),(0x40000,0x4000)]:u.mem_map(address,size)
u.reg_write(UC_PPC_REG_MSR,0x2000)
rng=random.Random(1269);values=[0.,-0.,1.,-1.,2.5,-3.25,1e-10,-1e-10,1e10,-1e10]
normal=list(range(48))
special=list(range(43))+list(range(44,48))
upper_cases=lower_cases=0
Vf=(C.c_uint32*4)*32;Vi=C.c_uint32*32;Ac=C.c_uint32*4

def execute(w,lower,n):
 global upper_cases,lower_cases
 vf=Vf();vi=Vi();acc=Ac()
 for j in range(32):
  for k in range(4):vf[j][k]=struct.unpack('>I',struct.pack('>f',rng.choice(values)))[0] if not lower else rng.getrandbits(32)
 for k in range(4):vf[0][k]=0x3f800000 if k==3 else 0
 for j in range(32):vi[j]=rng.getrandbits(32)
 for k in range(4):acc[k]=struct.unpack('>I',struct.pack('>f',rng.choice(values)))[0]
 for j in [21,22]:vi[j]=struct.unpack('>I',struct.pack('>f',rng.choice(values)))[0]
 fn=w&63; idx=(((w>>6)&31)<<2)|(fn&3)
 if not lower and (fn in list(range(16,24))+[29,31,43,47] or (fn>=60 and 16<=idx<=23)):
  edges=[0,0x80000000,0x7fffffff,0xffffffff,0x7f800000,0xff800000,0x7fc12345,0xffc12345,0x4f000000,0xcf000000,1,0x80000001]
  for j in range(1,32):
   for k in range(4):vf[j][k]=edges[(n+j+k)%len(edges)] if n%2 else rng.getrandbits(32)
  vi[21]=edges[n%len(edges)] if n%2 else rng.getrandbits(32)
 raw=list(x for row in vf for x in row)+list(vi)+list(acc)
 mem_mask=4095 if n%2 else 16383
 mem_bytes=rng.randbytes(16384);mem=(C.c_uint8*16384).from_buffer_copy(mem_bytes)
 delay=C.c_uint32(n%3);target=C.c_uint32(rng.getrandbits(32));initial_control=(delay.value,target.value);pc=[0,8,4088,16376][n%4]
 upper=(15<<21)|(2<<16)|(1<<11)|(3<<6)|0x2b if lower else w
 low=w if lower else (0x40<<25)|(1<<16)|(1<<11)|(2<<6)|0x30
 if n%5==0:upper|=0x80000000;low=rng.getrandbits(32)
 if upper&0x80000000:vi[21]=low
 valid=ref.reference_upper(vf,vi,acc,upper)
 if not upper&0x80000000:valid &= ref.reference_lower(vf,vi,mem,mem_mask,low,pc,C.byref(delay),C.byref(target))
 assert valid==1,('reference',lower,hex(w))
 expected=list(x for row in vf for x in row)+list(vi)+list(acc)
 c=Context();assert lib.ppc_dynarec_init(C.byref(c),2)==0
 result=lib.ppc_dynarec_translate_vu_pair(C.byref(c),upper,low)
 assert result==0,('translate',lower,hex(w),result)
 lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
 u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x14000);u.mem_write(0x20000,struct.pack('>164I',*raw));u.mem_write(0x40000,mem_bytes);u.mem_write(0x21000,struct.pack('>2I',*initial_control))
 for k in range(32):u.reg_write(UC_PPC_REG_0+k,0xdead0000+k)
 u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_4,0x20200);u.reg_write(UC_PPC_REG_5,0x20280);u.reg_write(UC_PPC_REG_6,0x40000);u.reg_write(UC_PPC_REG_7,mem_mask);u.reg_write(UC_PPC_REG_8,pc);u.reg_write(UC_PPC_REG_9,0x21000);u.reg_write(UC_PPC_REG_10,0x21004);u.reg_write(UC_PPC_REG_LR,0x11000)
 u.emu_start(0x10000,0x11000,count=1000);got=struct.unpack('>164I',bytes(u.mem_read(0x20000,656)))
 assert list(got)==expected,(lower,n,hex(w),[(i,hex(a),hex(b)) for i,(a,b) in enumerate(zip(got,expected)) if a!=b])
 if lower:
  assert bytes(u.mem_read(0x40000,16384))==bytes(mem),(hex(w),'memory')
  assert struct.unpack('>2I',bytes(u.mem_read(0x21000,8)))==(delay.value,target.value),(hex(w),'branch',initial_control,delay.value,target.value,struct.unpack('>2I',bytes(u.mem_read(0x21000,8))))
 assert u.reg_read(UC_PPC_REG_1)==0x32000
 for k in range(14,32):assert u.reg_read(UC_PPC_REG_0+k)==0xdead0000+k,(k,hex(w))
 if lower:lower_cases+=1
 else:upper_cases+=1

for special_mode,operations in [(False,normal),(True,special)]:
 for op in operations:
  for n in range(64):
   fs=[0,1,2,31][n%4];ft=fs if n%3==0 else [0,3,4,30][(n//4)%4];fd=[0,fs,ft,5][(n//16)%4];mask=n%16
   w=(mask<<21)|(ft<<16)|(fs<<11)|((((op>>2)<<6)|0x3c|(op&3)) if special_mode else (fd<<6)|op)
   w|=(n%4)<<30 # I/E flags must not change arithmetic decoding
   execute(w,False,n)

for kind in range(33):
 for n in range(160):
  rs=[0,1,15,16,17,31][n%6];rt=rs if n%3==0 else [0,2,15,16,18,31][(n//6)%6];rd=[0,rs,rt,3,16,31][(n//24)%6];mask=n%16
  if kind<2:w=((8+kind)<<25)|(rt<<16)|(rs<<11)|(rng.randrange(16)<<21)|rng.randrange(2048)
  elif kind<7:w=(0x40<<25)|(rt<<16)|(rs<<11)|(rd<<6)|(0x30+kind-2)
  elif kind<9:w=(0x40<<25)|(mask<<21)|(rt<<16)|(rs<<11)|(12<<6)|0x3c|(kind-7)
  elif kind<11:w=(0x40<<25)|(mask<<21)|(rt<<16)|(rs<<11)|(15<<6)|0x3c|(kind-9)
  elif kind<13:w=(0x40<<25)|((14 if kind==11 else 30)<<6)|0x3f
  elif kind<17:w=([0,1,4,5][kind-13]<<25)|(mask<<21)|(rt<<16)|(rs<<11)|rng.randrange(2048)
  elif kind<21:w=(0x40<<25)|(mask<<21)|(rt<<16)|(rs<<11)|(13<<6)|0x3c|(kind-17)
  elif kind<23:w=(0x40<<25)|(mask<<21)|(rt<<16)|(rs<<11)|(15<<6)|0x3e|(kind-21)
  else:w=([0x20,0x21,0x24,0x25,0x28,0x29,0x2c,0x2d,0x2e,0x2f][kind-23]<<25)|(rt<<16)|(rs<<11)|rng.randrange(2048)
  execute(w,True,n)

print('PASS',upper_cases+lower_cases,'native fused pair cases; I-immediate, aliases, lower branches/memory, ABI')
