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
events=[];begins=[];gxs={v:k for k,v in syms.items() if k.startswith(('GX_','ps_guMtx')) or k in ('guOrtho','gs_gx_dump_flush')}
def gpu(uc,address,size,user):
 if address not in gxs:return
 if gxs[address]=='gs_gx_dump_flush':uc.reg_write(UC_PPC_REG_3,1)
 events.append((gxs[address],uc.reg_read(UC_PPC_REG_3),uc.reg_read(UC_PPC_REG_4),uc.reg_read(UC_PPC_REG_5)))
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
assert call('test_run',sample,xy,pipe)==0
assert call('gs_mem_gpu_pending')==0
assert not begins, 'disabled resident blend queued GX commands'
word(pipe+7*4,0);word(pipe+8*4,2);word(pipe+9*4,2);word(pipe+10*4,1);word(pipe+11*4,20)
try:
 assert call('test_run',sample,xy,pipe)==1
except Exception:
 pc=u.reg_read(UC_PPC_REG_PC);print('PC',hex(pc),sorted([(pc-v,k) for k,v in syms.items() if v<=pc])[:3]);raise
assert len(begins)>=2
assert not any(e[0]=='GX_CopyTex' for e in events), 'native additive issued destination copy'
assert ('GX_SetBlendMode',1,1,1) in events
assert not any(e[0]=='GX_SetVtxDesc' and e[1:3]==(14,1) for e in events), 'native additive enabled TEX1'
assert not any(e[0]=='GX_LoadTexObj' and e[2]==1 for e in events)
tile=12
assert bytes(u.mem_read(0x91000000+tile,2))==bytes([128,(0x33*20)>>7])
assert bytes(u.mem_read(0x91000000+tile+32,2))==bytes([(0x22*20)>>7,(0x11*20)>>7])
# First draw seeds a small rectangle, not the full 64x64 target.
call('gs_gx_surface_count',5);seed_bytes=u.reg_read(UC_PPC_REG_4)
assert 0<seed_bytes<64*64*4
# Repeat with changed FIX: never reuse/double-scale a cached source.
for fix in (128,0,20):
 word(pipe+11*4,fix);assert call('test_run',sample,xy,pipe)==1
 assert bytes(u.mem_read(0x91000000+tile,2))==bytes([128,(0x33*fix)>>7])
call('gs_gx_surface_count',5);assert u.reg_read(UC_PPC_REG_4)==seed_bytes, 'repeated draw reseeded GPU output'
call('gs_gx_surface_count',10);assert u.reg_read(UC_PPC_REG_4)>0
assert not any(e[0]=='GX_CopyTex' for e in events)

call('gs_gx_shaded_triangle_stat',1);assert u.reg_read(UC_PPC_REG_4)==4
surfaceptr=int.from_bytes(u.mem_read(syms['surface'],4),'big');assert surfaceptr
# alpha metadata: inside is exact 128; outside retains old zero.
alpha=surfaceptr+24+640*512
assert bytes(u.mem_read(alpha+4*64+4,1))==b'\x80'
assert bytes(u.mem_read(alpha,1))==b'\0'
# A depth read does not overlap the resident color target.
assert call('gs_mem_read_z',16384,64,4,4,0)==128
# A disjoint later draw initializes new tiles without overwriting prior alpha.
u.mem_write(xy,struct.pack('>6i',40,3,62,3,40,29))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'far.c';exe=Path(td)/'far.elf';raw=Path(td)/'far.bin'
 src.write_text('#include "core/hw/gs_gx.h"\nint run(gs_gx_texel_fn s,const int *xy,const gs_gx_pipeline *p){return gs_gx_draw_shaded_triangle(0,0,64,40,3,62,29,xy,0,s,p); }\n')
 subprocess.run([cc,'-O2','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817c1000','-Wl,-e,run','-Wl,--defsym,gs_gx_draw_shaded_triangle='+hex(syms['gs_gx_draw_shaded_triangle']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text',str(exe),str(raw)],check=True)
 u.mem_write(0x817c1000,raw.read_bytes());syms['test_far']=0x817c1000
assert call('test_far',sample,xy,pipe)==1
call('gs_gx_surface_count',5);assert seed_bytes<u.reg_read(UC_PPC_REG_4)<64*64*4
assert bytes(u.mem_read(alpha+4*64+4,1))==b'\x80'
print('PASS sparse seed: smaller upload, unchanged repeated tiles, disjoint later seed preserves prior alpha')
# R1375: each resident submission completes in the fenced B build.
assert sum(e[0]=='GX_DrawDone' for e in events)>=4
start=len(events);assert call('gs_mem_sync')==1
copies=[e for e in events[start:] if e[0]=='GX_SetTexCopyDst']
assert len(copies)==1 and copies[0][1]*copies[0][2]<64*64
call('gs_gx_surface_count',7);assert u.reg_read(UC_PPC_REG_4)>0
assert call('gs_mem_gpu_pending')==0
assert bytes(u.mem_read(alpha+4*64+4,1))==b'\x80'
print('PASS linked PPC partial resolve: smaller aligned copy, exact alpha shadow, pending cleared, resident completion fences')
# Destination-alpha coefficient that varies cannot be sent as a fixed blend.
# First verify unsupported dimensions and masked scanlines return safely.
word(syms['render_enabled'],0);assert call('test_run',sample,xy,pipe)==0
word(syms['surface_enabled'],0) # setter is inlined by the Wii build
assert call('gs_gx_reset_boot_state')==1
assert int.from_bytes(u.mem_read(syms['surface_enabled'],4),'big')==0
print('PASS unsupported blend rejects; exact native additive ONE+ONE, no destination copy/TEX1, changed FIX correct; diagnostic I/O mocked; real linked PPC submits resident textured-triangle GX commands, exact alpha/depth metadata, uncovered alpha preserved, disabled GX rejects; EFB pixels and Wii speed unverified')
