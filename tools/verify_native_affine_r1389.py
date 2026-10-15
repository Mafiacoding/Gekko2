"""Linked PPC native texture submission; GX modeled, no physical pixel claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
u.mem_map(0x90000000,16*1024*1024);heap=0x90000000;heap_limit=0x91000000
u.mem_map(0xcc008000,4096)
xy=0x817a0000;uv=xy+32;pipe=xy+64;sample=xy+256;counter=0x817b0000
u.mem_write(xy,struct.pack('>6i',0,0,64,0,0,64));u.mem_write(uv,struct.pack('>6f',0,0,1,0,0,1))
u.mem_write(pipe,struct.pack('>14I',16384,0,0,0,1,128,0,0,2,2,1,32,0,1))
code=[0x3ca0817b,0x80c50000,0x38c60001,0x90c50000,0x3c608080,0x60638080,0x4e800020]
u.mem_write(sample,struct.pack('>'+str(len(code))+'I',*code))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'call.c';exe=Path(td)/'call.elf';raw=Path(td)/'call.bin'
 src.write_text('#include "core/hw/gs_gx.h"\nint run(gs_gx_texel_fn s,const int *xy,const float *uv,const gs_gx_pipeline *p){return gs_gx_draw_native_stq_triangle(0,0,64,0,0,63,63,xy,uv,0,0,16,16,1,1,1,0,s,p,0,0); }\nint runstq(gs_gx_texel_fn s,const int *xy,const float *uv,const gs_gx_pipeline *p){return gs_gx_draw_native_stq_triangle(0,0,64,0,0,63,63,xy,uv,(const float*)0x817a0080,(const unsigned*)0x817a00a0,16,16,1,1,1,0,s,p,0,0); }\n')
 src.write_text(src.read_text()+'unsigned vary(int x,int y){(*(volatile unsigned*)0x817b0000)++;return ((x?0x40u:0x80u)<<24)|0x00808080u;}\n')
 src.write_text(src.read_text()+('int runbound'+src.read_text().split('int runstq',1)[1].split('unsigned vary',1)[0]).replace('s,p,0,0);','s,p,1,0);'))
 src.write_text(src.read_text()+'int runraw(gs_gx_texel_fn s,const int *xy,const float *uv,const gs_gx_pipeline *p){return gs_gx_draw_native_stq_triangle(0,0,64,0,0,63,63,xy,uv,0,0,16,16,1,1,1,0,s,p,0,(const gs_gx_native_color*)0x817a00c0); }\n')
 src.write_text(src.read_text()+'int runrawq(gs_gx_texel_fn s,const int *xy,const float *uv,const gs_gx_pipeline *p){return gs_gx_draw_native_stq_triangle(0,0,64,0,0,63,63,xy,uv,(const float*)0x817a0080,0,16,16,1,1,1,0,s,p,0,(const gs_gx_native_color*)0x817a00c0); }\n')
 subprocess.run([cc,'-O2','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817c0000','-Wl,-e,run','-Wl,--defsym,gs_gx_draw_native_stq_triangle='+hex(syms['gs_gx_draw_native_stq_triangle']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text',str(exe),str(raw)],check=True)
 u.mem_write(0x817c0000,raw.read_bytes());syms['test_native']=0x817c0000
 for row in subprocess.check_output([a.nm,str(exe)],text=True).splitlines():
  if row.endswith(' runstq'):syms['test_stq']=int(row.split()[0],16)
  if row.endswith(' runbound'):syms['test_bound']=int(row.split()[0],16)
  if row.endswith(' vary'):syms['test_vary']=int(row.split()[0],16)
  if row.endswith(' runraw'):syms['test_raw']=int(row.split()[0],16)
  if row.endswith(' runrawq'):syms['test_rawq']=int(row.split()[0],16)
events=[];begins=[];matrices=[];gxs={v:k for k,v in syms.items() if k.startswith(('GX_','ps_guMtx')) or k in ('guOrtho','gs_gx_dump_flush')}
def gpu(uc,address,size,user):
 if address not in gxs:return
 name=gxs[address];args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(6)]
 if name=='gs_gx_dump_flush':uc.reg_write(UC_PPC_REG_3,1)
 events.append((name,args))
 if name=='GX_Begin':begins.append(args[:3])
 if name=='GX_LoadTexMtxImm':matrices.append(struct.unpack('>12f',uc.mem_read(args[0],48)))
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,gpu)
for name,value in [('initialized',1),('render_enabled',1),('surface_enabled',1),('efb_width',640),('efb_height',64),('surface',0),('texture',0x91000000),('texture_in_flight',0)]:word(syms[name],value)
call('gs_mem_init');word(counter,0)
assert call('test_native',sample,xy,uv,pipe)==1
assert int.from_bytes(u.mem_read(counter,4),'big')==256,'CPU shaded screen pixels instead of 16x16 source texels'
native_batches=[x for x in begins if x[0]==0x90]
assert native_batches and all(x[2]==3 for x in native_batches)
assert any(n=='GX_InitTexObjLOD' and a[1:3]==[1,1] for n,a in events),'linear filtering not on GX'
assert call('gs_mem_gpu_pending')==1
# Uniform GS alpha is authoritative CPU metadata despite RGB8 EFB.
surface=int.from_bytes(u.mem_read(syms['surface'],4),'big')
alpha_offset=24+640*512
assert u.mem_read(surface+alpha_offset+1,1)==b'\x80'
# A differing second alpha must reject after TWO samples, not the whole texture.
saved=len(begins);word(counter,0)
assert call('test_native',syms['test_vary'],xy,uv,pipe)==0
assert int.from_bytes(u.mem_read(counter,4),'big')==2
assert len(begins)==saved and u.mem_read(surface+alpha_offset+1,1)==b'\x80'
# Raw varying-alpha textures now submit GX RGB, with exact GS alpha metadata.
u.mem_write(xy+192,struct.pack('>3I',0x40808080,1,0));word(counter,0)
assert call('test_raw',syms['test_vary'],xy,uv,pipe)==1
assert int.from_bytes(u.mem_read(counter,4),'big')==256
assert u.mem_read(surface+alpha_offset+4+64,1)==b'\x30'
assert u.mem_read(surface+alpha_offset+8+64,1)==b'\x20'
assert u.mem_read(surface+alpha_offset+63+63*64,1)==b'\0','uncovered alpha changed'
import math
alpha_checks=0
for projective in (False,True):
 u.mem_write(xy+128,struct.pack('>3f',1,2,4))
 assert call('test_rawq' if projective else 'test_raw',syms['test_vary'],xy,uv,pipe)==1
 for yy in range(64):
  for xx in range(64-yy):
   b0=(4096-64*xx-64*yy)/4096;b1=xx/64;b2=yy/64
   qi=b0+2*b1+4*b2 if projective else 1
   tx=(b1*(1.0/qi))*16-0.5;ix=math.floor(tx);fx=int((tx-ix)*16)
   a0=128 if max(0,min(15,ix))==0 else 64
   a1=128 if max(0,min(15,ix+1))==0 else 64
   want=(((a0*(16-fx)+a1*fx)>>4)*64)//128
   got=u.mem_read(surface+alpha_offset+xx+64*yy,1)[0]
   assert got==want,(projective,xx,yy,got,want)
   alpha_checks+=1
# Repeat with source-alpha blending: whole asset max GS alpha is 64 <= 128.
word(pipe+6*4,1);word(pipe+7*4,0);word(pipe+8*4,1);word(pipe+9*4,0)
assert call('test_raw',syms['test_vary'],xy,uv,pipe)==1
word(pipe+8*4,2);event_start=len(events)
assert call('test_raw',syms['test_vary'],xy,uv,pipe)==1
assert any(n=='GX_SetNumTevStages' and a[0]==2 for n,a in events[event_start:])
assert any(n=='GX_SetTevColorIn' and a[:5]==[1,15,0,9,15] for n,a in events[event_start:])
assert any(n=='GX_SetBlendMode' and a[:3]==[1,1,1] for n,a in events[event_start:])
assert struct.unpack('>Q',u.mem_read(syms['native_affine_stats']+80,8))[0]==1
# RGB-only textures must preserve constant vertex alpha without a pixel loop.
word(xy+196,0);before_alpha=struct.unpack('>Q',u.mem_read(syms['native_affine_stats']+72,8))[0]
assert call('test_raw',syms['test_vary'],xy,uv,pipe)==1
assert u.mem_read(surface+alpha_offset+4+64,1)==b'\x40'
assert struct.unpack('>Q',u.mem_read(syms['native_affine_stats']+72,8))[0]==before_alpha
word(xy+196,1)
word(pipe+6*4,0);word(pipe+8*4,2);word(pipe+9*4,2)
# Execute actual projective matrix construction and GX Gouraud submission.
u.mem_write(xy+128,struct.pack('>3f',1,2,4))
u.mem_write(xy+160,struct.pack('>3I',0x80000080,0x80008000,0x80800000))
word(counter,0)
assert call('test_stq',sample,xy,uv,pipe)==1
assert int.from_bytes(u.mem_read(counter,4),'big')==256
assert matrices,'STQ division not delegated through GX texture generator'
m=matrices[-1]
for x,y,S,T,Q in [(0.5,0.5,0,0,1),(64.5,0.5,1,0,2),(0.5,64.5,0,1,4)]:
 assert all(abs(m[k*4]*x+m[k*4+1]*y+m[k*4+3]-f)<1e-5 for k,f in enumerate((S,T,Q)))
assert any(n=='GX_SetVtxDesc' and a[:2]==[11,1] for n,a in events),'vertex RGB not submitted'
assert struct.unpack('>2Q',u.mem_read(syms['native_affine_stats']+48,16))==(2,1)
saved=len(begins);word(xy+128,0);word(counter,0)
assert call('test_stq',sample,xy,uv,pipe)==0 and len(begins)==saved
assert int.from_bytes(u.mem_read(counter,4),'big')==0,'invalid Q mutated target'
word(xy+128,0x3f800000);word(xy+164,0x40008000)
assert call('test_stq',sample,xy,uv,pipe)==0 and len(begins)==saved
# Conservative varying-Z admission: never bypass a failed depth sample.
word(xy+164,0x80008000);word(pipe+2*4,1);word(pipe+4*4,2)
assert call('test_bound',sample,xy,uv,pipe)==1
call('gs_mem_write_z',16384,64,1,1,0,200)
saved=len(begins);word(counter,0)
assert call('test_bound',sample,xy,uv,pipe)==0 and len(begins)==saved
assert int.from_bytes(u.mem_read(counter,4),'big')==0,'failed depth decoded source'
word(pipe+2*4,0)
# Classic GS source-over with constant FIX is performed by GX blending.
word(pipe+6*4,1);word(pipe+7*4,0);word(pipe+8*4,1)
assert call('test_native',sample,xy,uv,pipe)==1
assert any(n=='GX_SetBlendMode' and a[:3]==[1,4,5] for n,a in events)
assert u.mem_read(surface+alpha_offset+1,1)==b'\x80','GX coefficient destroyed GS alpha'
saved=len(begins);word(pipe+6*4,1);word(pipe+7*4,1);word(counter,0)
assert call('test_native',sample,xy,uv,pipe)==0 and len(begins)==saved
assert int.from_bytes(u.mem_read(counter,4),'big')==0,'unsupported blend mutated source/target'
word(pipe+6*4,0);word(syms['render_enabled'],0)
assert call('test_native',sample,xy,uv,pipe)==0
word(syms['requested'],0);call('gekko2_opt_apply')
assert int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')==((1<<7)|(1<<12))
assert int.from_bytes(u.mem_read(syms['requested'],4),'big')==0,'test mode overwrote preferences'
word(syms['g_gpu_pending'],0);assert call('gs_gx_reset_boot_state')==1
assert u.mem_read(syms['native_affine_stats'],88)==b'\0'*88,'cold boot kept native counters'
print(json.dumps({'varying_alpha_GPU_submission_verified':True,'variable_alpha_shadow_verified':True,'alpha_oracle_checks':alpha_checks,'varying_source_alpha_blend_verified':True,'additive_source_alpha_TEV_verified':True,'TCC0_constant_alpha_verified':True,'native_counters_reset_on_cold_boot':True,'early_alpha_reject_two_samples':True,'failed_depth_zero_samples':True,'native_affine_PPC_submission_passed':True,'projective_STQ_matrix_verified':True,'Gouraud_submission_verified':True,'invalid_Q_and_alpha_fallback_verified':True,'conservative_variable_Z_guard_verified':True,'source_over_GX_blend_verified':True,'GX_session_force_verified':True,'CPU_source_texels':256,'output_bbox_pixels':4096,'GPU_triangle_batches':len(native_batches),'physical_Wii_GPU_pixels_verified':False}))
