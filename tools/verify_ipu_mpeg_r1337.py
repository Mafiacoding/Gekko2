"""Actual linked Wii PPC decoder, real FIFO stalls and ARM ELF validation.
No BIOS/disc data or physical-Wii speed claim. Transport remains mocked.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask=syms['gekko2_optimization_mask'];word(mask,int.from_bytes(u.mem_read(mask,4),'big')&~(1<<16))
setup()
def stream(idec):
 bits=('1' if idec else '')+('10010'*4+'0010'*2)
 if idec:bits+='11'+'10010'*4+'0010'*2
 bits+='0'*((-len(bits))%8)+f'{0x000001b3:032b}'
 bits+='0'*((-len(bits))%128)
 return bytes(int(bits[i:i+8],2) for i in range(0,len(bits),8))
inp=ram+0x1000;out=ram+0x3000;tmp=ram+0x6000
for command,total in [(0x2c010000,768),(0x10010000,2048),(0x18010000,1024)]:
 call('ipu_init');call('dma_init');data=stream(command>>28==1);u.mem_write(inp,data)
 call('ipu_mmio_write32',0x10002000,command);sent=got=0
 for guard in range(1024):
  if sent<len(data)//16:sent+=call('ipu_input_write',inp+sent*16,1)
  call('ipu_service')
  if got<total//16:got+=call('ipu_output_read',out+got*16,1)
  call('ipu_service');call('ipu_mmio_read32',0x10002010,tmp)
  ctrl=int.from_bytes(u.mem_read(tmp,4),'big')
  if got==total//16 and not(ctrl&0x80000000):break
 assert got==total//16 and not(ctrl&0x80004000),(hex(command),guard,got,hex(ctrl))
 assert ctrl&0x8000
 call('ipu_mmio_read32',0x10002030,tmp);assert int.from_bytes(u.mem_read(tmp,4),'big')==0x1b3
 actual=bytes(u.mem_read(out,total))
 if command>>28==2:expect=bytes([128,0])*384
 else:
  u.mem_write(inp,bytes([128])*384);call('ipu_csc_convert',inp,tmp,0,0,0,0)
  expect=bytes(u.mem_read(tmp,1024))
  if command&(1<<27):
   # RGB16 produced by PACK with neutral dither/threshold state.
   vals=[(expect[i]>>3)|((expect[i+1]>>3)<<5)|((expect[i+2]>>3)<<10)|((expect[i+3]==0x40)<<15) for i in range(0,1024,4)]
   expect=b''.join(struct.pack('<H',v) for v in vals)
  expect*=2
 assert actual==expect,hex(command)
 print('PASS linked PPC MPEG command',hex(command),'output_bytes',total)
elf=Path(__file__).resolve().parents[1]/'arm/build/Gekko2-ARM-Worker.elf'
data=elf.read_bytes();u.mem_write(ram+0x10000,data)
# GCC may split the trivial null/size guard into its caller. These two cases
# provide valid pointers/sizes and exercise the emitted validation body.
if 'arm_loader_validate' not in syms:syms['arm_loader_validate']=syms['arm_loader_validate.part.0']
assert call('arm_loader_validate',ram+0x10000,len(data),0x137f0000,65536,tmp)==1
assert call('arm_loader_validate',ram+0x10000,len(data),0x137f0000,1024,tmp)==0
print('PASS linked PPC ARM ELF plan accepts actual worker, rejects insufficient load area')
