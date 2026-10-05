"""Independent integer oracles for additional MMI PPC bodies and linked CPU."""
import ctypes as C,struct,random,subprocess,sys
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
root=Path(__file__).resolve().parents[1];out=root/'outputs/verification';out.mkdir(parents=True,exist_ok=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libmmi_extra.so')],check=True)
lib=C.CDLL(str(out/'libmmi_extra.so'));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
specs=[(9,x) for x in [16,17,20,21,10,26,27,30,2,3,0,4]]+[(41,x) for x in [0,10,26,30,3,12,13]]
specs += [(8,x) for x in [2,3,6,7,10,16,17,20,21,24,25,30,31]]+[(40,x) for x in [1,2,3,4,5,6,7,10,17,21,25]]
mask=(1<<32)-1;wide=(1<<64)-1
woff=lambda r,k:r*16+[4,0,12,8][k]
hoff=lambda r,k:r*16+[6,4,2,0,14,12,10,8][k]
bofs=[7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8]
def signed(v,bits):return v-(1<<bits) if v>>(bits-1) else v
def word(b,r,k):return struct.unpack_from('>I',b,woff(r,k))[0]
def setword(b,r,k,v):struct.pack_into('>I',b,woff(r,k),v&mask)
def half(b,r,k):return struct.unpack_from('>h',b,hoff(r,k))[0]
def oracle(initial,fn,sa,rs,rt,rd):
 e=initial[:]
 if fn==9 and sa in [16,17,20,21]:
  products=[half(initial,rs,k)*half(initial,rt,k) for k in range(8)]
  for k in range(8):
   bank=32 if k&2 else 33;lane=k//4*2+k%2
   if sa in [16,20]:v=word(initial,bank,lane)+(products[k] if sa==16 else -products[k])
   elif k%2:v=products[k] if sa==17 else ~products[k]
   else:v=products[k+1]+products[k] if sa==17 else products[k+1]-products[k]
   setword(e,bank,lane,v)
   if rd and k%2==0:setword(e,rd,k//2,v)
 elif (fn==9 and sa in [10,26,27,30]) or (fn==41 and sa in [10,26,30]):
  if rd:
   if sa==30:
    for k,src in enumerate([2,1,0,3] if fn==9 else [0,2,1,3]):setword(e,rd,k,word(initial,rt,src))
   else:
    for k in range(8):
     if sa==10:src=rs if k%2 else rt;lane=(k//2+4 if k%2 else k//2) if fn==9 else k&~1
     else:src=rt;lane=([0,2,1,3,4,6,5,7] if fn==41 else ([3,2,1,0,7,6,5,4] if sa==27 else [2,1,0,3,6,5,4,7]))[k]
     struct.pack_into('>H',e,hoff(rd,k),half(initial,src,lane)&65535)
 elif fn in [9,41]:
  for pipe in range(2):
   a=word(initial,rs,pipe*2);b=word(initial,rt,pipe*2)
   if sa in [2,3]:
    sh=a&31;v=(b<<sh) if sa==2 else ((signed(b,32)>>sh) if fn==41 else b>>sh)
    if rd:setword(e,rd,pipe*2,v);setword(e,rd,pipe*2+1,mask if (v&mask)>>31 else 0)
    continue
   oldlo=word(initial,33,pipe*2);oldhi=word(initial,32,pipe*2)
   if fn==9:
    product=signed(a,32)*signed(b,32)
    total=((oldhi<<32)+product if sa==0 else (oldhi<<32)-product)&wide
    if sa==0 and pipe==0 and (b&0x7fffffff) in [0,0x7fffffff] and a!=b:total=(total+0x70000000)&wide
    x=signed(total,64);hi=((abs(x)//mask)*(-1 if x<0 else 1))&mask
    lo=(oldlo+product if sa==0 else oldlo-product)&mask;value=(hi<<32)|lo
   elif sa==13:lo=a//b if b else mask;hi=a%b if b else a;value=0
   else:
    value=(a*b+(((oldhi<<32)|oldlo) if sa==0 else 0))&wide;lo=value&mask;hi=value>>32
   for bank,v in [(33,lo),(32,hi)]:setword(e,bank,pipe*2,v);setword(e,bank,pipe*2+1,mask if v>>31 else 0)
   if rd and not(fn==41 and sa==13):struct.pack_into('>Q',e,rd*16+pipe*8,value)
 else:
  bits=32>>(sa//4) if sa<16 else (32>>((sa-16)//4) if sa<30 else 32)
  if fn==40 and sa==4:bits=16
  limit=(1<<bits)-1;size=bits//8
  def offset(reg,k):return woff(reg,k) if bits==32 else (hoff(reg,k) if bits==16 else reg*16+bofs[k])
  def get(reg,k):return int.from_bytes(initial[offset(reg,k):offset(reg,k)+size],'big')
  if rd:
   for k in range(128//bits):
    a=get(rs,k);b=get(rt,k);aa=signed(a,bits);bb=signed(b,bits)
    if fn==8:
     if sa in [2,6,10]:v=limit if aa>bb else 0
     elif sa in [3,7]:v=max(aa,bb)
     elif sa in [16,17,20,21,24,25]:v=min(limit//2,max(-(limit//2)-1,aa-bb if sa%2 else aa+bb))
     elif sa==30:v=((b&31)<<3)|((b&0x3e0)<<6)|((b&0x7c00)<<9)|((b&0x8000)<<16)
     else:v=((b>>3)&31)|((b>>6)&0x3e0)|((b>>9)&0x7c00)|((b>>16)&0x8000)
    else:
     if sa in [1,5]:v=min(limit//2,abs(bb))
     elif sa in [2,6,10]:v=limit if a==b else 0
     elif sa in [3,7]:v=min(aa,bb)
     elif sa==4:v=a-b if k<4 else a+b
     else:v=max(0,a-b)
    e[offset(rd,k):offset(rd,k)+size]=(v&limit).to_bytes(size,'big')
 return e
rng=random.Random(1310);cases=0;maxwords=0
linked=len(sys.argv)>1
if linked:
 readword=word
 exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
 setup();write32=word;word=readword
else:
 u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
 for addr in [0x10000,0x20000,0x30000]:u.mem_map(addr,0x10000)
for fn,sa in specs:
 if linked:
  word=write32;setup();word=readword
 for n in range(180 if linked else 1200):
  rs=n%32;rt=rs if n%5==0 else rng.randrange(32);rd=[0,rs,rt,rng.randrange(32)][n%4]
  initial=bytearray(rng.randbytes(1024));initial[:16]=bytes(16)
  if n<300:
   for reg in [rs,rt]:
    if reg:
     for k in range(4):setword(initial,reg,k,[0,1,31,32,63,0x80000000,0xffffffff,0x7fffffff,0x80008000,0x7fff7fff][(n+k)%10])
  expected=oracle(initial,fn,sa,rs,rt,rd)
  instr=(28<<26)|(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|fn
  if linked:
   u.mem_write(state,bytes(initial[:544]));write32(state+off['pc'],base);write32(state+off['next_pc'],base+4)
   u.mem_write(ram+base,struct.pack('<I',instr));before=executed()
   assert call('ee_core_step_n',1)==1
   actual=bytes(u.mem_read(state,544));assert executed()==before+1
   assert actual==bytes(expected[:544]),(fn,sa,n,rs,rt,rd)
  else:
   code=(C.c_uint32*1024)();ctx=Context(code,1024,0)
   assert lib.ppc_dynarec_translate_one(C.byref(ctx),instr)==0,(fn,sa)
   assert ctx.used<=128,(fn,sa,ctx.used);maxwords=max(maxwords,ctx.used)
   raw=b''.join(struct.pack('>I',code[k]) for k in range(ctx.used))+struct.pack('>I',0x4e800020)
   u.mem_write(0x10000,raw);u.ctl_remove_cache(0x10000,0x10000+len(raw));u.mem_write(0x20000,bytes(initial))
   u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_1,0x38000);u.reg_write(UC_PPC_REG_LR,0x18000)
   for reg in range(14,32):u.reg_write(UC_PPC_REG_0+reg,0xab000000+reg)
   u.emu_start(0x10000,0x18000,count=10000)
   assert u.reg_read(UC_PPC_REG_PC)==0x18000
   actual=bytes(u.mem_read(0x20000,1024))
   assert actual==bytes(expected),(fn,sa,n,rs,rt,rd,[(k,hex(int.from_bytes(actual[k:k+4],'big')),hex(int.from_bytes(expected[k:k+4],'big'))) for k in range(0,544,4) if actual[k:k+4]!=expected[k:k+4]])
   assert u.reg_read(UC_PPC_REG_1)==0x38000 and u.reg_read(UC_PPC_REG_3)==0x20000
   assert all(u.reg_read(UC_PPC_REG_0+reg)==0xab000000+reg for reg in range(14,32))
  cases+=1
print('PASS',cases,'MMI full-state independent integer oracles; aliases/zero/extremes; max code words',maxwords,'linked',linked)
