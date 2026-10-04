"""Execute the Wii-compiled EE interpreter's MIPS IRQ polling fixture on PPC.

Both ELF variants are checked; this calls ee_core_step_n(1), not the frontend JIT
wrapper. It therefore validates shared interrupt delivery, not JIT coverage.
"""
from pathlib import Path
import tempfile
exec(compile(Path(__file__).with_name('verify_ppc_linear_r1261.py').read_text(),'verify_ppc_linear_r1261.py','exec'))
root=Path(__file__).resolve().parents[1]
cc=str(Path(a.nm).with_name('powerpc-eabi-gcc'))
with tempfile.TemporaryDirectory() as d:
    src=Path(d)/'layout.c';obj=Path(d)/'layout.o'
    fields=['gpr','pc','next_pc','cop0','ram','ram_size','halted','branch_pending']
    src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst uint32_t layout[]={sizeof(ee_state_t),'+','.join('offsetof(ee_state_t,'+f+')' for f in fields)+'};\n')
    subprocess.run([cc,'-O2','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
    blob=obj.read_bytes();shoff=struct.unpack_from('>I',blob,32)[0];stride,n,stridx=struct.unpack_from('>HHH',blob,46)
    headers=[struct.unpack_from('>10I',blob,shoff+stride*i) for i in range(n)]
    h=headers[stridx];names=blob[h[4]:h[4]+h[5]]
    for h in headers:
        name=names[h[0]:].split(b'\0',1)[0]
        if name==b'.rodata': layout=struct.unpack_from('>9I',blob,h[4]);break
    else: raise AssertionError('missing ABI layout constants')
size,*offsets=layout;off=dict(zip(fields,offsets));state=call('ee_core_get_state')
u.mem_write(state,bytes(size));ram=0x90000000;u.mem_map(ram,65536)
def word(address,v):u.mem_write(address,struct.pack('>I',v))
word(state+off['ram'],ram);word(state+off['ram_size'],65536)
word(state+off['pc'],0x80001000);word(state+off['next_pc'],0x80001004)
word(state+off['cop0']+12*4,0x10401)
u.mem_write(state+off['gpr']+8*16,struct.pack('>Q',0x1000f000))
code=[0,0x8d020000,0x30420004,0x14400003,0,0x08000401,0,0x24100001,0x08000408,0]
handler=[0x3c1b1000,0x377bf000,0x241a0004,0xaf7a0000,0x42000018,0]
u.mem_write(ram+0x1000,struct.pack('<10I',*code));u.mem_write(ram+0x200,struct.pack('<6I',*handler))
call('ee_hle_thread_init');call('dma_init');call('ee_intc_init')
intc=call('ee_intc_get_state');word(intc+4,4);call('ee_intc_raise',2)
for i in range(100):call('ee_core_step_n',1)
assert bytes(u.mem_read(state+off['gpr']+16*16,8))==struct.pack('>Q',1),'poll did not exit'
assert not(int.from_bytes(bytes(u.mem_read(intc,4)),'big')&4),'guest ACK missing'
assert bytes(u.mem_read(state+off['halted'],1))==b'\0','EE halted'
print('PASS 3 actual Wii ELF EE interpreter IRQ checks: polling exit, guest ACK/ERET, no halt')

# Execute the actual HLE syscall instructions as emitted in each Wii ELF.
u.mem_write(ram+0x3000,struct.pack('<I',12))
call('dma_init');dma=call('dma_get_state')
for base,mask_addr,bit in [(20,intc+4,1<<5),(22,dma+4,1<<21)]:
    # Get the DMA field offset from the compiler too, rather than guessing.
    if base==22:
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'dma.c';obj=Path(d)/'dma.o';raw=Path(d)/'dma.bin'
            src.write_text('#include <stddef.h>\n#include "core/hw/dma.h"\nconst uint32_t offset=offsetof(dma_state_t,d_stat);\n')
            subprocess.run([cc,'-O2','-msdata=none','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
            subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
            mask_addr=dma+struct.unpack_from('>I',raw.read_bytes())[0]
        word(mask_addr,8)
    else:
        word(mask_addr,0);word(intc,8)
    for k in range(4):
        word(state+off['pc'],0x80003000);word(state+off['next_pc'],0x80003004)
        u.mem_write(state+off['branch_pending'],b'\0')
        u.mem_write(state+off['gpr']+3*16,struct.pack('>Q',base+(k>=2)))
        u.mem_write(state+off['gpr']+4*16,struct.pack('>Q',37))
        call('ee_core_step_n',1)
        assert bytes(u.mem_read(state+off['gpr']+2*16,8))==struct.pack('>Q',1 if k%2==0 else 0)
        value=int.from_bytes(bytes(u.mem_read(mask_addr,4)),'big')
        assert bool(value&bit)==(k<2)
        assert (int.from_bytes(bytes(u.mem_read(intc if base==20 else mask_addr,4)),'big')&8)==8
print('PASS 24 actual Wii ELF syscall checks: INTC/DMAC changed returns, mask state, pending flags and wrapped shift counts')
