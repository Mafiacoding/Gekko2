"""Actual linked-PPC source packing and CPU flush publication.
Mocks cache-maintenance calls; no physical GPU execution or timing claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'layout.c';obj=Path(td)/'layout.o';raw=Path(td)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/hw/gs_gx.h"\nconst unsigned layout[]={sizeof(gs_gx_texture_draw),offsetof(gs_gx_texture_draw,origin_x),offsetof(gs_gx_texture_draw,origin_y),offsetof(gs_gx_texture_draw,tw),offsetof(gs_gx_texture_draw,th)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 dsize,ox,oy,tw,th=struct.unpack('>5I',raw.read_bytes())
desc=0x817a0000;sample=0x817d0000;pixels=ram+0x10000
u.mem_write(sample,struct.pack('>3I',0x3c601122,0x60633344,0x4e800020))
u.mem_write(desc,bytes(dsize));word(desc+tw,4);word(desc+th,4)
word(syms['texture'],pixels);word(syms['texture_in_flight'],0)
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<8))
flushes=[]
def record(uc,address,size,user):
 if address==syms['DCFlushRange']:flushes.append((uc.reg_read(UC_PPC_REG_3),uc.reg_read(UC_PPC_REG_4)))
u.hook_add(UC_HOOK_CODE,record)
def pack(key=1,valid=1):
 call('gs_gx_source_key',key,2,valid)
 assert call('source_pack',desc,sample)==64
 return bytes(u.mem_read(pixels,64))
first=pack();assert int.from_bytes(u.mem_read(syms['source_upload_pending'],4),'big')==1
# Rejected draw: the cache may hit before any successful publication.
assert pack()==first;call('source_upload',64);assert flushes==[(pixels,64)]
assert pack()==first;call('source_upload',64);assert len(flushes)==1
call('gs_gx_source_cache_count',3);assert u.reg_read(UC_PPC_REG_4)==64
# New source generation and invalid key must repack and flush, even if the
# synthetic sampler produces the same pixels.
assert pack(3)==first;call('source_upload',64);assert len(flushes)==2
assert pack(3,0)==first;call('source_upload',64);assert len(flushes)==3
word(desc+ox,1);assert pack(3)==first;call('source_upload',64);assert len(flushes)==4
print('PASS 6 source packs: miss/hit, deferred upload after rejection, generation/invalid-key/origin changes; unchanged pixels and four required CPU flushes')
# Fresh scanout is a different producer: it must always flush its new bytes.
# Stop at the first viewport command; no GPU rasterization is modeled here.
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'mode.c';obj=Path(td)/'mode.o';raw=Path(td)/'mode.bin'
 sdk=Path(a.nm).resolve().parents[2]
 src.write_text('#include <stddef.h>\n#include <ogc/gx_struct.h>\nconst unsigned layout[]={sizeof(GXRModeObj),offsetof(GXRModeObj,fbWidth),offsetof(GXRModeObj,efbHeight),offsetof(GXRModeObj,xfbHeight)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-DHW_RVL','-DGEKKO','-I'+str(sdk/'libogc/include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 msize,fw,eh,xh=struct.unpack('>4I',raw.read_bytes())
mode=0x817c0000;u.mem_write(mode,bytes(msize))
for off in [fw,eh,xh]:u.mem_write(mode+off,struct.pack('>H',4))
word(syms['initialized'],1);word(syms['surface'],0)
call('gs_mem_init')
def gx_stop(uc,address,size,user):
 if address==syms['GX_InvalidateTexAll']:
  uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
 elif address==syms['GX_SetViewport']:
  uc.reg_write(UC_PPC_REG_3,1);uc.reg_write(UC_PPC_REG_PC,0x817ff000)
u.hook_add(UC_HOOK_CODE,gx_stop)
assert call('gs_gx_present',pixels+1024,mode,0,64,0,0,4,4)==1
assert flushes[-1]==(pixels,64) and len(flushes)==5
print('PASS fresh scanout flushes repacked data before GX viewport; rendering not modeled')
