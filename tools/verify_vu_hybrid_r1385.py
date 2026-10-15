#!/usr/bin/env python3
"""Execute production timed VU JIT blocks from the linked Wii ELF.
Platform allocation/cache maintenance uses the common fixture. Arithmetic,
source validation, emitted code, native lower helpers and retirement are real.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
definition='''typedef struct {uint32_t vf[32][4],vi[32],acc[4];uint8_t mem[16384],micro[16384];uint32_t pc,bd,bt,ed;uint64_t retired,unknown;vu_pipeline_t pipeline;vu_timing_t timing;vu_jit_timed_context jit;} context;'''
fields=['vf','vi','acc','mem','micro','pc','bd','bt','ed','retired','unknown','pipeline','timing','jit']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/recompiler/vu_jit.h"\n'+definition+'\nconst unsigned sizes[]={sizeof(context),'+','.join('offsetof(context,'+f+')' for f in fields)+',offsetof(vu_jit_timed_context,ran),offsetof(vu_jit_timed_context,stopped)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 size,*offsets=struct.unpack('>17I',raw.read_bytes());layout=dict(zip(fields,offsets[:14]));ranoff,stopoff=offsets[14:]
ctx=0x93000000;u.mem_map(ctx,0x10000)
def get(f,n=0):return int.from_bytes(u.mem_read(ctx+layout[f]+n*4,4),'big')
def put(f,n,v):word(ctx+layout[f]+n*4,v)
def reset():
 call('vu_jit_reset_for_test');setup();u.mem_write(ctx,bytes(size));put('vf',3,0x3f800000)
 # Disable AOT to force the dynamic backend; VU JIT remains on.
 word(syms['gekko2_optimization_mask'],4)
 values=[ctx+layout[f] for f in ['vf','vi','acc','mem','micro']]+[16383,16383]+[ctx+layout[f] for f in ['pc','bd','bt','ed','retired','unknown','pipeline','timing']]+[0,0]
 u.mem_write(ctx+layout['jit'],struct.pack('>17I',*values))
def pair(pc,upper,lower):u.mem_write(ctx+layout['micro']+pc,struct.pack('<II',lower,upper))
def block(budget):return call('vu_jit_try_timed_block',ctx+layout['jit'],budget)
NOP=0x2ff;LNOP=0x8000033c
add=(8<<21)|(2<<16)|(1<<11)|(3<<6)|40
query=(22<<25)|(4<<16)|15
reset();put('vf',4,0x3f800000);put('vf',8,0xbf800000);put('vi',16,0x20);put('vi',17,0xaaaa)
pair(0,add,query)
for i in range(1,5):pair(i*8,NOP,LNOP)
pair(40,NOP,query);pair(48,NOP|0x40000000,LNOP)
assert block(8)==6 and get('pc')==48 and get('vi',4)==1 and get('vi',17)==8 and get('vi',16)==0x61
assert get('retired',1)==6 and get('pipeline',1)==6
assert int.from_bytes(u.mem_read(syms['timed_block_count'],8),'big')==1
# Warm hits and a smaller budget must retire exactly the requested prefix.
put('pc',0,0);assert block(2)==2 and get('pc')==16
# Same owner/PC but different source: no stale operation may execute.
put('pc',0,0);pair(0,NOP,0x80000000|(7<<16)|(7<<11)|(7<<6)|0x30)
put('vi',7,3);assert block(2)==2 and get('vi',7)==6
# Dynamic arithmetic RAW dependency waits for issue+4, rather than blindly
# advancing by the block's instruction count.
reset();put('vf',4,0x3f800000);put('vf',8,0x3f800000)
pair(0,add,LNOP);pair(8,(8<<21)|(2<<16)|(3<<11)|(4<<6)|40,LNOP)
assert block(2)==2 and get('vf',16)==0x40400000 and get('pipeline',1)==5
# A pending Q operation publishes at its own cycle while inside a block.
reset();put('vi',22,0x3f800000)
# q_value/q_status/q_ready/cycle/q_pending layout comes from vu.h.
# Derive fields with target compiler rather than hardcoding its uint64 ABI.
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'p.c';obj=Path(d)/'p.o';raw=Path(d)/'p.bin'
 names=['q_value','q_ready','q_pending','cycle']
 src.write_text('#include <stddef.h>\n#include "core/hw/vu.h"\nconst unsigned a[]={'+','.join('offsetof(vu_pipeline_t,'+n+')' for n in names)+'};')
 subprocess.run([cc,'-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 po=dict(zip(names,struct.unpack('>4I',raw.read_bytes())))
p=ctx+layout['pipeline'];word(p+po['q_value'],0x40000000);u.mem_write(p+po['q_ready'],struct.pack('>Q',1));u.mem_write(p+po['q_pending'],bytes([1]))
for i in range(3):pair(i*8,(8<<21)|(1<<11)|(3<<6)|32,LNOP)
assert block(3)==3 and get('vi',22)==0x40000000 and get('vf',12)==0x40000000
# Refuse pending branches/E and M boundaries without execution.
reset();pair(0,NOP,LNOP);pair(8,NOP,LNOP);put('bd',0,1)
assert block(2)==0 and get('retired',1)==0
put('bd',0,0);pair(8,NOP|0x20000000,LNOP);assert block(2)==0
# Per-pair source guard catches a change after block entry as well.
reset();pair(0,NOP,LNOP);pair(8,NOP,LNOP)
mutated=[False]
def mutation(uc,address,size,user):
 if address==syms['vu_jit_timed_pair'] and not mutated[0]:
  mutated[0]=True;pair(8,NOP,0x800007fc)
hook=u.hook_add(UC_HOOK_CODE,mutation)
assert block(2)==1 and get('pc')==8 and get('retired',1)==1
u.hook_del(hook)
# All callee saved registers and SP must survive emitted helper calls.
reset();pair(0,NOP,LNOP);pair(8,NOP,LNOP)
for n in range(14,32):u.reg_write(UC_PPC_REG_0+n,0xbeef0000+n)
assert block(2)==2
assert u.reg_read(UC_PPC_REG_1)==0x81780000
for n in range(14,32):assert u.reg_read(UC_PPC_REG_0+n)==0xbeef0000+n
# Production VU0/VU1 runners must choose timed blocks and preserve E delay.
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'production.c';obj=Path(d)/'production.o';raw=Path(d)/'production.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\n#include "core/hw/vu.h"\nconst unsigned a[]={'+
  ','.join('offsetof(ee_state_t,'+f+')' for f in ['vu0_micro','vu0_vf','cop2_ctrl','vu0_running','vu0_instructions_executed'])+','+
  ','.join('offsetof(vu1_state_t,'+f+')' for f in ['micro','vf','tpc','running','instructions_executed'])+'};')
 subprocess.run([cc,'-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 prod=struct.unpack('>10I',raw.read_bytes())
for unit in [0,1]:
 reset()
 if unit==1:call('vu1_init');owner=call('vu1_get_state');offs=prod[5:]
 else:owner=state;offs=prod[:5]
 micro,vf,pc,running,retired=offs
 for i in range(5):u.mem_write(owner+micro+i*8,struct.pack('<II',LNOP,add if i==0 else NOP|(0x40000000 if i==3 else 0)))
 word(owner+vf+16,0x3f800000);word(owner+vf+32,0xbf800000)
 if unit==0:call('vu0_exec_micro',owner,0)
 else:call('vu1_exec_micro',0)
 assert int.from_bytes(u.mem_read(owner+retired,8),'big')==5
 assert int.from_bytes(u.mem_read(owner+running,4),'big')==0
 assert int.from_bytes(u.mem_read(syms['timed_block_count'],8),'big')>=1
 if unit==0:assert int.from_bytes(u.mem_read(owner+pc+26*4,4),'big')==40
 else:assert int.from_bytes(u.mem_read(owner+pc,4),'big')==40
print('PASS linked PPC timed hybrid VU blocks: FMAC/query latency, RAW, Q, budgets, warm source invalidation, mid-block invalidation, boundaries, ABI, production VU0/VU1')
