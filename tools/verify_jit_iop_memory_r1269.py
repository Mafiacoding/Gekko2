"""Generated IOP integer memory helpers, endian/alignment/aliasing and ABI.
Mock helpers expose an independent byte-addressed RAM model and clobber every
volatile operand register, checking preservation across one/two real calls.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_jit_iop_control_r1269.py').read_text(),'verify_jit_iop_control_r1269.py','exec'))
from unicorn import UC_HOOK_CODE
u.mem_map(0x00f00000,4096)
ram=bytearray();access=[]
def hook(uc,addr,size,data):
 if not 0x00f00000<=addr<=0x00f00014:return
 index=(addr-0x00f00000)//4;st=uc.reg_read(UC_PPC_REG_3);ea=uc.reg_read(UC_PPC_REG_4);value=uc.reg_read(UC_PPC_REG_5);width=[1,2,4,1,2,4][index]
 assert st==0x20000
 off=ea&16383
 if index<3:value=int.from_bytes(bytes(ram[(off+k)&16383] for k in range(width)),'little');access.append(('read',ea,width))
 else:
  access.append(('write',ea,width,value))
  for k in range(width):ram[(off+k)&16383]=(value>>(8*k))&255
 for k in range(4,13):uc.reg_write(UC_PPC_REG_0+k,0xaabb0000+k)
 uc.reg_write(UC_PPC_REG_3,value if index<3 else 0xdeadbeef);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,hook);u.ctl_remove_cache(0x10000,0x12000)
mem_cases=0
for op in [0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2b,0x22,0x26,0x2a,0x2e]:
 for n in range(400):
  rs=n%32;rt=rs if n%3==0 else rng.randrange(32);imm=[-32768,-1,0,1,32767][n%5] if n<100 else rng.randrange(-32768,32768)
  words=[rng.getrandbits(32) for k in range(256)];words[0]=0
  if rs:words[rs]=[0,1,16381,16383,0x80000000,0x1f801070,0xffffffff][n%7] if n<200 else rng.getrandbits(32)
  ea=(words[rs]+imm)&mask;old=words[rt];expected=words[:];ram=bytearray(rng.randbytes(16384));wanted=ram[:];want_access=[];access=[]
  aligned=ea&~3;off=aligned&16383;memword=int.from_bytes(wanted[off:off+4],'little');k=ea&3
  if op in [0x22,0x26,0x2a,0x2e]:
   want_access.append(('read',aligned,4))
   if op==0x22:value=(old&[0xffffff,0xffff,0xff,0][k])|((memword<<[24,16,8,0][k])&mask)
   elif op==0x26:value=(old&[0,0xff000000,0xffff0000,0xffffff00][k])|(memword>>[0,8,16,24][k])
   elif op==0x2a:value=(memword&[0xffffff00,0xffff0000,0xff000000,0][k])|(old>>[24,16,8,0][k])
   else:value=(memword&[0,0xff,0xffff,0xffffff][k])|((old<<[0,8,16,24][k])&mask)
   if op in [0x2a,0x2e]:wanted[off:off+4]=value.to_bytes(4,'little');want_access.append(('write',aligned,4,value))
   elif rt:expected[rt]=value
  else:
   width=1 if op in [0x20,0x24,0x28] else 2 if op in [0x21,0x25,0x29] else 4
   off=ea&16383
   if op>=0x28:
    value=old&((1<<(8*width))-1);want_access.append(('write',ea,width,value))
    for j in range(width):wanted[(off+j)&16383]=(value>>(8*j))&255
   else:
    value=int.from_bytes(bytes(wanted[(off+j)&16383] for j in range(width)),'little');want_access.append(('read',ea,width))
    if op in [0x20,0x21] and value&(1<<(8*width-1)):value=(value-(1<<(8*width)))&mask
    if rt:expected[rt]=value
  iw=(op<<26)|(rs<<21)|(rt<<16)|(imm&65535)
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0;assert lib.ppc_dynarec_translate_iop_one(C.byref(c),iw)==0
  lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>256I',*words))
  for j in range(32):u.reg_write(UC_PPC_REG_0+j,0xabba0000+j)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=500);got=struct.unpack('>256I',u.mem_read(0x20000,1024))
  assert got==tuple(expected),(op,n,hex(ea),'state');assert ram==wanted,(op,n,'RAM');assert access==want_access,(op,n,access,want_access)
  assert u.reg_read(UC_PPC_REG_1)==0x32000
  for j in range(14,32):assert u.reg_read(UC_PPC_REG_0+j)==0xabba0000+j,(op,n,j)
  mem_cases+=1
print('PASS',mem_cases,'generated IOP memory cases across all 12 forms, including rt0/MMIO-side-effect calls and aliased base/data')
(out/'jit_iop_memory_r1269.json').write_text(json.dumps({'cases':mem_cases,'encodings':12,'errors':0,'scope':'Real helper ABI with independent RAM/access oracle. Helper internals and actual MMIO covered by ELF integration.'},indent=2))
