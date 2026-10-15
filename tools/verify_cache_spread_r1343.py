"""Same guest trace on baseline/new ELF: old BIOS page aliases and source mutation."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
word(mask_addr,(initial|8192)&~(1<<17));setup()
pcs=[base];candidate=base+32
h=lambda pc:((pc>>2)^(pc>>12))&1023
while len(pcs)<8:
 if h(candidate)==h(base):pcs.append(candidate)
 candidate+=4
for pc in pcs:u.mem_write(ram+pc,struct.pack('<8I',*[(9<<26)|(2<<21)|(2<<16)|1]*8))
total[0]=0;active[0]=True
for n in range(128):
 pc=pcs[n%8]|0x80000000;word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
 assert call('ee_core_step_n',8)==8
active[0]=False;assert reg2()==1024 and executed()==1024
buf=0x81760000;call('ee_jit_get_cache_profile',buf)
profile=dict(zip(['lookups','hits','misses','collisions','stale','attempts','installed','failures','compile_tb','compile_samples'],struct.unpack('>10Q',u.mem_read(buf,80))))
print('SPREAD_REPLAY',json.dumps({'PPC_instructions':total[0],'retired':executed(),'guest_signature':hashlib.sha256(bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+base,0x20000))).hexdigest(),'profile':profile}))
for pc in pcs:
 call('ee_jit_notify_physical_write',pc,4);u.mem_write(ram+pc,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
 word(state+off['pc'],pc|0x80000000);word(state+off['next_pc'],(pc|0x80000000)+4);before=reg2()
 assert call('ee_core_step_n',8)==8 and reg2()==before+16
call('ee_jit_reset_stats_for_test')
print('PASS exact 1024 retirements, all eight owner mutations and teardown')
