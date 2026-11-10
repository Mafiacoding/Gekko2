#!/usr/bin/env python3
"""Actual PPC EE windows vs precise retirement with stable masked IRQ levels.
Synthetic RAM; platform services modeled. Instruction counts are not Wii cycles.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
flags=(initial|1|8|(1<<20)|(1<<19))&~((1<<4)|(1<<27)|(1<<25))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'layout.c';obj=Path(td)/'layout.o';raw=Path(td)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/hw/dma.h"\nconst unsigned data[]={offsetof(dma_state_t,d_ctrl),offsetof(dma_state_t,d_stat)};')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-Iinclude','-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 ctrl,stat=struct.unpack('>2I',raw.read_bytes())
def window_slots():
 hi=call('ee_core_native_window_stat',1);return hi<<32|u.reg_read(UC_PPC_REG_4)
def clocks():
 call('ee_timers_init');u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',100000));word(syms['g_ee_display_phase'],100000);call('ee_timers_tick')
 u.mem_write(syms['g_ee_irq'],bytes(8))
def run(mode,budget,intc,dmac,status):
 setup();word(mask,flags|((1<<24) if mode else 0));clocks()
 program=[(9<<26)|(2<<21)|(2<<16)|1]*32
 u.mem_write(ram+base,struct.pack('<32I',*program));saved=bytes(u.mem_read(state,size))
 assert call('ee_core_step_n',32)==32 # warm native code, outside cost scope
 u.mem_write(state,saved);clocks();word(state+off['cop0']+12*4,status)
 if intc:
  assert call('ee_intc_mmio_write32',0x1000f010,1)==1;call('ee_intc_raise',0)
  u.mem_write(syms['g_ee_irq'],struct.pack('>2I',0,1))
 dma=syms['gekko2_dma_live'];word(dma+ctrl,1);word(dma+stat,0x10001 if dmac else 0)
 before=window_slots();total[0]=0;active[0]=True
 assert call('ee_core_step_n',budget)==budget
 active[0]=False;cost=total[0];admitted=window_slots()-before
 call('ee_timers_snapshot',0x817e0000)
 signature=bytes(u.mem_read(state,size))+bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
 return signature,cost,admitted
rows=[]
for budget in [8,32]:
 for intc,dmac,status in [(1,0,0),(0,1,0),(1,1,0x10001),(1,0,0x10801),(0,1,0x10401),(1,1,0x10c03)]:
  before,old_cost,_=run(0,budget,intc,dmac,status)
  after,new_cost,admitted=run(1,budget,intc,dmac,status)
  assert before==after,('complete EE/timer/IRQ state',budget,intc,dmac,status,next(i for i,(a,b) in enumerate(zip(before,after)) if a!=b))
  assert admitted>0,('new masked-level admission unused',budget,intc,dmac,status)
  rows.append(dict(slots=budget,intc=intc,dmac=dmac,status=hex(status),precise_ppc_instructions=old_cost,event_ppc_instructions=new_cost,event_slots=admitted))
print('PASS 12 complete linked PPC EE/timer/display/IRQ comparisons with actual masked-level native execution')
print('MASKED_WINDOW_COST '+json.dumps(rows,sort_keys=True))
