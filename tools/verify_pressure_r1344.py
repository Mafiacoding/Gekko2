"""Linked PPC full arena: reclaim cold owners, protect hot/prefix/foreign code,
suppress impossible retries, and recover after release. No Wii FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
word(mask_addr,(initial|1|8|8192|(1<<12)|(1<<19))&~(1<<17))
def counter(name,*args):
 hi=call(name,*args);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
def profile():
 dest=0x81760000;call('ee_jit_get_cache_profile',dest)
 return dict(zip(['lookups','hits','misses','collisions','stale','attempts','installed','failures','compile_tb','compile_samples'],struct.unpack('>10Q',u.mem_read(dest,80))))
def step(pc,n=8):
 word(state+off['pc'],pc|0x80000000);word(state+off['next_pc'],(pc|0x80000000)+4)
 assert call('ee_core_step_n',n)==n
program=struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8)
setup();pcs=[base+i*64 for i in range(128)]
for pc in pcs:u.mem_write(ram+pc,program);step(pc)
for n in range(300):step(pcs[-1])
hot=precise_native_for_pc(pcs[-1]|0x80000000);assert hot
used=call('ppc_code_cache_used');cap=call('ppc_code_cache_capacity')
hold=call('ppc_code_cache_alloc',cap-used-256-32);assert hold
u.mem_write(hold,b'foreign-code-must-survive')
newpc=base+0x20000;u.mem_write(ram+newpc,program)
before=profile();total[0]=0;active[0]=True
for n in range(128):step(newpc)
active[0]=False
result=profile();print('PRESSURE_REPLAY',json.dumps({'PPC_instructions':total[0],'before':before,'after':result,'used':call('ppc_code_cache_used')}))
assert reg2()==(128+300+128)*8
assert bytes(u.mem_read(hold,25))==b'foreign-code-must-survive'
if 'ee_jit_get_pressure_stat' in syms:
 assert precise_native_for_pc(newpc|0x80000000)
 assert result['installed']==before['installed']+1 and result['failures']==before['failures'],result
 assert counter('ee_jit_get_pressure_stat',1)>0
 assert precise_native_for_pc(pcs[-1]|0x80000000)==hot
 print('PASS cold reclaim, hot and foreign ownership, no failed arena allocation')
call('ppc_code_cache_release',hold);call('ee_jit_reset_stats_for_test')
assert call('ppc_code_cache_used')==0
if 'ee_jit_get_pressure_stat' not in syms:exit()
# Admission probes never consume capacity, alter the arena epoch or count
# allocation failures. The actual allocation is the oracle for fit.
for n in [0,1,32,64,1024,cap-32,cap,0xffffffff]:
 epoch=call('ppc_code_cache_generation');used=call('ppc_code_cache_used');fails=counter('ppc_code_cache_failures')
 fits=call('ppc_code_cache_can_alloc',n)
 assert call('ppc_code_cache_generation')==epoch and call('ppc_code_cache_used')==used
 assert counter('ppc_code_cache_failures')==fails
 ptr=call('ppc_code_cache_alloc',n);assert bool(ptr)==bool(fits),(n,fits,ptr)
 if ptr:call('ppc_code_cache_release',ptr)
 assert call('ppc_code_cache_used')==0
print('PASS nonallocating exact fit probes, epoch/capacity and overflow boundaries')
# No EE owners can relieve foreign arena exhaustion. Only one translation is
# attempted while the epoch stays unchanged. It is not an opcode rejection.
setup();hold=call('ppc_code_cache_alloc',cap-32);assert hold
before=counter('ppc_code_cache_failures');step(base)
first=profile()['attempts'];assert first==1
for n in range(64):step(base)
assert profile()['attempts']==first
assert counter('ppc_code_cache_failures')==before
assert counter('ee_jit_get_pressure_stat',2)>0
assert reg2()==65*8
call('ppc_code_cache_release',hold);step(base)
assert precise_native_for_pc(base|0x80000000) and reg2()==66*8
print('PASS impossible full arena: 65 exact grants, one translation, deferred retries and recovery')
# A cached scalar function remains callable while NEW translations are deferred.
setup();word(mask_addr,((initial|1|(1<<12))&~8));assert call('ee_jit_try_execute_one_at',state,base|0x80000000,(9<<26)|(2<<21)|(2<<16)|1)==1
assert call('ee_jit_get_cache_size')==1
word(mask_addr,(initial|1|8|8192|(1<<12)|(1<<19))&~(1<<17))
used=call('ppc_code_cache_used');hold=call('ppc_code_cache_alloc',cap-used-32);assert hold
for n in range(16):step(base)
assert reg2()==129 and call('ee_jit_get_cache_size')==1
assert counter('ee_jit_get_compile_attempt_count')==1
assert call('ee_jit_try_execute_one_at',state,base|0x80000000,(9<<26)|(2<<21)|(2<<16)|1)==1 and reg2()==130
assert call('ee_jit_try_execute_one_at',state,base|0x80000000,(9<<26)|(2<<21)|(2<<16)|7)==0 and reg2()==130
u.mem_write(ram+base,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|7]*8))
step(base);assert reg2()==186 and call('ee_jit_get_cache_size')==1
call('ppc_code_cache_release',hold);step(base)
assert reg2()==242 and precise_native_for_pc(base|0x80000000)
call('ee_jit_reset_stats_for_test');assert call('ppc_code_cache_used')==0
print('PASS warmed scalar code survives deferral, changed opcodes interpret and epoch release recovers')
# Prefix creation protects its full owner under pressure.
setup();step(base)
fn=precise_native_for_pc(base|0x80000000);assert fn
for pc in pcs[1:]:u.mem_write(ram+pc,program);step(pc)
used=call('ppc_code_cache_used');hold=call('ppc_code_cache_alloc',cap-used-128-32);assert hold
used=call('ppc_code_cache_used');word(syms['precise_active'],1)
assert call('ee_jit_try_execute_chain_fetched',state,8,(9<<26)|(2<<21)|(2<<16)|1)==0
assert call('ppc_code_cache_used')==used and precise_native_for_pc(base|0x80000000)==fn
word(syms['precise_active'],0)
print('PASS active native chain refuses nested compilation and preserves owner')
step(base,3)
assert precise_native_for_pc(base|0x80000000)==fn and reg2()==128*8+3
assert counter('ee_jit_get_budget_cache_stat',6)==1
call('ppc_code_cache_release',hold);call('ee_jit_reset_stats_for_test');assert call('ppc_code_cache_used')==0
print('PASS protected prefix owner, bounded grant and complete teardown')
