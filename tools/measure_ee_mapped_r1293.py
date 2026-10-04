"""Full retirement from real TLB-mapped code/data in the linked PPC ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_frontend_r1276.py').read_text(),'measure_ee_frontend_r1276.py','exec'))
import tempfile,subprocess,json
root=Path(__file__).resolve().parents[1];cc=str(Path(a.nm).with_name('powerpc-eabi-gcc'));oc=str(Path(a.nm).with_name('powerpc-eabi-objcopy'))
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={offsetof(ee_state_t,tlb)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 tlb=struct.unpack('>I',raw.read_bytes())[0]
u.mem_write(state+tlb,bytes(48*16))
u.mem_write(state+tlb,struct.pack('>4I',0,0x200007,(6<<6)|2,(7<<6)|2))
u.mem_write(state+tlb+16,struct.pack('>4I',0,0x300007,(7<<6)|2,(8<<6)|2))
results={}
for kind in ['ADDIU','LW']:
 samples=[]
 for iteration in range(4):
  reset_frontend()
  for n in range(8):guest(0x80006000+4*n,((9<<26)|(2<<21)|(2<<16)|1)if kind=='ADDIU'else((35<<26)|(3<<21)|(2<<16)))
  word(state+off['pc'],0x200000);word(state+off['next_pc'],0x200004)
  word(state+off['cop0']+12*4,0);word(state+off['cop0']+10*4,7)
  u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0');u.mem_write(state,bytes(512))
  u.mem_write(state+48,(0x300400).to_bytes(8,'big')+bytes(8));u.mem_write(ram+0x7400,struct.pack('<I',0x90abcdef))
  before=int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')
  total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
  assert retired==8 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==(8 if kind=='ADDIU'else 0xffffffff90abcdef)
  assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x200020
  assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==(before+8)&0xffffffff
  samples.append(total[0])
 results[kind]={'samples':samples}
# A changed physical mapping and code word must be visible on the next fetch.
u.mem_write(state+tlb+8,struct.pack('>I',(8<<6)|2));guest(0x80008000,(9<<26)|(2<<16)|123)
word(state+off['pc'],0x200000);word(state+off['next_pc'],0x200004)
assert call('ee_core_step_n',1)==1 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==123
guest(0x80008000,(9<<26)|(2<<16)|321);word(state+off['pc'],0x200000);word(state+off['next_pc'],0x200004)
assert call('ee_core_step_n',1)==1 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==321
print('EE_MAPPED_BENCH '+json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'eight fully retired instructions with TLB-mapped code/data; no Wii FPS claim'}))
print('PASS actual PPC mapped fetch/load retirement, Count/PC and immediate TLB replacement/SMC')
