"""R3000A native control/divide: runtime PC, signed bounds, zero, overflow."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_jit_iop_r1268.py').read_text(),'verify_jit_iop_r1268.py','exec'))
new_cases=0
ops2=[('branch',n) for n in [4,5,6,7]]+[('regimm',n) for n in [0,1,0x10,0x11]]+[('jump',2),('jump',3),('special',8),('special',9),('special',0x1a),('special',0x1b)]
for group,fn in ops2:
 for n in range(1000):
  rs=n%32;rt=rs if n%5==0 else rng.randrange(32);imm=[-32768,-1,0,1,32767][n%5] if n<200 else rng.randrange(-32768,32768)
  words=[rng.getrandbits(32) for k in range(256)];words[0]=0
  if rs:words[rs]=edges[n%len(edges)] if n<300 else rng.getrandbits(32)
  if rt:words[rt]=edges[(n//len(edges))%len(edges)] if n<300 else rng.getrandbits(32)
  pc=[0,0x80002000,0xbfc00100,0x0ffffffc,0xfffffffc][n%5] if n<300 else rng.getrandbits(32)&~3
  a=words[rs];b=words[rt];expected=words[:]
  if group=='branch':
   iw=(fn<<26)|(rs<<21)|(rt<<16)|(imm&65535)
   taken={4:a==b,5:a!=b,6:signed(a)<=0,7:signed(a)>0}[fn]
   if taken:expected[33]=(pc+4+imm*4)&mask
  elif group=='regimm':
   iw=(1<<26)|(rs<<21)|(fn<<16)|(imm&65535)
   if fn>=0x10:expected[31]=(pc+8)&mask
   a=expected[rs]
   if (signed(a)<0)==((fn&1)==0):expected[33]=(pc+4+imm*4)&mask
  elif group=='jump':
   iw=(fn<<26)|rng.randrange(1<<26);expected[33]=(pc&0xf0000000)|((iw&0x3ffffff)<<2)
   if fn==3:expected[31]=(pc+8)&mask
  else:
   rd=rs if n%3==0 else rng.randrange(32)
   iw=(rs<<21)|(rt<<16)|fn|((rd<<11) if fn==9 else 0)
   if fn in [8,9]:
    expected[33]=a
    if fn==9 and rd:expected[rd]=(pc+8)&mask
   else:
    if not b:q=1 if fn==0x1a and signed(a)<0 else mask;rem=a
    elif fn==0x1a:
     aa=signed(a);bb=signed(b);q=(abs(aa)//abs(bb))*(-1 if (aa<0)!=(bb<0) else 1);rem=aa-q*bb
    else:q=a//b;rem=a%b
    expected[34]=rem&mask;expected[35]=q&mask
  c=Context();assert lib.ppc_dynarec_init(C.byref(c),1)==0
  assert lib.ppc_dynarec_translate_iop_one(C.byref(c),iw)==0,(group,fn,hex(iw))
  lib.ppc_dynarec_finalize(C.byref(c));code=struct.pack('>'+str(c.used)+'I',*c.code[:c.used]);lib.ppc_dynarec_free(C.byref(c))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>256I',*words))
  for k in range(32):u.reg_write(UC_PPC_REG_0+k,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_4,pc);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=300);got=struct.unpack('>256I',u.mem_read(0x20000,1024))
  assert tuple(expected)==got,(group,fn,n,hex(a),hex(b),hex(pc),[(j,hex(x),hex(y)) for j,(x,y) in enumerate(zip(expected,got)) if x!=y]);new_cases+=1
print('PASS',new_cases,'new PPC IOP control/divide cases; existing',cases,'scalar cases retained')
(out/'jit_iop_control_r1269.json').write_text(json.dumps({'new_cases':new_cases,'existing_cases':cases,'errors':0,'new_encodings':14,'scope':'Branches/J/JR/JAL/JALR/REGIMM links preserve current interpreter conventions, including aliased link operands. Retail ROM HLE admission is separately guarded by the core. DIV zero/overflow follows primary PCSX2 R3000A semantics.'},indent=2))
