"""Linked Wii GX ownership oracle with a deliberately deferred mock GPU.
FIFO copy/sample order and CPU invalidation are checked independently of
actual GPU rendering; visual correctness still requires physical Wii tests.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
from unicorn.ppc_const import UC_PPC_REG_FPR1
u.mem_map(0x91400000,0x400000)
u.mem_map(0xcc008000,0x1000) # inline GX vertex writes to the write-gather pipe
surface_addr,pixels,texture_addr,mode,xfb=0x91400000,0x91500000,0x91600000,0x91700000,0x91701000
fields=['fbWidth','efbHeight','xfbHeight','aa']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'gxlayout.c';obj=Path(d)/'gxlayout.o';raw=Path(d)/'gxlayout.bin'
 src.write_text('#include <stddef.h>\n#include <gccore.h>\nconst unsigned layout[]={sizeof(GXRModeObj),'+','.join('offsetof(GXRModeObj,'+f+')' for f in fields)+'};\n')
 ogc=Path(cc).resolve().parents[2]/'libogc/include'
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(ogc),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 mode_size,*mo=struct.unpack('>5I',raw.read_bytes())
u.mem_write(mode,bytes(mode_size))
for o in mo[:3]:u.mem_write(mode+o,struct.pack('>H',64 if o==mo[0] else 4))
word(syms['surface'],surface_addr);word(syms['surface_pixels'],pixels)
word(syms['texture'],texture_addr);word(syms['initialized'],1)
word(syms['efb_width'],64);word(syms['efb_height'],4)
pending=[];events=[];cpu_safe=True
names={v:k for k,v in syms.items() if k.startswith('GX_') or k=='DCInvalidateRange' or 'guMtx' in k or 'guOrtho' in k}
def hardware(uc,address,size,user):
 global cpu_safe
 name=names.get(address)
 if name is None:return
 if name in ['GX_CopyTex','GX_PixModeSync','GX_InvalidateTexAll','GX_CopyDisp','GX_DrawDone','DCInvalidateRange']:events.append(name)
 if name=='GX_CopyTex':pending.append(uc.reg_read(UC_PPC_REG_3));cpu_safe=False
 if name=='GX_DrawDone':
  for target in pending:
   data=bytearray(64*4*4);data[1]=17;data[32]=34;data[33]=51;uc.mem_write(target,bytes(data))
  pending.clear();cpu_safe=True
 if name=='DCInvalidateRange':assert cpu_safe,'CPU invalidation overtook queued GPU copy'
 if name=='GX_GetYScaleFactor':uc.reg_write(UC_PPC_REG_FPR1,0x3ff0000000000000)
 if name=='GX_SetDispCopyYScale':uc.reg_write(UC_PPC_REG_3,4)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,hardware)
def begin():
 assert call('gs_gx_surface_begin',surface_addr,0,64,64,4)==1
 u.mem_write(surface_addr+24,b'\x03');u.mem_write(surface_addr+24+640*512,b'\x7f')
 word(syms['surface_texture_valid'],0)
begin();events=[]
assert call('resolve_surface',0,syms['g_gs_mem'],4*1024*1024)==1
assert events==['GX_CopyTex','GX_PixModeSync','GX_InvalidateTexAll','GX_DrawDone','DCInvalidateRange'],events
assert bytes(u.mem_read(syms['g_gs_mem'],4))==bytes([17,34,51,127])
# Resident presentation makes two GPU copies, but only one CPU wait for XFB.
begin();events=[]

try:assert call('gs_gx_present',xfb,mode,0,64,0,0,64,4)==1
except Exception:
 print('GX_PC',hex(u.reg_read(UC_PPC_REG_PC)), 'EVENTS',events,flush=True);raise
assert events.count('GX_CopyTex')==2 and events.count('GX_PixModeSync')==2
assert events.count('GX_DrawDone')==1 and events.index('GX_CopyDisp')<events.index('GX_DrawDone'),events
assert not pending
# The saved snapshot is already valid. CPU import still fences ownership.
events=[];assert call('resolve_surface',0,syms['g_gs_mem'],4*1024*1024)==1
assert events==['GX_DrawDone','DCInvalidateRange'],events
print('PASS deferred GX FIFO: copy->PixModeSync->sample order, one present fence, fresh/cached snapshot readback fence and exact RGB/alpha import')
