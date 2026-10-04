"""IOP resident pool against hostile callback linkage/volatile clobbers."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_iop_block_abi_internal.py').read_text(),'verify_iop_block_abi_internal.py','exec'))
def resident_hostile(uc,address,size,user):
 if address==prepare:
  n=(uc.reg_read(UC_PPC_REG_4)-ibase)//4
  if n==change:
   word(mockst+4,0x55);word(mockst+io['gpr_generation'],0xffffffff)
  if n==change+1:word(mockst+io['gpr_generation'],0)
 hostile(uc,address,size,user)
h=u.hook_add(UC_HOOK_CODE,resident_hostile)
resident_checks=0
for change in range(8):
 for budget in [0,1,2,7,8,16]:
  source=[(1<<21)|(2<<16)|(3<<11)|0x25]*8;pattern=[1]*8
  heap=0x81600000;u.mem_write(words_ptr,struct.pack('>8I',*source));u.mem_write(ctx,bytes(16))
  assert call('ppc_dynarec_init',ctx,10)==0
  assert call('ppc_dynarec_translate_iop_resident_block',ctx,ibase,words_ptr,8,prepare,retire,scalar,native_ptr)==0
  fn=call('ppc_dynarec_finalize',ctx);assert fn
  u.mem_write(mockst,bytes(isize));word(mockst+4,1);word(mockst+8,2);word(mockst+io['pc'],ibase);word(mockst+io['next_pc'],ibase+4);word(mockst+io['gpr_generation'],0xfffffffe)
  for r in range(14,32):u.reg_write(UC_PPC_REG_0+r,0xabcd0000+r)
  initial_cr=u.reg_read(UC_PPC_REG_CR);u.reg_write(UC_PPC_REG_1,0x81780000);u.reg_write(UC_PPC_REG_3,mockst);u.reg_write(UC_PPC_REG_4,budget);u.reg_write(UC_PPC_REG_LR,0x817ff000)
  retired=ticks=0;seen=[];u.emu_start(fn,0x817ff000,count=100000)
  assert u.reg_read(UC_PPC_REG_PC)==0x817ff000
  assert u.reg_read(UC_PPC_REG_3)==ticks==retired==min(8,budget)
  want=0 if not budget else 0x57 if min(8,budget)>change else 3
  assert int.from_bytes(u.mem_read(mockst+12,4),'big')==want,(change,budget)
  assert u.reg_read(UC_PPC_REG_1)==0x81780000
  for r in range(14,32):assert u.reg_read(UC_PPC_REG_0+r)==0xabcd0000+r,(change,budget,r)
  assert u.reg_read(UC_PPC_REG_CR)&0x00fff000==initial_cr&0x00fff000
  resident_checks+=1
u.hook_del(h)
print('PASS',resident_checks,'hostile resident-pool EABI, all nonvolatile registers, budgets and generation wrap oracles')
