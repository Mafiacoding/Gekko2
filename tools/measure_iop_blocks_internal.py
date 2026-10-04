"""Warm full-retirement PPC counts: blocks versus scalar IOP entry.
Synthetic instruction counts, not hardware cycles or measured Wii FPS.
"""
from pathlib import Path
prefix=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction sequences')[0]
exec(compile(prefix,'verify_iop_blocks_internal.py','exec'))
results={}
for name,w in [('ADDIU',(9<<26)|(2<<21)|(2<<16)|1),('ORI',(13<<26)|(2<<21)|(2<<16)|1),('MULT',(2<<21)|(3<<16)|0x18),('DIV',(2<<21)|(3<<16)|0x1a),('LW',(0x23<<26)|(3<<21)|(2<<16)),('SW',(0x2b<<26)|(3<<21)|(2<<16)),('LWL',(0x22<<26)|(3<<21)|(2<<16)|1),('RFE',0x42000010)]:
 regs=[0]*32;regs[2]=0x12345678;regs[3]=0x80180000
 setup([w]*16,regs);assert call('iop_core_step_n',8)==8
 counts=[]
 for native in [False,True]:
  u.mem_write(ist,struct.pack('>32I',*regs));word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
  word(ist+io['cop0']+12*4,0);u.mem_write(ist+io['halted'],b'\0');u.mem_write(ist+io['idle'],b'\0')
  total[0]=0;active[0]=True
  if native:call('iop_core_step_n',8)
  else:
   for _ in range(8):call('iop_core_step')
  active[0]=False;counts.append(total[0])
 results[name]={'scalar':counts[0],'block':counts[1],'reduction_percent':round(100*(1-counts[1]/counts[0]),1)}
print('IOP_BLOCK_MEASURE',json.dumps(results,sort_keys=True))
