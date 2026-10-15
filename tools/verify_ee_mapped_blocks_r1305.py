"""Linked PPC live mapped RAM blocks; identical scalar/old/new state digest."""
from pathlib import Path
old=Path(__file__).with_name('verify_ee_memory_blocks_r1304.py').read_text()
old=old.replace('if enabled and guard:\n   before=','if enabled and guard and ("ee_core_block_memory_resolve" not in syms or not call("ee_core_block_memory_resolve",state,(op<<26)|(3<<21)|(2<<16))):\n   before=')
exec(compile(old,'verify_ee_memory_blocks_r1304.py','exec'))
mapped_digest=hashlib.sha256();resolved='ee_core_block_memory_resolve' in syms
# Each program combines both pages, signed offsets and dependent ALU ops.
def mapped_setup(vaddr=0x300000,mask=0,lo0=(0x300<<6)|6,lo1=(0x301<<6)|6,asid=7,global_bit=0):
 setup();u.mem_write(state+tlb+16,struct.pack('>4I',mask,(vaddr&~0x1fff)|asid,lo0|global_bit,lo1|global_bit))
 word(state+off['cop0']+10*4,7)
for op in ops:
 for address in [0x300000,0x301000,0xc0300000,0xc0301000,0xffff8000,0xffffc000]:
  for bits in [0x90abcdef,0x800080ff]:
   mask=0x6000 if address>=0xffff8000 else 0
   lo0=(0x300<<6)|6;lo1=((0x304 if mask else 0x301)<<6)|6
   mapped_setup(address if mask else address&~0x1fff,mask,lo0,lo1)
   before=executed();physical=0x304000 if address==0xffffc000 else (0x301000 if address&0x1000 and not mask else 0x300000)
   u.mem_write(ram+physical,bytes(range(32)));u.mem_write(state+48,struct.pack('>QQ',address+4,0))
   u.mem_write(state+32,struct.pack('>QQ',bits,0xaabbccddeeff0011))
   for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',((op<<26)|(3<<21)|(2<<16)|0xfffc) if n%2==0 else ((9<<26)|(4<<21)|(4<<16)|1)))
   assert call('ee_core_step_n',8)==8 and executed()==before+8
   if enabled and resolved:
    hi=call('ee_jit_get_block_retired');assert ((hi<<32)|u.reg_read(UC_PPC_REG_4))==8
   mapped_digest.update(bytes(u.mem_read(state,544))+bytes(u.mem_read(ram+physical,32)))
# Every live mapping is re-resolved before retirement. Change EntryLo after
# instruction one, and verify instruction two reads the new page.
if enabled and resolved:
 mapped_setup();u.mem_write(state+48,struct.pack('>QQ',0x300000,0))
 for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x23<<26)|(3<<21)|(2<<16)))
 u.mem_write(ram+0x300000,struct.pack('<I',11));u.mem_write(ram+0x302000,struct.pack('<I',22))
 seen=[0]
 def remap(uc,address,size,user):
  if address in {syms['ee_core_block_prepare_memory_resolved'],syms.get('ee_core_block_memory_boundary',0)} and seen[0]==0 and uc.reg_read(UC_PPC_REG_4)==base+4:
   seen[0]+=1
   if seen[0]==1:word(state+tlb+16+8,(0x302<<6)|6)
 hook=u.hook_add(UC_HOOK_CODE,remap);before=executed();assert call('ee_jit_try_execute_block',state,8)==8
 u.hook_del(hook);assert reg2()==22 and executed()==before+8
 # A revoked mapping must stop before memory or PC is changed for that op.
 for revoke in ['V','D','ASID','RAM','MMIO']:
  mapped_setup();u.mem_write(state+48,struct.pack('>QQ',0x300000,0));u.mem_write(state+32,struct.pack('>QQ',33,0))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x2b<<26)|(3<<21)|(2<<16)))
  seen=[0]
  def revoke_mapping(uc,address,size,user):
   if address in {syms['ee_core_block_prepare_memory_resolved'],syms.get('ee_core_block_memory_boundary',0)} and seen[0]==0 and uc.reg_read(UC_PPC_REG_4)==base+4:
    seen[0]+=1
    if seen[0]==1:
     if revoke=='V':word(state+tlb+24,(0x300<<6)|4)
     if revoke=='D':word(state+tlb+24,(0x300<<6)|2)
     if revoke=='ASID':word(state+tlb+20,0x300008)
     if revoke=='RAM':word(state+tlb+24,(0x400<<6)|6)
     if revoke=='MMIO':u.mem_write(state+48,struct.pack('>QQ',0x10000000,0))
  hook=u.hook_add(UC_HOOK_CODE,revoke_mapping);before=executed();assert call('ee_jit_try_execute_block',state,8)==1,revoke
  u.hook_del(hook);assert executed()==before+1 and int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==base+4
 # Mapped SW targets the next compiled word through a different VA.
 mapped_setup(lo0=(0x200<<6)|6);u.mem_write(state+48,struct.pack('>QQ',0x300004,0))
 replacement=(9<<26)|(2<<16)|77;u.mem_write(state+5*16,struct.pack('>QQ',replacement,0))
 u.mem_write(ram+base,struct.pack('<I',(0x2b<<26)|(3<<21)|(5<<16)))
 before=executed();assert call('ee_jit_try_execute_block',state,8)==1 and executed()==before+1
 assert bytes(u.mem_read(ram+base+4,4))==struct.pack('<I',replacement)
 assert call('ee_core_step_n',1)==1 and reg2()==77
 for boundary in range(1,9):
  mapped_setup();u.mem_write(state+48,struct.pack('>QQ',0x300000,0));u.mem_write(state+32,struct.pack('>QQ',0x90abcdef,0))
  u.mem_write(ram+0x300000,bytes(32))
  for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x2b<<26)|(3<<21)|(2<<16)|(n*4)))
  word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,boundary);word(state+off['cop0']+12*4,0x18001)
  before=executed();assert call('ee_jit_try_execute_block',state,8)==boundary and executed()==before+boundary
  assert bytes(u.mem_read(ram+0x300000,32))==struct.pack('<I',0x90abcdef)*boundary+bytes((8-boundary)*4)
  assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+4*boundary
 print('PASS mapped block TLB replacement + V/D/ASID/RAM/MMIO live revocation, aliased source mutation and eight store IRQ boundaries')
bench=[]
for iteration in range(5):
 mapped_setup();u.mem_write(state+48,struct.pack('>QQ',0x300000,0));u.mem_write(ram+0x300000,struct.pack('<I',0x90abcdef))
 for n in range(8):u.mem_write(ram+base+4*n,struct.pack('<I',(0x23<<26)|(3<<21)|(2<<16)))
 assert call('ee_core_step_n',8)==8
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;bench.append(total[0])
print('EE_MAPPED_BLOCK_SIGNATURE '+mapped_digest.hexdigest())
print('EE_MAPPED_BLOCK_BENCH '+json.dumps({'elf':Path(a.elf).name,'LW_8_warm_PPC':bench,'scope':'live TLB data and full retirement, mocked platform services; not Wii cycles/FPS'}))
print('PASS 96 mixed mapped-memory programs across KUSEG, KSEG2, kernel mirror and even/odd 4KB/16KB pages')
