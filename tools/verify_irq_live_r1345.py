from pathlib import Path
exec(compile(Path(__file__).with_name("ee_ppc_fixture_internal.py").read_text(),"ee_ppc_fixture_internal.py","exec"))

with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'layout.c';obj=Path(td)/'layout.o';raw=Path(td)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/hw/dma.h"\nconst unsigned layout[]={offsetof(dma_state_t,d_stat),offsetof(dma_state_t,d_ctrl),offsetof(dma_state_t,d_enable_state)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 ds,dc,de=struct.unpack('>3I',raw.read_bytes())
dma=call('dma_get_state');intc=call('ee_intc_get_state');checks=0
for stat in [0,1,0x10000,0x10001,0x8000,0x80000000,0x80008000,0xffff,0xffffffff]:
 for ctrl in [0,1,2,3]:
  for enable in [0,0x10000,0x20000,0xff010000]:
   for pending in [0,1]:
    extension_setup()
    word(dma+ds,stat);word(dma+dc,ctrl);word(dma+de,enable)
    word(intc,2);word(intc+4,2 if pending else 0)
    expected=int(bool(ctrl&1) and ((enable>>16)&255)!=1 and bool((stat&(stat>>16)&65535) or (stat&0x8000)))
    if 'dma_dmac_interrupt_pending' in syms:assert call('dma_dmac_interrupt_pending')==expected
    if 'ee_intc_pending' in syms:assert call('ee_intc_pending')==pending
    word(state+off['cop0']+11*4,0xffffffff)
    word(state+off['cop0']+12*4,0) # latch, without vectoring or handler side effects
    call('ee_core_park_tick',state)
    cause=int.from_bytes(u.mem_read(state+off['cop0']+13*4,4),'big')
    assert bool(cause&0x400)==bool(pending),(stat,ctrl,enable,pending,hex(cause))
    assert bool(cause&0x800)==bool(expected),(stat,ctrl,enable,pending,hex(cause))
    checks+=1
print('PASS',checks,'live INTC/DMAC state combinations through real PPC EE parked retirement')
