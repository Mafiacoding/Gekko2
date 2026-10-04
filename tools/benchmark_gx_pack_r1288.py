"""Measured actual PPC texture packing instructions; not Wii timing."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_gx_pack_r1284.py').read_text(),'verify_gx_pack_r1284.py','exec'))
import json,hashlib
from unicorn import UC_HOOK_CODE
results={}
for width,height in [(64,32),(640,32)]:
    count=[0]
    def instruction(uc,address,size,user):count[0]+=1
    handle=u.hook_add(UC_HOOK_CODE,instruction)
    size=width*height*4
    assert call('gs_gx_pack_rgba8',out,size,64,1024,0,0,width,height)==size
    u.hook_del(handle)
    results[f'{width}x{height}']={'PPC':count[0],'sha256':hashlib.sha256(bytes(u.mem_read(out,size))).hexdigest()}
print('GX_PACK_BENCH '+json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'PPC instructions and exact texture bytes, not Wii cycles or GPU timings'}))
