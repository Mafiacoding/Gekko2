"""Measure complete filtered scanout, preserving byte-oracle coverage."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_scanout_r1282.py').read_text(),'verify_scanout_r1282.py','exec'))
results={}
for dw,sw,sh,dh in [(640,640,16,36),(640,640,16,16),(320,640,16,36)]:
 u.mem_write(0x81780008,struct.pack('>I',sh))
 total[0]=0;active[0]=True;call('gs_blit_scaled_psmct32_to_xfb',out,dw,dh,64,640,7,31,sw);active[0]=False
 results[f'{dw}x{dh}_from_{sw}x{sh}']=total[0]
print('SCANOUT_R1284_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Complete real PPC scanout calls, not Wii cycles/FPS or GPU rendering.'}))
