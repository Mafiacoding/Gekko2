"""Execute linked PPC triangle path; mock GX calls, never GPU pixel accuracy."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text().replace('else 10000000','else 100000000'),'fixture','exec'))
u.mem_map(0x90000000,16*1024*1024);heap=0x90000000;heap_limit=0x91000000
u.mem_map(0xcc008000,4096)
base=0x817a0000;pipe=base+128;zs=base+256;sample=base+512
# Small triangle; alternating texel alpha creates disjoint passing islands.
u.mem_write(base,struct.pack('>6i',2,2,18,2,2,18))
u.mem_write(base+32,struct.pack('>6f',0,0,1,0,0,1))
u.mem_write(zs,struct.pack('>3I',100,132,164))
u.mem_write(sample,struct.pack('>I',0x4e800020))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'call.c';exe=Path(td)/'call.elf';raw=Path(td)/'call.bin'
 src.write_text('''#include "core/hw/gs_gx.h"
int run(const int *xy,const float *uv,const unsigned *zs,const gs_gx_pipeline *p,gs_gx_texel_fn sample,unsigned h){
 gs_gx_native_color color={0x80808080,1,1,64};
 if(h==2||h==3){color.tcc=0;color.rgba=h==2?0x40808080:0x80808080;h=16;}
 return gs_gx_draw_native_triangle_ex(0,0,64,2,2,18,18,xy,uv,0,0,h==1024?128:16,h,0,0,0,0,sample,p,zs,&color);
}
''')
 subprocess.run([cc,'-O2','-msdata=none','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817c0000','-Wl,-e,run','-Wl,--defsym,gs_gx_draw_native_triangle_ex='+hex(syms['gs_gx_draw_native_triangle_ex']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text','-j','.rodata','-j','.sdata2','-j','.data',str(exe),str(raw)],check=True)
 u.mem_write(0x817c0000,raw.read_bytes());syms['test_run']=0x817c0000
samples=0;scissors=[];begins=[];blend_modes=[]
gxs={v:k for k,v in syms.items() if k.startswith(('GX_','ps_guMtx')) or k=='guOrtho'}
def gpu(uc,address,size,user):
 global samples
 if address==sample:
  samples+=1;x=uc.reg_read(UC_PPC_REG_3);uc.reg_write(UC_PPC_REG_3,(0x80 if x&1 else 0x20)<<24|0x112233)
 elif address in gxs:
  if gxs[address]=='GX_SetScissor':scissors.append(tuple(uc.reg_read(UC_PPC_REG_3+i) for i in range(4)))
  if gxs[address]=='GX_SetBlendMode':blend_modes.append(uc.reg_read(UC_PPC_REG_3))
  if gxs[address]=='GX_Begin':begins.append(uc.reg_read(UC_PPC_REG_5))
 else:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,gpu)
for name,value in [('initialized',1),('render_enabled',1),('surface_enabled',1),('efb_width',640),('efb_height',64),('surface',0),('texture',0x91000000),('texture_in_flight',0)]:word(syms[name],value)
word(syms['gekko2_optimization_mask'],0xffffffff)
call('gs_mem_init')
# Z GEQUAL, ATE GREATER(64), AFAIL KEEP. Z = 100 + 2*(x-2) + 4*(y-2).
p=[16384,0,1,1,2,0,0,0,1,0,1,128,0,1,0,0,0,0,1,6,64,0]
u.mem_write(pipe,struct.pack('>22I',*p))
call('gs_gx_source_key',1,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,16)==1
assert samples==256
surfaceptr=int.from_bytes(u.mem_read(syms['surface'],4),'big');alpha=surfaceptr+24+640*512
# Triangle coverage oracle independent from submitted run segmentation.
checked=0
for y in range(3,16):
 for x in range(3,16):
  if x+y>=19:continue
  passed=(x-2)&1
  z=call('gs_mem_read_z',16384,64,x,y,0)
  assert z==(100+2*(x-2)+4*(y-2) if passed else 0),(x,y,z)
  assert u.mem_read(alpha+y*64+x,1)==bytes([128 if passed else 0])
  checked+=1
# Every color scissor after the initial full-surface seed covers only passing columns.
segments=[r for r in scissors if r[2]<20 and r[0]>=2 and r[1]>=2]
assert segments and all(all((x-2)&1 for x in range(r[0],r[0]+r[2])) for r in segments),segments
before=samples;call('gs_gx_source_key',1,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,16)==1 and samples==before
call('gs_gx_source_key',3,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,16)==1 and samples==before+256
# DATE reads the exact alpha shadow from the preceding draw, never GX EFB alpha.
p[16]=1;p[17]=1;p[18]=0
u.mem_write(pipe,struct.pack('>22I',*p));scissors.clear()
call('gs_gx_source_key',3,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,16)==1
segments=[r for r in scissors if r[2]<20 and r[0]>=2 and r[1]>=2]
assert segments and all(all((x-2)&1 for x in range(r[0],r[0]+r[2])) for r in segments)
# Z-only alpha failure writes Z, emits no color triangles, and retains alpha.
p[16]=0;p[18]=1;p[19]=0;p[21]=2
u.mem_write(pipe,struct.pack('>22I',*p));before_begins=len(begins)
assert call('test_run',base,base+32,zs,pipe,sample,16)==1
assert len(begins)==before_begins
assert call('gs_mem_read_z',16384,64,4,4,0)==112
assert u.mem_read(alpha+4*64+4,1)==b'\0'
# PABE changes the blend command per permission run, not just per triangle.
p[6]=1;p[12]=1;p[18]=0;p[21]=0
u.mem_write(pipe,struct.pack('>22I',*p));scissors.clear();blend_modes.clear()
assert call('test_run',base,base+32,zs,pipe,sample,16)==1
assert 0 in blend_modes and 1 in blend_modes,blend_modes
p[6]=0;p[12]=0;p[18]=1;p[19]=6
u.mem_write(pipe,struct.pack('>22I',*p))
# Color/alpha transformation is part of the cache key even with identical VRAM.
p[18]=0;u.mem_write(pipe,struct.pack('>22I',*p));before=samples
call('gs_gx_source_key',3,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,2)==1 and samples==before+256
assert u.mem_read(alpha+4*64+4,1)==b'\x40'
call('gs_gx_source_key',3,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,3)==1 and samples==before+512
assert u.mem_read(alpha+4*64+4,1)==b'\x80'
# 1024-high textures admitted without pretending 1024x1024 fits the upload arena.
call('gs_gx_source_key',4,2,1)
assert call('test_run',base,base+32,zs,pipe,sample,1024)==1
assert samples>=128*1024
# Masked framebuffer stays on the oracle route, never silently ignored.
p[14]=1;u.mem_write(pipe,struct.pack('>22I',*p));before=samples
assert call('test_run',base,base+32,zs,pipe,sample,16)==0 and samples==before
print('PASS linked PPC: fragmented alpha coverage, interpolated Z, alpha shadow, texture reuse/invalidation, 128x1024 admission, FBMASK rejection; '+str(checked)+' interior pixels; GX mocked, RGB and speed unverified')
