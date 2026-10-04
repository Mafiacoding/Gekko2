"""Actual Wii ELF presentation pixels and PPC-work measurements."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_vu0_r1273.py').read_text(),'verify_ppc_vu0_r1273.py','exec'))
from unicorn import UC_HOOK_CODE
import json
active=[False];total=[0]
def counter_hook(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,counter_hook);u.ctl_remove_cache(0x80000000,0x81800000)
call('gs_mem_init');dest=0x81742000
for y in range(24):
 for x in range(64):call('gs_mem_write_psmct32',0,64,x,y,0xff0000ff if y%2==0 else 0xff00ff00)
word(dest-4,0x12345678);word(dest+64*48*2,0xabcdef01)
samples=[]
for n in range(3):
 word(0x81780000+8,24) # ninth argument follows the eight EABI register args
 total[0]=0;active[0]=True;call('gs_blit_scaled_psmct32_to_xfb',dest,64,48,0,64,0,0,64);active[0]=False;samples.append(total[0])
assert int.from_bytes(bytes(u.mem_read(dest-4,4)),'big')==0x12345678
assert int.from_bytes(bytes(u.mem_read(dest+64*48*2,4)),'big')==0xabcdef01
red=call('gs_rgb8_pair_to_ycbcr',255,0,0,255,0,0);green=call('gs_rgb8_pair_to_ycbcr',0,255,0,0,255,0)
def blend(a,b,f):return sum((((((a>>c)&255)*(4-f)+((b>>c)&255)*f+2)//4)&255)<<c for c in range(0,32,8))
filtered='R1274' in Path(a.elf).name
expected=[red,blend(red,green,1),blend(red,green,3),blend(green,red,1)] if filtered else [red,red,green,green]
for y,value in enumerate(expected):assert struct.unpack('>32I',bytes(u.mem_read(dest+y*128,128)))==(value,)*32,(y,hex(value))
assert len(set(samples))==1
print('GS_SCALER_MEASURE',json.dumps({'elf':Path(a.elf).name,'source':[64,24],'output':[64,48],'samples':samples,'pixels_correct':True,'scope':'PPC instructions for scaled field presentation; output intentionally changes with vertical filtering. Not Wii frame time/FPS.'}))
