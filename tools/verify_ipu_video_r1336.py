"""Actual linked PPC VDEC and PACK FIFO/DMA. Synthetic stream, SDK mocked."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
initial=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
word(syms['gekko2_optimization_mask'],initial&~(1<<16))
setup();call('ee_intc_init');call('ipu_init')
dest=ram+0x4000;source=ram+0x1000
def rd(addr):
 assert call('ipu_mmio_read32',addr,dest)==1
 return int.from_bytes(u.mem_read(dest,4),'big')
for bits,table,ctrl,expected,bp in [(0x80000000,0,0,0x10001,1),(8<<21,0,0,0xb0023,11),
 (15<<21,0,1<<23,0xb0022,11),(0x60000000,2,0,0xffffffff,3),
 (0xc0000000,3,0,0xffffffff,2),(0x80000000,1,3<<24,0x2008c,2)]:
 call('ipu_init');call('ipu_mmio_write32',0x10002010,ctrl)
 u.mem_write(source,struct.pack('>I',bits)+bytes(28));assert call('ipu_input_write',source,2)==2
 call('ipu_mmio_write32',0x10002000,0x30000000|(table<<26))
 assert rd(0x10002000)==expected and rd(0x10002020)&127==bp
 assert rd(0x10002004)==0
# TOP starvation must retain a successfully decoded symbol.
call('ipu_init');call('ipu_mmio_write32',0x10002000,96)
u.mem_write(source,bytes(12)+bytes([128])+bytes(3));call('ipu_input_write',source,1)
call('ipu_mmio_write32',0x10002000,0x30000000)
assert rd(0x10002000)==0x10001 and rd(0x10002004)==0x80000000
u.mem_write(source,bytes(16));call('ipu_input_write',source,1);call('ipu_service')
assert rd(0x10002004)==0 and rd(0x10002020)&127==97
# PACK really consumes a 64-QWC RGB32 stream and emits 32 QWCs through DMA.
rgba=bytes([248,0,0,64])*256;u.mem_write(source,rgba)
call('ipu_init');call('dma_init');call('dma_bind_ee_ram',ram,0x400000)
call('ipu_mmio_write32',0x10002000,0x88000001)
for base,addr,qwc,chcr in [(0x1000b000,0x2000,32,0x100),(0x1000b400,0x1000,64,0x101)]:
 for offset,value in [(16,addr),(32,qwc),(0,chcr)]:assert call('dma_mmio_write32',base+offset,value)==1
assert bytes(u.mem_read(ram+0x2000,512))==bytes([31,128])*256
assert rd(0x10002010)&0x80000000==0
print('PASS linked PPC VDEC tables/signed data/TOP starvation and PACK actual 64->32-QWC DMA RGB16')
