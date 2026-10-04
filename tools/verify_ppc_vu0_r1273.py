"""Real Wii ELF VU0 block dispatch, TPC, MSCNT, 4KB wrap and fallback."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_block_r1272.py').read_text(),'verify_ppc_block_r1272.py','exec'))
import tempfile
fields=['vu0_vf','cop2_ctrl','vu0_acc','vu0_mem','vu0_micro','vu0_branch_delay','vu0_branch_target','vu0_ebit_delay','vu0_running','vu0_instructions_executed','vu0_unimplemented_opcodes_seen']
with tempfile.TemporaryDirectory() as directory:
 src=Path(directory)/'layout.c';obj=Path(directory)/'layout.o';raw=Path(directory)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={'+','.join('offsetof(ee_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-msdata=none','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
 offs=dict(zip(fields,struct.unpack('>'+str(len(fields))+'I',raw.read_bytes())))
def address(field):return state+offs[field]
f0=address('vu0_vf');v0=address('cop2_ctrl');m0=address('vu0_micro');d0=address('vu0_mem');pc0=v0+26*4
ctr0=address('vu0_instructions_executed');bad0=address('vu0_unimplemented_opcodes_seen')
def integer(at,width=4):return int.from_bytes(bytes(u.mem_read(at,width)),'big')
def put0(at,l,h):u.mem_write(m0+at,struct.pack('<2I',l,h))
def reset0():
 for field,size in [('vu0_vf',512),('cop2_ctrl',128),('vu0_acc',16),('vu0_mem',4096),('vu0_micro',4096),('vu0_branch_delay',4),('vu0_branch_target',4),('vu0_ebit_delay',4),('vu0_running',1),('vu0_instructions_executed',8),('vu0_unimplemented_opcodes_seen',8)]:u.mem_write(address(field),bytes(size))
 word(f0+12,0x3f800000)
 for k in range(4):word(f0+16+k*4,0x3f800000);word(f0+32+k*4,0x40000000)
 word(v0+4,7);word(v0+8,9)
def stop0(at):put0(at,0x800003bf,0x400002ff);put0((at+8)&4095,0x800003bf,0x2ff)
def finish(count,pc):
 assert integer(ctr0,8)==count and integer(pc0)==pc,(integer(ctr0,8),integer(pc0))
 assert integer(address('vu0_running'),1)==0
 assert integer(address('vu0_branch_delay'))==0 and integer(address('vu0_ebit_delay'))==0
reset0();put0(0,lo,up1);put0(8,lo,up2);stop0(16)
before=integer(syms['block_count'],8) if enabled else 0
call('vu0_exec_micro',state,0);finish(4,32)
assert struct.unpack('>4I',bytes(u.mem_read(f0+64,16)))==(0x40c00000,)*4
if enabled:assert integer(syms['block_count'],8)>before
# Continue resumes TPC=32, rather than restarting address zero.
put0(32,lo,(15<<21)|(2<<16)|(4<<11)|(5<<6)|0x28)
put0(40,lo,(15<<21)|(2<<16)|(5<<11)|(6<<6)|0x2a);stop0(48)
call('vu0_exec_micro_continue',state);finish(8,64)
assert struct.unpack('>4I',bytes(u.mem_read(f0+96,16)))==(0x41800000,)*4
reset0();put0(4088,lo,up1);put0(0,lo,up2);stop0(8)
call('vu0_exec_micro',state,511);finish(4,24)
reset0();word(v0+4,1024)
sq=(1<<25)|(15<<21)|(1<<16)|(2<<11);lq=(15<<21)|(7<<16)|(1<<11)
put0(0,sq,up1);put0(8,lq,up2);stop0(16)
call('vu0_exec_micro',state,0);finish(4,32)
assert struct.unpack('>4I',bytes(u.mem_read(f0+112,16)))==(0x40000000,)*4
assert bytes(u.mem_read(d0,16))==struct.pack('<4I',*([0x40000000]*4))
reset0();word(v0+4,3)
dec=(0x40<<25)|(1<<16)|(1<<11)|(31<<6)|0x32
put0(0,dec,up1)
for n in range(1,8):put0(n*8,lo,up1)
put0(64,(0x29<<25)|(1<<11)|0x7f7,up2)
put0(72,(0x40<<25)|(15<<21)|(5<<16)|(3<<11)|(12<<6)|0x3c,up1);stop0(80)
call('vu0_exec_micro',state,0);finish(32,96);assert integer(v0+4)==0
reset0();put0(0,lo,up1);put0(8,lo,up2);put0(16,0x10<<25,0x2ff);stop0(24)
call('vu0_exec_micro',state,0);finish(5,40);assert integer(bad0,8)==1
reset0();put0(0,lo,up1);put0(8,lo,up2);stop0(16)
word(address('vu0_branch_delay'),1);word(address('vu0_branch_target'),16)
call('vu0_exec_micro',state,0);finish(3,32);assert bytes(u.mem_read(f0+64,16))==bytes(16)
reset0();put0(0,lo,up1);put0(8,lo,(15<<21)|(2<<16)|(3<<11)|(4<<6)|0x2c);stop0(16)
call('vu0_exec_micro',state,0);finish(4,32)
assert struct.unpack('>4I',bytes(u.mem_read(f0+64,16)))==(0x3f800000,)*4
print('PASS real Wii ELF VU0 block use, TPC/MSCNT, 4KB code/data wrap, mixed branches/E delay, unsupported fallback, pending delay and word replacement')
# Exercise the real VIF0 command path rather than only direct VU calls.
call('vif_init');reset0();put0(0,lo,up1);put0(8,lo,up2);stop0(16)
command=mem+0x3000
u.mem_write(command,struct.pack('<I',0x14000000))
call('vif0_process_tag_words',0,command,1);finish(4,32)
put0(32,lo,up1);put0(40,lo,up2);stop0(48)
u.mem_write(command,struct.pack('<I',0x17000000))
call('vif0_process_tag_words',0,command,1);finish(8,64)
print('PASS actual Wii ELF VIF0 MSCAL/MSCNT command integration into the VU0 backend')
