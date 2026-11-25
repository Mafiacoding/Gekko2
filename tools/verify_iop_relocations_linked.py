#!/usr/bin/env python3
"""Execute IRX relocator on actual linked big-endian PPC with synthetic LE ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'layout.c';obj=Path(td)/'layout.o';raw=Path(td)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned data[]={sizeof(iop_state_t),offsetof(iop_state_t,ram),offsetof(iop_state_t,ram_size)};')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-Iinclude','-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 size_iop,ram_off,ram_size_off=struct.unpack('>3I',raw.read_bytes())
iop=call('iop_core_get_state');ir=0x92000000;u.mem_map(ir,0x200000)
u.mem_write(iop,bytes(size_iop));word(iop+ram_off,ir);word(iop+ram_size_off,0x200000)
image=0x817a0000;result=0x817b0000;base_iop=0x108004;checks=0
def fixture():
 b=bytearray(512);b[:7]=b'\x7fELF\1\1\1'
 struct.pack_into('<HHIIIIIHHHHHH',b,16,0xff80,8,1,0,52,256,0,52,32,1,40,2,0)
 struct.pack_into('<8I',b,52,1,128,0,0,64,64,7,4)
 struct.pack_into('<10I',b,296,0,9,0,0,192,32,0,0,4,8)
 return b
def rel(b,n,at,kind):struct.pack_into('<2I',b,192+n*8,at,kind)
def run(b):
 u.mem_write(image,bytes(b));return call('iop_elf_load',iop,image,len(b),base_iop,result,result+1024)
for low in [0,1,0x7fff,0x8000,0xffff]:
 b=fixture();struct.pack_into('<2I',b,128,0x24080000|low,0x24090000|low)
 rel(b,0,0,1);rel(b,1,4,6);rel(b,2,0xffffffff,0);rel(b,3,0xffffffff,0)
 assert run(b)==0
 for n,opcode in [(0,0x24080000),(1,0x24090000)]:
  assert call('iop_mem_read32',iop,base_iop+n*4)==opcode|((base_iop+low)&65535);checks+=1
for bad in range(4):
 b=fixture();struct.pack_into('<3I',b,128,0x3c080002,0x3c0a0000,0x3c09ffff)
 rel(b,0,0,250);rel(b,1,0x12345,0 if bad==3 else 251);rel(b,2,0xffffffff,0);rel(b,3,0xffffffff,0)
 if bad==1:struct.pack_into('<I',b,132,0x3c0affff)
 if bad==2:struct.pack_into('<I',b,136,0x3c097fff)
 rc=run(b)
 if bad:
  assert rc==0xffffffff and call('iop_mem_read32',iop,base_iop)==0x3c080002
 else:
  assert rc==0;hi=((base_iop+0x12345+0x8000)>>16)&65535
  for n,op in enumerate([0x3c080000,0x3c0a0000,0x3c090000]):assert call('iop_mem_read32',iop,base_iop+n*4)==op|hi
 checks+=1
print('PASS',checks,'actual PPC IRX NONE/16/LO16/SCE-MHI16 cases and malformed chains; little-endian guest RAM')
