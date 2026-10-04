"""VU Q/I row and VWAITQ; reuses real ACC verifier's compiler/runtime setup."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_jit_vu_acc_r1268.py').read_text(),'verify_jit_vu_acc_r1268.py','exec'))
extra=0
for fn in range(0x20,0x28):
 for n in range(96):
  fs=[0,1,2,31][n%4];fd=[0,fs,3,31][(n//4)%4];mask=n%16
  mem=bytearray(rng.randbytes(0x3000));vf=[[f32(rng.choice(values)) for k in range(4)] for j in range(32)];acc=[f32(rng.choice(values)) for k in range(4)];q=f32(rng.choice(values));ii=f32(rng.choice(values))
  for j in range(32):struct.pack_into('>4f',mem,1728+j*16,*vf[j])
  struct.pack_into('>4f',mem,10464,*acc);struct.pack_into('>f',mem,1600+22*4,q);struct.pack_into('>f',mem,1600+21*4,ii);expected=mem[:];b=ii if fn&2 else q
  if fd:
   for k in range(4):
    if not mask&(8>>k):continue
    a=vf[fs][k] if fs else (1. if k==3 else 0.)
    if fn&1:z=f32(acc[k]-f32(a*b)) if fn&4 else f32(acc[k]+f32(a*b))
    else:z=f32(a-b) if fn&4 else f32(a+b)
    struct.pack_into('>f',expected,1728+fd*16+k*4,z)
  iw=(0x12<<26)|((0x10|mask)<<21)|(fs<<11)|(fd<<6)|fn
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_one(C.byref(c),iw)==0;lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,bytes(mem));u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=200);assert bytes(u.mem_read(0x20000,len(mem)))==expected,(fn,n,fs,fd,mask);assert u.reg_read(UC_PPC_REG_1)==0x32000;extra+=1
for n in range(32):
 mem=rng.randbytes(0x3000);c=Context();lib.ppc_dynarec_init(C.byref(c),1);assert lib.ppc_dynarec_translate_one(C.byref(c),0x4a0003bf)==0;lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c));u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,mem);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000);u.emu_start(0x10000,0x11000,count=100);assert bytes(u.mem_read(0x20000,len(mem)))==mem;extra+=1
print('PASS',extra,'additional generated PPC Q/I and VWAITQ cases')
(out/'jit_vu_qi_r1268.json').write_text(json.dumps({'cases':extra,'errors':0,'scope':'Eight Q/I arithmetic operations and current synchronous-Q wait semantics; no VU micro JIT or full flag/pipeline model.'},indent=2))
