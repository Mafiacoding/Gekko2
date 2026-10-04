"""Exercise malformed IOP ELF input in the actual Wii PowerPC binary."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(), 'verify_ppc_gs_memory.py', 'exec'))
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory); src=d/'layout.c'; obj=d/'layout.o'; raw=d/'layout.bin'
    src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),offsetof(iop_state_t,ram),offsetof(iop_state_t,ram_size)};\n')
    subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-gcc')),'-msdata=none','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
    subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
    size,ramoff,sizeoff=struct.unpack('>3I',raw.read_bytes())
u.mem_map(0x90000000,0x100000)
st,image,out,err,ram=0x90000000,0x90010000,0x90020000,0x9002f000,0x90030000
u.mem_write(st,bytes(size));u.mem_write(st+ramoff,struct.pack('>I',ram));u.mem_write(st+sizeoff,struct.pack('>I',65536))
b=bytearray(160);b[:7]=b'\x7fELF\x01\x01\x01'
def w32(buf,off,n):struct.pack_into('<I',buf,off,n)
def w16(buf,off,n):struct.pack_into('<H',buf,off,n)
w16(b,18,8);w32(b,24,0);w32(b,28,52);w16(b,42,32);w16(b,44,1)
for off,n in [(52,1),(56,84),(60,0),(68,4),(72,4)]:w32(b,off,n)
b[84:88]=b'\x12\x34\x56\x78'
def run(buf):
    u.mem_write(image,bytes(buf));return call('iop_elf_load',st,image,len(buf),0x1000,out,err)
assert run(b)==0
assert bytes(u.mem_read(ram+0x1000,4))==b[84:88]
cases=[(28,0xfffffff0,4),(28,0xffffffff,4),(56,0xfffffffe,4),(68,0xffffffff,4),(72,1,4),(42,0,2),(32,0xfffffff0,4)]
for off,n,width in cases:
    bad=bytearray(b)
    if off==32:w16(bad,48,1);w16(bad,46,40)
    (w32 if width==4 else w16)(bad,off,n)
    before=bytes(u.mem_read(ram,65536));assert run(bad)==0xffffffff,(off,n)
    assert bytes(u.mem_read(ram,65536))==before,(off,n,'RAM mutated')
print('PASS 1 valid and 7 malformed IOP ELF cases in actual Wii PPC binary')
for paired in (False,True):
    bad=bytearray(b);w32(bad,32,88);w16(bad,46,40);w16(bad,48,1)
    w32(bad,92,9);w32(bad,104,128);w32(bad,108,16 if paired else 8)
    w32(bad,128,0 if paired else 0xfffffff0);w32(bad,132,5 if paired else 2)
    if paired:w32(bad,136,0xfffffff0);w32(bad,140,6)
    before=bytes(u.mem_read(ram,65536));assert run(bad)==0xffffffff
    assert bytes(u.mem_read(ram,65536))==before
print('PASS 2 wrapped relocation target cases in actual Wii PPC binary')
