"""Linked PPC hybrid GX submission. GX calls mocked; no GPU pixel/FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
u.mem_map(0x90000000,16*1024*1024);heap=0x90000000;heap_limit=0x91000000
u.mem_map(0xcc008000,4096)
xy=0x817a0000;pipe=xy+64;sample=xy+256
u.mem_write(xy,struct.pack('>6i',2,3,30,3,2,29))
u.mem_write(pipe,struct.pack('>14I',16384,0,1,1,1,128,1,0,1,0,1,128,0,1))
u.mem_write(sample,struct.pack('>7I',0x2c03001f,0x4180000c,0x38600000,0x4e800020,0x3c608011,0x60632233,0x4e800020))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'call.c';exe=Path(td)/'call.elf';raw=Path(td)/'call.bin'
 src.write_text('#include "core/hw/gs_gx.h"\nint run(gs_gx_texel_fn s,const int *xy,const gs_gx_pipeline *p){return gs_gx_draw_shaded_triangle(0,0,64,2,3,30,29,xy,0,s,p);}\n')
 subprocess.run([cc,'-O2','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817c0000','-Wl,-e,run','-Wl,--defsym,gs_gx_draw_shaded_triangle='+hex(syms['gs_gx_draw_shaded_triangle']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text',str(exe),str(raw)],check=True)
 u.mem_write(0x817c0000,raw.read_bytes());syms['test_run']=0x817c0000
begins=[];gxs={v:k for k,v in syms.items() if k.startswith(('GX_','ps_guMtx')) or k=='guOrtho'}
def gpu(uc,address,size,user):
 if address not in gxs:return
 if gxs[address]=='GX_Begin':begins.append(uc.reg_read(UC_PPC_REG_5))
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,gpu)
for name,value in [('initialized',1),('render_enabled',1),('surface_enabled',1),('efb_width',640),('efb_height',64),('surface',0),('texture',0x91000000),('texture_in_flight',0)]:word(syms[name],value)
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<7))
call('gs_mem_init')
# Exercise the real cold-boot lifecycle rather than only forcing residency on.
assert call('gs_gx_reset_boot_state')==1
assert int.from_bytes(u.mem_read(syms['surface_enabled'],4),'big')==1
word(syms['render_enabled'],1)
# Unsupported destination-alpha blending can reject before seeding in the
# native-additive build. If it seeds first, ownership must remain tracked.
call('gs_mem_write_psmct32',0,64,4,4,0x80112233)
word(pipe+2*4,0);word(pipe+3*4,0);word(pipe+9*4,1)
if 'surface_texture_valid' in syms:word(syms['surface_texture_valid'],1)
assert call('test_run',sample,xy,pipe)==0
seedptr=int.from_bytes(u.mem_read(syms['surface'],4),'big')
if call('gs_mem_gpu_pending'):
 assert seedptr and int.from_bytes(u.mem_read(syms['texture_in_flight'],4),'big')==1
 if 'surface_texture_valid' in syms:assert int.from_bytes(u.mem_read(syms['surface_texture_valid'],4),'big')==0
else:
 assert not begins, 'early rejection submitted unowned GPU work'
 assert call('gs_mem_read_psmct32',0,64,4,4)==0x80112233
# A software access must fence and release any seeded target before writing.
call('gs_mem_write_psmct32',0,64,0,0,0x11223344)
assert call('gs_mem_read_psmct32',0,64,0,0)==0x11223344
assert call('gs_mem_gpu_pending')==0
if seedptr:assert int.from_bytes(u.mem_read(seedptr+16,4),'big')==0
call('gs_mem_init')
word(pipe+2*4,1);word(pipe+3*4,1);word(pipe+9*4,0);word(pipe+6*4,0)
try:
 assert call('test_run',sample,xy,pipe)==1
except Exception:
 pc=u.reg_read(UC_PPC_REG_PC);print('PC',hex(pc),sorted([(pc-v,k) for k,v in syms.items() if v<=pc])[:3]);raise
assert len(begins)>=2
call('gs_gx_shaded_triangle_stat',1);assert u.reg_read(UC_PPC_REG_4)==1
surfaceptr=int.from_bytes(u.mem_read(syms['surface'],4),'big');assert surfaceptr
# alpha metadata: inside is exact 128; outside retains old zero.
alpha=surfaceptr+24+640*512
assert bytes(u.mem_read(alpha+4*64+4,1))==b'\x80'
assert bytes(u.mem_read(alpha,1))==b'\0'
# A depth read does not overlap the resident color target.
assert call('gs_mem_read_z',16384,64,4,4,0)==128
# Destination-alpha coefficient that varies cannot be sent as a fixed blend.
# First verify unsupported dimensions and masked scanlines return safely.
word(syms['render_enabled'],0);assert call('test_run',sample,xy,pipe)==0
word(syms['surface_enabled'],0) # setter is inlined by the Wii build
assert call('gs_gx_reset_boot_state')==1
assert int.from_bytes(u.mem_read(syms['surface_enabled'],4),'big')==0
print('PASS real linked PPC submits resident textured-triangle GX commands, exact alpha/depth metadata, uncovered alpha preserved, disabled GX rejects; EFB pixels and Wii speed unverified')
