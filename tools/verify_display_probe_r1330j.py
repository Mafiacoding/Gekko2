"""Run actual linked PPC scanout detection, including pixels missed by grid."""
from pathlib import Path
import hashlib
loader=Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0]
exec(compile(loader,'verify_ppc_gs_memory.py','exec'))
checks=0
for x,y in [(1,1),(3,7),(31,15),(63,31)]:
 call('gs_mem_init');call('gs_mem_write_psmct32',0,64,x,y,0x80000100)
 assert call('gs_display_has_rgb',0,64,0,0,64,32)==1,(x,y)
 checks+=1
call('gs_mem_init');call('gs_mem_write_psmct32',0,640,639,255,0x80000001)
assert call('gs_display_has_rgb',0,640,0,0,640,256)==1
assert call('gs_display_has_rgb',0,640,0,0,639,255)==0
checks+=2
call('gs_mem_init');call('gs_mem_write_psmct32',0,640,1,1,0xff000000)
assert call('gs_display_has_rgb',0,640,0,0,640,256)==0
assert call('gs_display_has_rgb',0,0,0,0,640,256)==0
checks+=2
print(f'PASS {checks} linked PPC first-image probes: unsampled sparse RGB, last scanout pixel, crop exclusion, black/alpha-only and invalid width')
