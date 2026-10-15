"""Warm EE event chains versus precise execution, including source/timer exits."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
flags=(initial|1|8|(1<<20)|(1<<24)|(1<<19))&~(1<<27)
def stat(n):
 if 'ee_jit_get_event_chain_stat' not in syms:return 0
 hi=call('ee_jit_get_event_chain_stat',n);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
def reset_clocks():
 call('ee_timers_init');u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick')
def run(mode,budget,change,irq):
 word(mask,(flags|(1<<4)) if mode==2 else flags&~(1<<4) if mode==1 else flags&~(1<<24))
 setup();reset_clocks()
 program=[(9<<26)|(2<<21)|(2<<16)|1,(13<<26)|(2<<21)|(3<<16)|0x1234,(0<<26)|(3<<16)|(4<<11)|(2<<6), (0<<26)|(4<<21)|(2<<16)|(5<<11)|33]*32
 if 'event_program_override' in globals():
  program=event_program_override
  u.mem_write(state+16,struct.pack('>QQ',0x80300000,0))
  u.mem_write(ram+0x300000,struct.pack('<128I',*[0x12340000+n for n in range(128)]))
 u.mem_write(ram+base,struct.pack('<128I',*program));snapshot=bytes(u.mem_read(state,size))
 for _ in range(16):assert call('ee_core_step_n',8)==8
 u.mem_write(state,snapshot);reset_clocks()
 if globals().get("warm_event_thunk",False):
  assert call("ee_core_step_n",128)==128
  u.mem_write(state,snapshot);reset_clocks()
 if change:
  address=base+(32 if change==1 else 33)*4
  u.mem_write(ram+address,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
  if change==2:call('ee_jit_notify_physical_write',address,4)
 if irq:word(state+off['cop0']+11*4,100+irq);word(state+off['cop0']+12*4,0x18001)
 before=stat(2)
 saved=[0xa1000000+n for n in range(18)]
 for r,value in zip(range(14,32),saved):u.reg_write(UC_PPC_REG_0+r,value)
 saved_cr=u.reg_read(UC_PPC_REG_CR)&0x00fff000
 total[0]=0;active[0]=True
 assert call('ee_core_step_n',budget)==budget
 active[0]=False;cost=total[0]
 for r,value in zip(range(14,32),saved):assert u.reg_read(UC_PPC_REG_0+r)==value,("callee-saved",r)
 assert u.reg_read(UC_PPC_REG_CR)&0x00fff000==saved_cr
 selected=stat(2)-before
 call('ee_timers_snapshot',0x817e0000)
 result=bytes(u.mem_read(state,size))+bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
 return result,cost,selected
checks=0;costs={};selected=0
for budget in [1,2,7,8,9,16,17,31,32]:
 for change,irq in [(0,0)]:
  reference,_,_=run(0,budget,change,irq)
  for mode in [1,2]:
   got,cost,n=run(mode,budget,change,irq);assert got==reference,(budget,change,irq,mode,next(i for i,(a,b) in enumerate(zip(got,reference)) if a!=b))
   selected+=n;checks+=1
   if not change and not irq:costs[mode,budget]=cost
if 'ee_jit_get_event_chain_stat' in syms:assert selected>0,'event successors were never selected'
print('PASS',checks,'complete EE/timer/display/IRQ-state comparisons for unchanged ALU runs; budgets 1..32; no mutation or injected IRQ in this focused suite')
print('EVENT_CHAIN_SELECTED',selected)
print('WARM_COST',json.dumps({b:{'separate_windows':costs[1,b],'event_chain':costs[2,b]} for b in [8,16,32]},sort_keys=True))
