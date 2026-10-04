"""Actual Wii ELF VU upper/lower dispatch plus real microprogram I/E retirement."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_jit_r1268.py').read_text(),'verify_ppc_jit_r1268.py','exec'))
# Compile the real target layout; no guessed struct offsets.
with tempfile.TemporaryDirectory() as directory:
 src=Path(directory)/'layout.c';obj=Path(directory)/'layout.o';raw=Path(directory)/'layout.bin'
 fields=['vf','vi','acc','mem','micro','tpc','branch_delay','branch_target','ebit_delay','running','instructions_executed','unimplemented_opcodes_seen']
 src.write_text('#include <stddef.h>\n#include "core/hw/vu.h"\nconst unsigned layout[]={sizeof(vu1_state_t),'+','.join('offsetof(vu1_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-msdata=none','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
 size,*offsets=struct.unpack('>'+str(len(fields)+1)+'I',raw.read_bytes());voff=dict(zip(fields,offsets))
vs=call('vu1_get_state');call('vu1_init');vf=vs+voff['vf'];vi=vs+voff['vi'];acc=vs+voff['acc'];mem=vs+voff['mem']
upper=(15<<21)|(2<<16)|(1<<11)|(3<<6)|0x28
for j in [1,2]:
 for k in range(4):word(vf+j*16+k*4,0x40000000 if j==1 else 0x40400000)
before=allocs
for n in range(1000):assert call('vu_jit_try_upper',vf,vi,acc,upper)==int(enabled)
assert int.from_bytes(bytes(u.mem_read(vf+48,4)),'big')==(0x40a00000 if enabled else 0)
assert allocs==before+2*int(enabled)
# Arithmetic cache excludes upper flags; same ownership when I/E vary.
for flag in [0x40000000,0x80000000,0xc0000000]:assert call('vu_jit_try_upper',vf,vi,acc,upper|flag)==int(enabled)
assert allocs==before+2*int(enabled)
print('PASS 1 actual Wii ELF VU upper hot-cache and upper-flag group')
lower=(0x40<<25)|(2<<16)|(1<<11)|(3<<6)|0x30;word(vi+4,65535);word(vi+8,2)
before=allocs
for n in range(1000):assert call('vu_jit_try_lower',vf,vi,mem,16383,lower,0,vs+voff['branch_delay'],vs+voff['branch_target'])==int(enabled)
assert int.from_bytes(bytes(u.mem_read(vi+12,4)),'big')==int(enabled)
assert allocs==before+int(enabled)
print('PASS 1 actual Wii ELF VU lower hot-cache VI16 group')
# Shared generated upper code acts on a second VU context without shadow copies.
second=0x90700000;u.mem_map(second,65536);u.mem_write(second,bytes(656))
for j in [1,2]:
 for k in range(4):word(second+j*16+k*4,0x3f800000)
assert call('vu_jit_try_upper',second,second+512,second+640,upper)==int(enabled)
assert int.from_bytes(bytes(u.mem_read(second+48,4)),'big')==(0x40000000 if enabled else 0)
print('PASS 1 actual Wii ELF shared VU arithmetic, distinct state arrays group')
# Ordinary JALR alias capture and local little-endian RAM access.
word(vi+4,9);jalr=(0x25<<25)|(1<<16)|(1<<11)
assert call('vu_jit_try_lower',vf,vi,mem,16383,jalr,0,vs+voff['branch_delay'],vs+voff['branch_target'])==int(enabled)
if enabled:assert int.from_bytes(bytes(u.mem_read(vs+voff['branch_target'],4)),'big')==72 and int.from_bytes(bytes(u.mem_read(vi+4,4)),'big')==2
word(vi+4,1024);u.mem_write(mem,struct.pack('<4I',1,2,3,4));lq=(1<<11)|(6<<16)
assert call('vu_jit_try_lower',vf,vi,mem,16383,lq,0,vs+voff['branch_delay'],vs+voff['branch_target'])==int(enabled)
if enabled:assert struct.unpack('>4I',bytes(u.mem_read(vf+96,16)))==(1,2,3,4)
print('PASS 1 actual Wii ELF VU branch-target capture/local endian-wrap group')
# Real VU1 microprogram: I-bit upper ADDi, E + lower IADD, one delayed pair.
call('vu1_init')
for k in range(4):word(vf+16+k*4,0x40000000)
word(vi+4,7)
pairs=[(0x40400000,0x80000000|(15<<21)|(1<<11)|(2<<6)|0x22),((0x40<<25)|(1<<16)|(1<<11)|(2<<6)|0x30,0x400002ff),((0x40<<25)|(2<<16)|(1<<11)|(3<<6)|0x30,(15<<21)|(2<<16)|(1<<11)|(4<<6)|0x2a)]
for n,(lo,hi) in enumerate(pairs):u.mem_write(vs+voff['micro']+n*8,struct.pack('<2I',lo,hi))
call('vu1_exec_micro',0)
assert int.from_bytes(bytes(u.mem_read(vi+21*4,4)),'big')==0x40400000
assert struct.unpack('>3I',bytes(u.mem_read(vi+4,12)))==(7,14,21)
assert int.from_bytes(bytes(u.mem_read(vf+32,4)),'big')==0x40a00000
assert int.from_bytes(bytes(u.mem_read(vf+64,4)),'big')==0x41200000
assert int.from_bytes(bytes(u.mem_read(vs+voff['instructions_executed'],8)),'big')==3
assert int.from_bytes(bytes(u.mem_read(vs+voff['unimplemented_opcodes_seen'],8)),'big')==0
assert int.from_bytes(bytes(u.mem_read(vs+voff['tpc'],4)),'big')==24
assert int.from_bytes(bytes(u.mem_read(vs+voff['running'],4)),'big')==0
print('PASS 1 actual Wii ELF VU1 I/E pair-order and delayed-retirement integration group')
# Flag instructions unsupported by the existing micro interpreter remain fallback.
bad=0x10<<25;before=allocs
for n in range(1000):assert call('vu_jit_try_lower',vf,vi,mem,16383,bad,0,vs+voff['branch_delay'],vs+voff['branch_target'])==0
assert allocs==before+int(enabled)
print('PASS 1 actual Wii ELF VU negative-cache group')
if enabled:
 fresh=upper+(1<<6);before=allocs;fail_alloc=True
 assert call('vu_jit_try_upper',vf,vi,acc,fresh)==0
 fail_alloc=False
 assert call('vu_jit_try_upper',vf,vi,acc,fresh)==1 and allocs==before+3
 before=allocs;word(syms['owned_count'],512)
 for n in range(1000):assert call('vu_jit_try_upper',vf,vi,acc,fresh+(1<<6))==0
 assert allocs==before
 assert call('vu_jit_try_upper',vf,vi,acc,upper)==1 and allocs==before
 print('PASS 1 actual Wii ELF VU transient-retry/full-cache ownership group')
