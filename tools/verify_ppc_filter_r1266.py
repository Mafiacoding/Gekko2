"""Actual emitted Wii UV-sprite filtering; inherits previous emitted-ELF checks."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_config_r1265.py').read_text(),'verify_ppc_config_r1265.py','exec'))
for linear in [0,1]:
 call('gif_init');call('gs_mem_init')
 for y in range(2):
  for x in range(2):call('gs_mem_write_psmct32',2000*64,64,x,y,0x80000000|(128*x))
 values=[(0x4c,10<<16,0),(6,2000|(1<<14)|(1<<26)|(1<<30),(1<<2)|(1<<3)),(8,0,0),(0x14,0x60 if linear else 0,0),(0,6|16|256,0),(1,0x80808080,0),(3,16,0),(5,0,0),(5,32|(32<<16),0)]
 packet=struct.pack('<4I',len(values)|0x8000,1<<28,14,0)+b''.join(struct.pack('<4I',lo,hi,reg,0) for reg,lo,hi in values)
 u.mem_write(0x81742000,packet);call('gif_process_quadwords',2,0x81742000,len(values)+1)
 assert call('gs_mem_read_psmct32',0,640,0,0)==(0x80000040 if linear else 0x80000080),('UV sprite filter',linear)
print('PASS 2 actual Wii ELF UV sprite pixels: nearest and bilinear')
