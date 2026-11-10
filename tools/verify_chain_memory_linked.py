#!/usr/bin/env python3
"""Execute linked LQ/SQ/LD/SD native companions against the EE interpreter.
No BIOS or game input; platform/cache services use the existing PPC fixture.
"""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))

def memory_run(native,budget,scenario,alias=0):
 extension_setup();call('dma_init')
 call('gekko2_experimental_request',0,int(native));call('gekko2_experimental_apply_cold_boot')
 word(mask_address,flags if native else flags&~(1|8));clocks()
 # Loads feed MMI across block boundaries; stores are observable RAM effects.
 # HI/LO and SA alternate with four-lane arithmetic, never a synthetic shortcut.
 lq=(0x1e<<26)|(1<<21)|((1 if alias==1 else 20)<<16)
 sq=(0x1f<<26)|(1<<21)|(21<<16)|32
 ld=(0x37<<26)|(1<<21)|((1 if alias==2 else 22)<<16)|16
 sd=(0x3f<<26)|(1<<21)|(22<<16)|48
 xor=(28<<26)|(20<<21)|(21<<16)|(21<<11)|(19<<6)|9
 program=([lq,xor,sq,ld,sd,(20<<21)|17,(21<<21)|19,(22<<21)|0x29]*16)
 u.mem_write(ram+base,struct.pack('<128I',*program))
 payload=bytes((i*37+19)&255 for i in range(256))
 if alias:
  payload=struct.pack('<4I',0x300000,0x11223344,0xdeadbeef,0xa5a5a5a5)+struct.pack('<2I',0x300000,0x12345678)+payload[24:]
 u.mem_write(ram+0x300000,payload)
 u.mem_write(state+16,struct.pack('>4I',0,0x300000,0,0))
 for r in (20,21,22):u.mem_write(state+r*16,struct.pack('>4I',0x12345678*r&0xffffffff,0x11111111*r&0xffffffff,0x22222222*r&0xffffffff,0x87654321))
 for bank in (512,528):u.mem_write(state+bank,struct.pack('>4I',0xabcdef01,0x76543210,0xdeadbeef,0xa5a5a5a5))
 word(state+552,7)
 snapshot=bytes(u.mem_read(state,size))
 assert call('ee_core_step_n',128)==128
 u.mem_write(state,snapshot);u.mem_write(ram+0x300000,payload);clocks()
 if scenario==1:call('ee_jit_notify_mapping_change')
 if scenario==2:
  word(state+off['cop0']+11*4,116);word(state+off['cop0']+12*4,0x18001)
 if scenario==3: # Data TLB miss: warmed companion must decline, scalar raises.
  word(state+16+4,0x600000)
 if scenario==4: # Source invalidation after compilation.
  u.mem_write(ram+base+20,struct.pack('<I',(9<<26)|(22<<21)|(22<<16)|7))
  call('ee_jit_notify_physical_write',base+20,4)
 if scenario==5:word(state+16+4,base) # SQ modifies a future compiled block.
 before=stat(3);saved=[0xa1000000+n for n in range(18)]
 for r,v in zip(range(14,32),saved):u.reg_write(UC_PPC_REG_0+r,v)
 assert call('ee_core_step_n',budget)==budget
 assert [u.reg_read(UC_PPC_REG_0+r) for r in range(14,32)]==saved
 successors=stat(3)-before
 call('ee_timers_snapshot',0x817e0000)
 result=(bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+0x300000,256))+bytes(u.mem_read(ram+base,512))+
         bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+
         bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8)))
 return result,successors

checks=successors=0
for budget in (1,2,7,8,9,16,17,31,32,33,64,128):
 for scenario in range(6):
  reference,_=memory_run(False,budget,scenario)
  actual,n=memory_run(True,budget,scenario)
  assert actual==reference,(budget,scenario,next(i for i,(a,b) in enumerate(zip(actual,reference)) if a!=b))
  checks+=1;successors+=n
assert successors>0,'RAM companion successors never executed'
print(f'PASS {checks} linked 128-bit LQ/SQ, LD/SD, HI/LO/SA, RAM, budget, IRQ, mapping, TLB-fault and SMC comparisons; successors {successors}')
