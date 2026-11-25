#!/usr/bin/env python3
"""Actual linked PPC GS points and VIF/GIF streaming, synthetic packets only."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),
             'ee_ppc_fixture_internal.py','exec'))
setup()
mask=syms['gekko2_optimization_mask']
word(mask,int.from_bytes(u.mem_read(mask,4),'big')&~(1<<16))
inp=ram+0x10000

with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);src=p/'layout.c';obj=p/'layout.o';raw=p/'layout.bin'
    src.write_text('#include <stddef.h>\n#include "core/hw/vif.h"\n#include "core/hw/gif.h"\n'
                   'const unsigned layout[]={offsetof(vif_state_t,direct_needed_words),'
                   'offsetof(vif_state_t,direct_sent_words),offsetof(vif_state_t,direct_qwords_forwarded),'
                   'offsetof(vif_state_t,itops),offsetof(gif_state_t,prim),offsetof(gif_state_t,rgba)};\n')
    subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
    subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
    need,sent,forwarded,itops,prim,rgba=struct.unpack('>6I',raw.read_bytes())

def field(ptr,off,n=4):return int.from_bytes(u.mem_read(ptr+off,n),'big')
def reset():
    call('dma_init');call('gs_mem_init');call('gs_init');call('gif_init');call('vif_init')

stream=struct.pack('<16I',0x50000003,0x8002,0x10000000,0xe,0,0x123,0,0,0,
                   0x80402010,0,1,0,0x04000035,0,0)
for split in range(17):
    reset();u.mem_write(inp,stream)
    v=call('vif1_get_state');g=call('gif_get_state')
    call('vif1_process_tag_words',1,inp,split)
    assert field(v,forwarded,8)==min(max(split-1,0),12)//4
    if split>=9:assert field(g,prim)==0x123
    call('vif1_process_tag_words',1,inp+split*4,16-split)
    assert field(v,need)==0 and field(v,forwarded,8)==3
    assert field(v,itops)==0x35 and field(g,rgba)==0x80402010
print('PASS linked PPC VIF DIRECT: 17 input splits, early PATH2 visibility, incomplete QWCs and trailing VIFcode')

def packet(registers):
    data=struct.pack('<4I',0x8000|len(registers),1<<28,0xe,0)
    return data+b''.join(struct.pack('<4I',lo,hi,addr,0) for addr,lo,hi in registers)

checks=0
for psm in (0,1,2,10):
    reset();is16=psm in (2,10);x=y=4
    if is16:call('gs_mem_write_psmct16s' if psm==10 else 'gs_mem_write_psmct16',0,64,x,y,0x7fff)
    else:call('gs_mem_write_psmct32',0,64,x,y,0x12505050)
    # 16-bit destination blend exposes zero extension versus bit replication.
    # For CT24/32, DTHE=1 and nonzero DIMX must not modify source RGB.
    regs=[(0x4c,(1<<16)|(psm<<24),0),(0x18,0,0),(0x40,31<<16,31<<16),
          (0x47,0,0),(0x46,1,0),(0x45,0 if is16 else 1,0),(0x44,3,0),
          (0x42,0xa9,127),(0,0x40 if is16 else 0,0),
          (1,0x80000000 if is16 else 0x80505050,0),(5,(x<<4)|(y<<20),0)]
    data=packet(regs);u.mem_write(inp,data);call('gif_process_quadwords',2,inp,len(data)//16)
    if is16:
        actual=call('gs_mem_read_psmct16s' if psm==10 else 'gs_mem_read_psmct16',0,64,x,y)
        assert actual==0xfbde,(psm,hex(actual))
    else:
        actual=call('gs_mem_read_psmct32',0,64,x,y)
        assert actual==(0x12505050 if psm==1 else 0x80505050),(psm,hex(actual))
    checks+=1
print(f'PASS linked PPC GS: {checks} framebuffer formats, CT16 destination blend and CT24/32 dither exclusion')
