"""Execute normal scratchpad DMA copies from the actual big-endian Wii ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_clut.py').read_text(),'verify_ppc_clut.py','exec'))
ram=0x81710000; spr=0x81720000
call('dma_init');call('dma_bind_ee_ram',ram,4096);call('dma_bind_scratchpad',spr,16384)
def read_reg(address):
 assert call('dma_mmio_read32',address,0x81718000)==1
 return int.from_bytes(bytes(u.mem_read(0x81718000,4)),'big')
payload=bytes(range(32));u.mem_write(ram+256,payload)
for address,value in [(0x1000d410,256),(0x1000d420,2),(0x1000d480,16368),(0x1000d400,0x100)]:call('dma_mmio_write32',address,value)
assert bytes(u.mem_read(spr+16368,16))==payload[:16]
assert bytes(u.mem_read(spr,16))==payload[16:]
assert read_reg(0x1000d480)==16
assert read_reg(0x1000d410)==288
for address,value in [(0x1000d010,512),(0x1000d020,2),(0x1000d080,16368),(0x1000d000,0x100)]:call('dma_mmio_write32',address,value)
assert bytes(u.mem_read(ram+512,32))==payload
assert read_reg(0x1000d080)==16
print('PASS 6 actual Wii ELF SPR checks: both directions, byte order, scratch wrap and address progress')
# A pending VIF MFIFO must survive an empty ring and an incomplete packet.
call('dma_init');call('dma_bind_ee_ram',ram,8192);call('dma_bind_scratchpad',spr,16384)
u.mem_write(ram+4096,b'\0'*128);u.mem_write(spr,struct.pack('<4I',0x70000001,0,0,0)+bytes(range(16)))
for address,value in [(0x1000e000,9),(0x1000e040,0x70),(0x1000e050,0x1000),(0x1000d010,0x1000),(0x10009030,0x1000),(0x10009000,0x104)]:call('dma_mmio_write32',address,value)
assert read_reg(0x10009000)&0x100
assert read_reg(0x10009030)==0x1000
assert not(read_reg(0x1000e010)&2)
for address,value in [(0x1000d020,1),(0x1000d000,0x100)]:call('dma_mmio_write32',address,value)
assert read_reg(0x10009000)&0x100
assert read_reg(0x10009030)==0x1000
for address,value in [(0x1000d020,1),(0x1000d000,0x100)]:call('dma_mmio_write32',address,value)
assert not(read_reg(0x10009000)&0x100)
assert read_reg(0x10009030)==0x1020
assert read_reg(0x1000e010)&2
assert bytes(u.mem_read(ram+4112,16))==bytes(range(16))
print('PASS 9 actual Wii ELF MFIFO checks: empty wait, partial packet, producer wakeup and real payload')
