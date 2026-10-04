"""Independent precise IOP wrapper oracle with hostile EABI callbacks."""
from pathlib import Path
prefix=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction sequences')[0]
exec(compile(prefix,'verify_iop_blocks_internal.py','exec'))
ctx=0x81700000;words_ptr=ctx+64;native_ptr=ctx+128;mockst=0x81710000
prepare,retire,scalar=0x81720000,0x81720004,0x81720008
u.mem_write(prepare,b'\x60\0\0\0'*3)
retired=ticks=0;pattern=[];source=[];seen=[]
def hostile(uc,address,size,user):
 global retired,ticks
 if address not in [prepare,retire,scalar]:return
 st=uc.reg_read(UC_PPC_REG_3);pc=uc.reg_read(UC_PPC_REG_4);w=uc.reg_read(UC_PPC_REG_5)
 assert st==mockst
 result=0
 if address==prepare:
  n=(pc-ibase)//4;prev=uc.reg_read(UC_PPC_REG_6);assert w==source[n]
  if prev!=0xffffffff:assert prev==pc-4;retired+=1
  result=pattern[n];seen.append(n)
  if result: ticks+=1
  if result==1:
   nxt=int.from_bytes(u.mem_read(st+io['next_pc'],4),'big');word(st+io['pc'],nxt);word(st+io['next_pc'],(nxt+4)&0xffffffff)
 elif address==retire:retired+=1
 else:
  assert w==12;retired+=1;word(st+8,0xdeadbeef)
 lr=uc.reg_read(UC_PPC_REG_LR);sp=uc.reg_read(UC_PPC_REG_1)
 uc.mem_write(sp+4,struct.pack('>I',lr));uc.mem_write(sp+8,b'\xca\xfe\xba\xbe'*8)
 for r in [0]+list(range(3,13)):uc.reg_write(UC_PPC_REG_0+r,0xcafe0000+r)
 uc.reg_write(UC_PPC_REG_3,result);uc.reg_write(UC_PPC_REG_PC,lr)
h=u.hook_add(UC_HOOK_CODE,hostile);checks=0
for length in range(1,9):
 for budget in [0,1,length,8,16]:
  for stop in [-1,0,length-1]:
   for result in [0,2]:
    source=[(9<<26)|(2<<21)|(2<<16)|1]*length
    pattern=[1]*length
    if stop>=0:pattern[stop]=result
    call('iop_jit_reset_for_test');heap=0x81600000
    u.mem_write(words_ptr,struct.pack('>'+str(length)+'I',*source));u.mem_write(ctx,bytes(16))
    assert call('ppc_dynarec_init',ctx,(length*100+160+127)//128)==0
    assert call('ppc_dynarec_translate_iop_block' if 'ppc_dynarec_translate_iop_block' in syms else 'ppc_dynarec_translate_iop_resident_block',ctx,ibase,words_ptr,length,prepare,retire,scalar,native_ptr)==0
    assert int.from_bytes(u.mem_read(native_ptr,4),'big')==length
    fn=call('ppc_dynarec_finalize',ctx);assert fn
    u.mem_write(mockst,bytes(isize));word(mockst+io['pc'],ibase);word(mockst+io['next_pc'],ibase+4)
    for r in range(14,32):u.reg_write(UC_PPC_REG_0+r,0xabcd0000+r)
    initial_cr=u.reg_read(UC_PPC_REG_CR)
    u.reg_write(UC_PPC_REG_1,0x81780000);u.reg_write(UC_PPC_REG_3,mockst);u.reg_write(UC_PPC_REG_4,budget);u.reg_write(UC_PPC_REG_LR,0x817ff000)
    retired=ticks=0;seen=[];u.emu_start(fn,0x817ff000,count=10000)
    want=0;body=0
    for n in range(min(length,budget)):
     if not pattern[n]:break
     want+=1
     if pattern[n]!=1:break
     body+=1
    assert u.reg_read(UC_PPC_REG_PC)==0x817ff000
    assert u.reg_read(UC_PPC_REG_3)==want==ticks and retired==body,(length,budget,stop,result,ticks,retired,want,body)
    assert int.from_bytes(u.mem_read(mockst+8,4),'big')==body
    assert u.reg_read(UC_PPC_REG_1)==0x81780000
    for r in range(14,32):assert u.reg_read(UC_PPC_REG_0+r)==0xabcd0000+r,(r,length,budget)
    # CR2..4 are callee-saved; volatile fields may change.
    assert u.reg_read(UC_PPC_REG_CR)&0x00fff000==initial_cr&0x00fff000
    checks+=1
for budget in range(5):
 source=[(9<<26)|(2<<21)|(2<<16)|1,12,(9<<26)|(2<<21)|(2<<16)|1];pattern=[1]*3
 heap=0x81600000;u.mem_write(words_ptr,struct.pack('>3I',*source));u.mem_write(ctx,bytes(16))
 assert call('ppc_dynarec_init',ctx,5)==0
 assert call('ppc_dynarec_translate_iop_block' if 'ppc_dynarec_translate_iop_block' in syms else 'ppc_dynarec_translate_iop_resident_block',ctx,ibase,words_ptr,3,prepare,retire,scalar,native_ptr)==0
 assert int.from_bytes(u.mem_read(native_ptr,4),'big')==1
 fn=call('ppc_dynarec_finalize',ctx);u.mem_write(mockst,bytes(isize));word(mockst+io['next_pc'],ibase+4)
 u.reg_write(UC_PPC_REG_1,0x81780000);u.reg_write(UC_PPC_REG_3,mockst);u.reg_write(UC_PPC_REG_4,budget);u.reg_write(UC_PPC_REG_LR,0x817ff000)
 retired=ticks=0;seen=[];u.emu_start(fn,0x817ff000,count=10000)
 assert u.reg_read(UC_PPC_REG_3)==ticks==retired==min(budget,2)
 assert int.from_bytes(u.mem_read(mockst+8,4),'big')==([0,1,0xdeadbeef][min(budget,2)])
 checks+=1
u.hook_del(h)
print('PASS',checks,'independent IOP wrapper budget/early-exit/EABI oracles with volatile register and argument-area clobbering')
