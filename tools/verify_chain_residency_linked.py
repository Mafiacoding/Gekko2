#!/usr/bin/env python3
"""Real linked dispatcher/retirement + generated native ABI vs interpreter."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
assert 'precise_resident_map' in syms, 'requires explicit chain-residency ELF'
mask_address=syms['gekko2_optimization_mask']
initial=int.from_bytes(u.mem_read(mask_address,4),'big')
flags=(initial|1|8|(1<<4)|(1<<5))&~(1<<24)

def stat(n):
 hi=call('ee_jit_get_chain_residency_stat',n)
 return (hi<<32)|u.reg_read(UC_PPC_REG_4)

def clocks():
 call('ee_timers_init');u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000))
 word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick')

def run(native,budget,change,irq,registers=(4,5,6)):
 extension_setup()
 call('dma_init')
 word(mask_address,flags if native else flags&~(1|8))
 clocks()
 # PAND/PXOR/POR/PNOR and scalar dependent updates; only synthetic words.
 program=[]
 for n in range(128):
  kind=n&3;program.append((28<<26)|(registers[0]<<21)|(registers[1]<<16)|(registers[2]<<11)|((18+(kind&1))<<6)|(9 if kind<2 else 41))
 u.mem_write(ram+base,struct.pack('<128I',*program))
 for r in registers:u.mem_write(state+r*16,struct.pack('>4I',0x12345678*r&0xffffffff,0x11111111*r&0xffffffff,0x22222222*r&0xffffffff,0x87654321))
 snapshot=bytes(u.mem_read(state,size))
 assert call('ee_core_step_n',128)==128
 u.mem_write(state,snapshot);clocks()
 if change:
  address=base+(0 if change==1 else 5)*4
  u.mem_write(ram+address,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|9))
  if change==3:call('ee_jit_notify_physical_write',address,4)
 if change==4:call('ee_jit_notify_mapping_change')
 if change==5:
  # Actual FromSPR DMA overwrites an interior compiled source quadword.
  payload=[(9<<26)|(registers[0]<<21)|(registers[0]<<16)|9,*program[5:8]]
  u.mem_write(ram+0x300000,struct.pack('<4I',*payload))
  call('dma_bind_ee_ram',ram,0x400000);call('dma_bind_scratchpad',ram+0x300000,16384)
  call('dma_set_ee_write_notify',syms['ee_jit_notify_physical_write'])
  for addr,value in [(0x1000d010,base+16),(0x1000d020,1),(0x1000d080,0),(0x1000d000,0x100)]:
   call('dma_mmio_write32',addr,value)
  assert bytes(u.mem_read(ram+base+16,16))==struct.pack('<4I',*payload)
 if irq:word(state+off['cop0']+11*4,100+irq);word(state+off['cop0']+12*4,0x18001)
 before=stat(3)
 saved=[0xa1000000+n for n in range(18)]
 for r,v in zip(range(14,32),saved):u.reg_write(UC_PPC_REG_0+r,v)
 saved_cr=u.reg_read(UC_PPC_REG_CR)&0x00fff000
 assert call('ee_core_step_n',budget)==budget
 assert [u.reg_read(UC_PPC_REG_0+r) for r in range(14,32)]==saved
 assert u.reg_read(UC_PPC_REG_CR)&0x00fff000==saved_cr
 selected=stat(3)-before
 call('ee_timers_snapshot',0x817e0000)
 result=bytes(u.mem_read(state,size))+bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
 return result,selected

checks=selected=0
for budget in [1,2,7,8,9,16,17,31,32,33,63,64,65,127,128]:
 for change,irq in [(0,0),(1,0),(2,0),(3,0),(4,0),(0,1),(0,16)]:
  reference,_=run(False,budget,change,irq)
  actual,n=run(True,budget,change,irq)
  assert actual==reference,(budget,change,irq,next(i for i,(a,b) in enumerate(zip(actual,reference)) if a!=b))
  checks+=1;selected+=n
assert selected>0,'native-only successors never executed'
print('PASS',checks,'linked interpreter/native-residency complete EE/timer/display/IRQ comparisons; selected successors',selected)
