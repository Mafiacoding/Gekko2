#!/usr/bin/env python3
"""Linked integer branch/delay-slot ABI differential oracle, synthetic input."""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))

def branch_run(native,budget,kind,scenario,prefix=6):
 extension_setup();call('dma_init');clocks()
 call('gekko2_experimental_request',0,int(native));call('gekko2_experimental_apply_cold_boot')
 word(mask_address,flags if native else flags&~(1|8))
 xor=(28<<26)|(20<<21)|(21<<16)|(21<<11)|(19<<6)|9
 lq=(0x1e<<26)|(1<<21)|(20<<16)
 target=base+4*(prefix+2)
 if kind==0:branches=[(3<<26)|(target>>2),(2<<26)|(base>>2)]
 elif kind==1:branches=[(2<<21)|(2<<11)|9,(3<<21)|8] # JALR r2,r2 alias, JR r3
 elif kind==2:branches=[(4<<26)|(20<<21)|(20<<16)|1,(2<<26)|(base>>2)]
 else:branches=[(5<<26)|(20<<21)|(20<<16)|1,(2<<26)|(base>>2)]
 delay=12 if scenario==3 else lq
 program=[*([xor]*prefix),branches[0],delay,*([xor]*prefix),branches[1],delay]
 u.mem_write(ram+base,struct.pack('<'+str(len(program))+'I',*program))
 u.mem_write(state+16,struct.pack('>4I',0,0x300000,0,0))
 u.mem_write(state+32,struct.pack('>4I',0,target,0xeeeeeeee,0xffffffff))
 u.mem_write(state+48,struct.pack('>4I',0,base,0x12345678,0xffffffff))
 for r in (20,21):u.mem_write(state+r*16,struct.pack('>4I',0x12345678*r&0xffffffff,0x11111111*r&0xffffffff,0x22222222*r&0xffffffff,0x87654321))
 u.mem_write(ram+0x300000,bytes(range(16)))
 u.mem_write(state+31*16,struct.pack('>4I',0xabcdef01,0x76543210,0xdeadbeef,0xa5a5a5a5))
 snapshot=bytes(u.mem_read(state,size))
 # A syscall delay slot intentionally vectors; do not require 128 retirements.
 call('ee_core_step_n',128)
 u.mem_write(state,snapshot);clocks()
 if scenario==1:word(state+16+4,0x600000) # Delay-slot TLB exception, EPC/BD observable.
 if scenario==2:
  word(state+off['cop0']+11*4,107);word(state+off['cop0']+12*4,0x18001)
 before=stat(3);saved=[0xb1000000+n for n in range(18)]
 for r,v in zip(range(14,32),saved):u.reg_write(UC_PPC_REG_0+r,v)
 retired=call('ee_core_step_n',budget)
 assert [u.reg_read(UC_PPC_REG_0+r) for r in range(14,32)]==saved
 n=stat(3)-before;call('ee_timers_snapshot',0x817e0000)
 result=(retired,bytes(u.mem_read(state,size)),bytes(u.mem_read(0x817e0000,64)),
         bytes(u.mem_read(syms['g_ee_display_ticks'],8)),
         bytes(u.mem_read(syms['g_ee_display_phase'],4)),bytes(u.mem_read(syms['g_ee_irq'],8)))
 return result,n

checks=successors=0
for kind in range(4):
 for budget in (6,7,8,9,16,33,64):
  for scenario in range(4):
   reference,_=branch_run(False,budget,kind,scenario)
   actual,n=branch_run(True,budget,kind,scenario)
   assert actual==reference,(kind,budget,scenario)
   checks+=1;successors+=n
 print(f'PASS branch family {kind}, cumulative comparisons={checks} successors={successors}',flush=True)
assert successors>0,'integer branch native successors never executed'
print(f'PASS {checks} linked J/JAL/JR/JALR/BEQ/BNE, link alias, 128-bit delay slot, EPC/BD, IRQ and scheduler comparisons; successors {successors}')
