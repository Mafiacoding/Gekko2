"""Actual emitted Wii JIT frontend integration, EE extensions and IOP IRQ boundary."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_cache_r1267.py').read_text(),'verify_ppc_cache_r1267.py','exec'))
from unicorn.ppc_const import UC_PPC_REG_MSR
reset_frontend()
u.reg_write(UC_PPC_REG_MSR,0x2000)
enabled='Interpreter' not in Path(a.elf).name
# Verify all new EE encodings pass the actual hardware-target frontend gate.
new=[(op<<26)|(1<<21)|(2<<16)|1 for op in [0x18,0x19]]
new += [(1<<21)|(2<<16)|(3<<11)|(17<<6)|fn for fn in [0x14,0x16,0x17,0x3c,0x3e,0x3f,0x20,0x22]]
new += [(0x1c<<26)|(1<<21)|(2<<16)|(3<<11)|fn for fn in [0x18,0x19,0x1a,0x1b]]
indices=list(range(16))+list(range(24,29))+[30]+list(range(32,43))+list(range(44,48))
new += [(0x12<<26)|(0x1f<<21)|(5<<16)|(4<<11)|((idx>>2)<<6)|0x3c|(idx&3) for idx in indices]
new += [(0x12<<26)|(0x1f<<21)|(4<<11)|(3<<6)|fn for fn in range(0x20,0x28)]
new += [0x4a0003bf]
for n,iw in enumerate(new):assert call('ee_jit_try_execute_one_at',state,0x280000+4*n,iw)==int(enabled),(n,hex(iw))
print('PASS 1 actual Wii ELF EE extension gate group:',len(new),'new integer/pipe1/VU0-ACC encodings')
# Compile the real target ABI layout, including pointer-dependent RAM fields.
import tempfile
with tempfile.TemporaryDirectory() as directory:
 src=Path(directory)/'layout.c';obj=Path(directory)/'layout.o';raw=Path(directory)/'layout.bin'
 fields=['ram','ram_size','halted','idle','instructions_executed','sched_ticks','pc','next_pc','cop0']
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),'+','.join('offsetof(iop_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-msdata=none','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
 size,*offsets=struct.unpack('>'+str(len(fields)+1)+'I',raw.read_bytes());ioff=dict(zip(fields,offsets))
ios=call('iop_core_get_state');u.mem_write(ios,bytes(size));io_ram=0x90600000;u.mem_map(io_ram,65536)
word(ios+ioff['ram'],io_ram);word(ios+ioff['ram_size'],65536)
good=(9<<26)|(2<<16)|7;bad=(3<<26)|0x4000
before=allocs
for n in range(1000):assert call('iop_jit_try_execute_one',ios,0x80002000,good)==int(enabled)
assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==(7 if enabled else 0)
assert allocs==before+int(enabled)
print('PASS 1 actual Wii ELF IOP hot dispatch group: 1000 calls, one allocation when enabled')
before=allocs
for n in range(1000):assert call('iop_jit_try_execute_one',ios,0x80002004,bad)==0
assert allocs==before+int(enabled)
assert call('iop_jit_try_execute_one',ios,0x80002004,good+2)==int(enabled)
assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==(9 if enabled else 0)
print('PASS 1 actual Wii ELF IOP rejection/word-replacement group')
# The real CPU step must still fetch, retire, advance PC and tick its scheduler.
call('iop_intc_init');call('iop_timers_init');word(ios+ioff['pc'],0x80002000);word(ios+ioff['next_pc'],0x80002004)
u.mem_write(io_ram+0x2000,struct.pack('<I',good));call('iop_core_step')
assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==7
assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==0x80002004
assert int.from_bytes(bytes(u.mem_read(ios+ioff['instructions_executed'],8)),'big')==1
assert int.from_bytes(bytes(u.mem_read(ios+ioff['sched_ticks'],8)),'big')==1
print('PASS 1 actual Wii ELF IOP CPU retirement/scheduler group')
# Declined memory operation reaches the existing real RAM helper.
word(ios+4,0x3000);u.mem_write(io_ram+0x3000,struct.pack('<I',0x12345678))
word(ios+ioff['pc'],0x80002004);word(ios+ioff['next_pc'],0x80002008)
u.mem_write(io_ram+0x2004,struct.pack('<I',(0x23<<26)|(1<<21)|(4<<16)));call('iop_core_step')
assert int.from_bytes(bytes(u.mem_read(ios+16,4)),'big')==0x12345678
print('PASS 1 actual Wii ELF IOP memory-fallback group')
# An IRQ must remain deliverable at the normal boundary after compiled ADDIU.
intc=call('iop_intc_get_state');word(intc+4,1);word(intc+8,1);call('iop_intc_raise',0)
word(ios+ioff['cop0']+12*4,0x401);word(ios+ioff['pc'],0x80002000);word(ios+ioff['next_pc'],0x80002004)
call('iop_core_step')
assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==7
assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==0x80000080
assert int.from_bytes(bytes(u.mem_read(ios+ioff['cop0']+14*4,4)),'big')==0x80002004
assert bytes(u.mem_read(ios+ioff['halted'],1))==b'\0'
print('PASS 1 actual Wii ELF IOP JIT IRQ-boundary group: result retired before exception, correct EPC, no halt')

if enabled:
 before=allocs;fail_alloc=True
 fresh=(9<<26)|(7<<16)|10
 assert call('iop_jit_try_execute_one',ios,0x80003000,fresh)==0
 fail_alloc=False
 assert call('iop_jit_try_execute_one',ios,0x80003000,fresh)==1 and allocs==before+2
 assert int.from_bytes(bytes(u.mem_read(ios+28,4)),'big')==10
 before=allocs;word(syms['cache_size'],1024)
 for n in range(1000):assert call('iop_jit_try_execute_one',ios,0x80003004,fresh+1)==0
 assert allocs==before
 assert call('iop_jit_try_execute_one',ios,0x80003008,fresh)==1 and allocs==before
 print('PASS 1 actual Wii ELF IOP transient-retry/full-cache ownership group')
