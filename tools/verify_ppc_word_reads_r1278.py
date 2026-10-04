"""Check real Wii EE/IOP little-endian reads, all alignments, with byte oracle.
Includes inherited CPU, display, IRQ, VU, GS and frontend tests.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_display_phase_r1277.py').read_text(),'verify_ppc_display_phase_r1277.py','exec'))
from unicorn import UC_HOOK_CODE
import random,json
active=[False];total=[0]
def count_hook(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,count_hook);u.ctl_remove_cache(0x80000000,0x81800000)
word(ios+ioff['cop0']+12*4,0)
rng=random.Random(1278);measure={}
for label,fn,st,memory in [('EE','ee_mem_read32',state,ram),('IOP','iop_mem_read32',ios,io_ram)]:
 samples=[]
 for align in range(8):
  for n in range(32):
   raw=bytes(rng.randrange(256) for _ in range(4));u.mem_write(memory+0x7400+align,raw)
   total[0]=0;active[0]=True;value=call(fn,st,0x80007400+align);active[0]=False
   assert value==int.from_bytes(raw,'little'),(label,align,raw.hex(),value)
   if n==31:samples.append(total[0])
 measure[label]=samples
print('WORD_READ_MEASURE',json.dumps({'elf':Path(a.elf).name,'ppc_counts_by_alignment':measure,'checked_reads':512,'scope':'PPC instruction counts for mapped RAM reads only, not cycles or whole boot/FPS.'}))

wide=[]
from unicorn.ppc_const import UC_PPC_REG_4
for align in range(8):
 for n in range(32):
  raw=bytes(rng.randrange(256) for _ in range(8));u.mem_write(ram+0x7500+align,raw)
  total[0]=0;active[0]=True;hi=call('ee_mem_read64',state,0x80007500+align);active[0]=False
  value=(hi<<32)|u.reg_read(UC_PPC_REG_4)
  assert value==int.from_bytes(raw,'little'),(align,raw.hex(),hex(value))
  if n==31:wide.append(total[0])
print('WIDE_READ_MEASURE',json.dumps({'elf':Path(a.elf).name,'ppc_counts_by_alignment':wide,'checked_reads':256,'scope':'Mapped RAM reads, not whole boot or cycles/FPS.'}))

# One genuine LD from the GS CSR through its KSEG1 mirror, full CPU retirement.
call('gs_init');seed_clock(100);word(state+off['cop0']+12*4,0)
call('gs_mmio_read64',0x12001000,0x81741000)
expected=int.from_bytes(bytes(u.mem_read(0x81741000,8)),'big')
u.mem_write(state,bytes(512));u.mem_write(state+48,(0xb2001000).to_bytes(8,'big')+bytes(8))
word(state+off['pc'],0x80006800);word(state+off['next_pc'],0x80006804)
u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
guest(0x80006800,(55<<26)|(3<<21)|(2<<16))
assert call('ee_core_step_n',1)==1
assert int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==expected
assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006804
print('PASS actual Wii ELF LD CPU route preserves KSEG1 GS MMIO and retirement')
