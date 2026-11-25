#!/usr/bin/env python3
"""Execute actual big-endian Wii IPU/DMA code on synthetic input.

Platform services use the existing PPC fixture; no GPU, BIOS or game image.
The cosine sample oracle is independent of the production integer IDCT.
"""
from pathlib import Path
import math

exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),
             'ee_ppc_fixture_internal.py', 'exec'))
word(syms['gekko2_optimization_mask'],
     int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big') & ~(1<<16))
setup()
inp,out,tmp=ram+0x1000,ram+0x3000,ram+0x6000

def read(addr):
    assert call('ipu_mmio_read32',addr,tmp)==1
    return int.from_bytes(u.mem_read(tmp,4),'big')

def dma_read(addr):
    assert call('dma_mmio_read32',addr,tmp)==1
    return int.from_bytes(u.mem_read(tmp,4),'big')

def stream(quantized,dtd,interlaced,q):
    bits='01' if quantized else '1'
    if dtd: bits+=str(interlaced)
    if quantized: bits+=f'{q:05b}'
    for b in range(6):
        bits+=('110'+('1000' if b==0 else '0111')) if b in (0,2) else ('100' if b<4 else '00')
        if b==0: bits+='110'
        bits+='10'
    bits+='0'*(-len(bits)%8)+f'{0x1b3:032b}'
    bits+='0'*(-len(bits)%128)
    return bytes(int(bits[i:i+8],2) for i in range(0,len(bits),8))

for command in (0x10000000,0x20000000):
    call('dma_init');call('ipu_init')
    call('ipu_mmio_write32',0x10002000,command)
    assert read(0x10002034)==0x80000000 and read(0x10002004)==0
    call('ipu_mmio_write32',0x10002010,0x40000000)
    assert read(0x10002034)==0

cases=0
scales=[0,1,2,3,4,5,6,7,8,10,12,14,16,18,20,22,24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112]
for q in (0,7,31):
    for nl in (0,1):
        for quantized in (0,1):
            for dtd,interlaced in ((0,0),(1,0),(1,1)):
                call('dma_init');call('ipu_init')
                # Load the actual IQ table through SETIQ and its input FIFO.
                u.mem_write(inp,bytes([16])*64)
                call('ipu_mmio_write32',0x10002000,0x50000000)
                assert call('ipu_input_write',inp,4)==4
                call('ipu_service');assert not(read(0x10002010)&0x80000000)
                call('ipu_mmio_write32',0x10002010,(nl<<22)|(3<<24))
                call('ipu_mmio_write32',0x10002000,0x10000000|(q<<16)|(dtd<<24))
                assert read(0x10002034)==0x80000000
                assert read(0x10002010)>>24&7==1
                data=stream(quantized,dtd,interlaced,q);u.mem_write(inp,data)
                assert call('ipu_input_write',inp,len(data)//16)==len(data)//16
                got=0
                for guard in range(32):
                    call('ipu_service')
                    got+=call('ipu_output_read',out+got*16,min(8,64-got))
                    if got==64 and not(read(0x10002010)&0x80000000):break
                assert got==64 and read(0x10002010)&0xc000==0x8000
                assert read(0x10002030)==0x1b3 and read(0x10002034)==0
                actual=bytes(u.mem_read(out,1024))
                scale=(scales[q] if nl else 2*q) if quantized else q
                for y in range(16):
                    for x in range(16):
                        b=x//8+(y%2 if interlaced else y//8)*2
                        sample=136 if b<2 else 128
                        if b==0:sample+=scale*math.cos((2*(x%8)+1)*math.pi/16)/(4*math.sqrt(2))
                        lum=max(0,math.floor(sample+0.5)-16)*149//64
                        gray=min(255,(lum+1)//2);at=(y*16+x)*4
                        assert all(abs(actual[at+c]-gray)<=2 for c in range(3)),(q,nl,quantized,dtd,interlaced,x,y)
                        assert actual[at+3]==128
                cases+=1
print(f'PASS linked PPC IDEC: {cases} cosine-oracle macroblocks, quant/DCT headers, TOP busy/reset, PCT, FIFO output')

# Real FromIPU DMA publishes STADR at start and only after accepted output.
call('dma_init');call('ipu_init');call('dma_bind_ee_ram',ram,0x400000)
call('dma_mmio_write32',0x1000e000,3<<4)
call('dma_mmio_write32',0x1000e060,0x1234)
call('dma_mmio_write32',0x1000b010,0x3000)
call('dma_mmio_write32',0x1000b020,64)
call('dma_mmio_write32',0x1000b000,0x100)
assert dma_read(0x1000e060)==0x3000 and dma_read(0x1000b020)==64
call('ipu_mmio_write32',0x10002000,0x70000001)
u.mem_write(inp,bytes([128])*384)
sent=0
for guard in range(16):
    sent+=call('ipu_input_write',inp+sent*16,min(8,24-sent))
    call('ipu_service')
    if sent==24 and not(read(0x10002010)&0x80000000):break
assert dma_read(0x1000e060)==0x3400 and dma_read(0x1000b020)==0
assert not(dma_read(0x1000b000)&0x100) and dma_read(0x1000e010)&8
assert bytes(u.mem_read(out,1024))==bytes([130,130,130,128])*256
print('PASS linked PPC FromIPU CSC DMA: start/progress STADR, output bytes, QWC/STR and completion')

for channel in (1,2):
    for selector in range(4):
        call('dma_init');call('gif_init');call('vif_init')
        call('dma_bind_ee_ram',ram,0x400000)
        call('dma_set_sink',channel,syms['gif_process_quadwords' if channel==2 else 'vif1_process_quadwords'])
        u.mem_write(ram+256,struct.pack('<8I',0x40000002,4096,0,0,0x70000000,0,0,0))
        u.mem_write(ram+4096,struct.pack('<8I',0x8000 if channel==2 else 0,0,0,0,0x8000 if channel==2 else 0,0,0,0))
        call('dma_mmio_write32',0x1000e000,selector<<6)
        call('dma_mmio_write32',0x1000e060,4096)
        base=0x10009000 if channel==1 else 0x1000a000
        call('dma_mmio_write32',base+0x30,256)
        call('dma_mmio_write32',base,0x105)
        selected=selector==channel
        if selected:
            assert dma_read(base+0x20)==2 and dma_read(base)&0x100
            assert dma_read(0x1000e010)&(1<<13)
            call('dma_mmio_write32',0x1000e060,4112);call('dma_gif_service')
            assert dma_read(base+0x20)==1 and dma_read(base+0x10)==4112
            call('dma_mmio_write32',0x1000e060,4128);call('dma_gif_service')
        assert dma_read(base+0x20)==0 and not(dma_read(base)&0x100)
        assert dma_read(base+0x10)==288 and dma_read(0x1000e010)&(1<<channel)
        assert selected or not(dma_read(0x1000e010)&(1<<13))
print('PASS linked PPC REFS: all 4 STD selectors for VIF1/GIF, partial progress and completion')
