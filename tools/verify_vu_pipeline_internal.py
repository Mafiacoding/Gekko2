"""Linked PPC pipeline and pair-hazard independent oracles, JIT/Interpreter."""
from pathlib import Path
import struct,subprocess,tempfile
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import *
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0],'loader','exec'))
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 d=Path(d);fields=['vf','vi','acc','mem','micro','tpc','branch_delay','branch_target','ebit_delay','instructions_executed','unimplemented_opcodes_seen','pipeline']
 (d/'s.c').write_text('#include <stddef.h>\n#include "core/hw/vu.h"\n#include "core/ee/ee_core.h"\nconst unsigned offsets[]={'+','.join('offsetof(vu1_state_t,'+f+')' for f in fields)+',offsetof(ee_state_t,cop2_ctrl)};')
 subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-gcc')),'-O2','-msdata=none','-I'+str(r/'include'),'-c',str(d/'s.c'),'-o',str(d/'s.o')],check=True)
 subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(d/'s.o'),str(d/'s.bin')],check=True)
 raw_offsets=struct.unpack('>13I',(d/'s.bin').read_bytes())
 offsets=dict(zip(fields,raw_offsets));ee_ctrl_offset=raw_offsets[-1]
u.reg_write(UC_PPC_REG_MSR,0x2000);heap=[0x81600000]
def hook(uc,address,size,data):
 if address==syms.get('memalign'):
  alignment=uc.reg_read(UC_PPC_REG_3);length=uc.reg_read(UC_PPC_REG_4);ptr=(heap[0]+alignment-1)&-alignment;heap[0]=ptr+length
  assert heap[0]<0x81700000
  uc.reg_write(UC_PPC_REG_3,ptr);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
 elif address in [syms.get(x) for x in ['free','DCFlushRange','ICInvalidateRange']]:uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,hook);u.ctl_remove_cache(0x80000000,0x81800000)
vs=call('vu1_get_state');v={f:vs+x for f,x in offsets.items()}
def word(addr,value):u.mem_write(addr,struct.pack('>I',value))
def get(addr):return int.from_bytes(bytes(u.mem_read(addr,4)),'big')
def fbits(f):return struct.unpack('>I',struct.pack('>f',f))[0]
def up(dst,fs,ft,mask,fn):return mask<<21|ft<<16|fs<<11|dst<<6|fn
def lo(sub,fs,ft,fields,bc):return 0x80000000|fields<<21|ft<<16|fs<<11|sub<<6|60|bc
NOP=up(11,0,0,0,63)
def reset():call('vu1_init');word(v['vi']+22*4,fbits(11))
def step(upper,lower):
 pc=get(v['tpc']);u.mem_write(v['micro']+pc,struct.pack('<2I',lower,upper))
 return call_step()
def call_step():
 # 8 GPR parameters, followed by 5 stack parameters.
 for n,f in enumerate(['branch_delay','branch_target','ebit_delay','instructions_executed','unimplemented_opcodes_seen','pipeline']):word(0x81780008+n*4,v[f])
 return call('vu_micro_step_pipeline',v['vf'],v['vi'],v['acc'],v['mem'],16383,v['micro'],16383,v['tpc'])
checks=0
reset();word(v['vi']+84,fbits(3));word(v['vf']+16,fbits(2))
step(up(2,1,0,8,34)|0x80000000,fbits(4));assert get(v['vf']+32)==fbits(5) and get(v['vi']+84)==fbits(4)
step(up(2,1,0,8,34),0);assert get(v['vf']+32)==fbits(6);checks+=1
for fsf in range(4):
 for ftf in range(4):
  reset()
  for n in range(4):word(v['vf']+16+n*4,fbits(12+n*4));word(v['vf']+32+n*4,fbits(2+n))
  step(NOP,lo(14,1,2,fsf|ftf<<2,0));assert get(v['vi']+88)==fbits(11)
  for n in range(6):step(NOP,0);assert get(v['vi']+88)==fbits(11)
  step(up(3,0,0,8,32),lo(14,0,0,0,3));expected=fbits((12+fsf*4)/(2+ftf))
  assert get(v['vi']+88)==expected and get(v['vf']+48)==expected;checks+=1
reset();word(v['vf']+16,fbits(2));word(v['vf']+32,fbits(3));word(v['vf']+48,fbits(99))
step(up(3,1,2,8,40),lo(12,3,4,8,0));assert get(v['vf']+48)==fbits(5) and get(v['vf']+64)==fbits(99);checks+=1
reset();word(v['vf']+16,fbits(2));word(v['vf']+32,fbits(3));word(v['vi']+20,7)
step(up(3,1,2,8,40),lo(13,5,3,15,0));assert get(v['vf']+48)==fbits(5) and get(v['vi']+20)==7;checks+=1
subs=[28,28,28,28,29,29,29,30,30,30,31,31,31];bcs=[0,1,2,3,0,1,2,0,1,2,0,1,2]
answers=[25,.04,5,.2,0,0,16,3,1/3,1/9,0,0.785398185253143,1]
for k in range(13):
 reset();word(v['vi']+92,fbits(99));word(v['vf']+16,fbits(0 if k in (4,5) else 3));word(v['vf']+20,fbits(4));word(v['vf']+28,fbits(0 if k>=10 else 9))
 step(NOP,lo(subs[k],1,0,3,bcs[k]));assert get(v['vi']+92)==fbits(99)
 step(NOP,lo(30,0,0,0,3));assert get(v['vi']+92)==fbits(answers[k]),(k,hex(get(v['vi']+92)),hex(fbits(answers[k])))
 step(NOP,lo(25,0,5,8,0));assert get(v['vf']+80)==fbits(answers[k]);checks+=1
print('PASS',checks,'linked PPC VU Q/P/EFU and dual-issue hazard oracles',Path(a.elf).name)

# The 48 D/T cases assert constants independently of the interpreter.
shared_ctrl=call('ee_core_get_state')+ee_ctrl_offset
for unit in range(2):
 for gates in range(4):
  for marked in range(1,4):
   for e in range(2):
    reset();call('ee_intc_init');shift=8 if unit else 0
    ctrl=shared_ctrl if unit else v['vi']
    word(ctrl+28*4,gates<<(shift+2));word(ctrl+29*4,0x80000000)
    if unit:word(v['vi']+28*4,12)
    word(v['vi']+4,7);word(v['vi']+8,9)
    word(v['vf']+16,fbits(2));word(v['vf']+32,fbits(3))
    upper=up(3,1,2,8,40)|(0x10000000 if marked&1 else 0)|(0x08000000 if marked&2 else 0)|(0x40000000 if e else 0)
    lower=0x80000000|2<<16|1<<11|3<<6|48
    u.mem_write(v['micro'],struct.pack('<2I',lower,upper))
    for n,f in enumerate(['branch_delay','branch_target','ebit_delay','instructions_executed','unimplemented_opcodes_seen','pipeline']):word(0x81780008+n*4,v[f])
    mask=16383 if unit else 4095
    stop=call('vu_micro_step_pipeline',v['vf'],v['vi'],v['acc'],v['mem'],mask,v['micro'],mask,v['tpc'])
    enabled=gates&marked
    assert stop==bool(enabled) and get(v['tpc'])==8 and get(v['instructions_executed']+4)==1
    assert get(v['vf']+48)==fbits(5) and get(v['vi']+12)==16
    assert get(ctrl+29*4)==(0x80000000|enabled<<(shift+1))
    assert get(call('ee_intc_get_state'))==(1<<(7 if unit else 6) if enabled else 0)
    assert get(syms['g_raise_count']+(7 if unit else 6)*4)==bool(enabled&1)+bool(enabled&2)
    assert get(v['ebit_delay'])==(0 if enabled else e)
print('PASS 48 linked PPC VU D/T shared FBRST, trap priority and full-pair oracles')

# Native block admission/cache checks use this focused loader, avoiding
# historical nested IOP fixtures whose JAL rejection assumption is obsolete.
def put(at,l,h):u.mem_write(v['micro']+at,struct.pack('<2I',l,h))
def setup_block():
 reset()
 for k in range(4):word(v['vf']+16+k*4,fbits(1));word(v['vf']+32+k*4,fbits(2))
 word(v['vi']+4,7);word(v['vi']+8,9)
 low=0x80000000|(2<<16)|(1<<11)|(3<<6)|48
 put(0,low,up(3,1,2,15,40));put(8,low,up(4,3,2,15,42));put(16,0,NOP|0x40000000)
 return low
def block(budget=8,data=None):
 for n,x in enumerate([v['branch_delay'],v['branch_target'],v['ebit_delay'],v['instructions_executed'],budget]):word(0x81780008+n*4,x)
 return call('vu_jit_try_block',v['vf'],v['vi'],v['acc'],v['mem'] if data is None else data,16383,v['micro'],16383,v['tpc'])
enabled='Interpreter' not in a.elf
low=setup_block();assert block()==(2 if enabled else 0)
if enabled:
 assert get(v['vf']+64)==fbits(6) and get(v['tpc'])==16
 before=heap[0];setup_block();assert block()==2 and heap[0]==before
 setup_block();put(8,low,up(4,3,2,15,44));assert block()==2 and get(v['vf']+64)==fbits(1)
 setup_block();assert block(1)==0 and get(v['tpc'])==0
 setup_block();assert block(data=v['micro'])==0
 # Known VF read hazard declines before any first-pair effect.
 setup_block();put(0,lo(12,3,4,15,0),up(3,1,2,15,40));assert block()==0 and get(v['vf']+48)==0
 # A straight native I-bit block reads old I in its first upper.
 setup_block();word(v['vi']+84,fbits(3));put(0,fbits(4),up(5,1,0,15,34)|0x80000000)
 put(8,low,up(6,1,0,15,34));assert block()==2
 assert get(v['vf']+80)==fbits(4) and get(v['vf']+96)==fbits(5)
 print('PASS 7 linked PPC native VU block cache/budget/alias/hazard/I-order groups')

reset();step(NOP,(32<<25)|2);assert get(v['tpc'])==8 and get(v['branch_delay'])==1
step(NOP,(32<<25)|3);assert get(v['tpc'])==24 and get(v['branch_delay'])==1 and get(v['branch_target'])==40
step(NOP,0);assert get(v['tpc'])==40 and get(v['branch_delay'])==0
reset();step(NOP,(32<<25)|2);word(v['vi']+4,1);word(v['vi']+8,2)
step(NOP,(40<<25)|(1<<11)|(2<<16)|3);assert get(v['tpc'])==24 and get(v['branch_delay'])==0
reset();step(NOP,(32<<25)|2);word(v['vi']+4,9)
step(NOP,(37<<25)|(1<<11)|(1<<16));assert get(v['tpc'])==24 and get(v['branch_target'])==72 and get(v['vi']+4)==4
step(NOP,0);assert get(v['tpc'])==72 and get(v['branch_delay'])==0
print('PASS 3 linked PPC nested VU branch/untaken/JALR independent control oracles')

if enabled:
 for opcode in [33,37]:
  for delay in [0,1,2]:
   for dst in [0,1,2]:
    reset();word(v['branch_delay'],delay);word(v['branch_target'],24)
    word(v['vi']+4,9);word(v['vi']+8,0xbeef)
    lower=(opcode<<25)|(dst<<16)|((1<<11) if opcode==37 else 2)
    assert call('vu_jit_try_lower',v['vf'],v['vi'],v['mem'],16383,lower,8,v['branch_delay'],v['branch_target'])==1
    assert get(v['branch_delay'])==2 and get(v['branch_target'])==(72 if opcode==37 else 32)
    if dst:assert get(v['vi']+dst*4)==(4 if delay==1 else 3)
    else:assert get(v['vi'])==0 and get(v['vi']+4)==9
 print('PASS 18 independent native PPC BAL/JALR normal/nested/alias/VI0 link oracles')
