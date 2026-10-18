#!/usr/bin/env python3
"""Validate actual PPC FIFO vertex byte counts against modeled GX descriptors.
SDK configuration calls modeled; no GPU pixels or real Wii timing.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
from unicorn import UC_HOOK_MEM_WRITE
import hashlib
u.mem_map(0x90000000,16*1024*1024);heap=0x90000000;heap_limit=0x91000000
u.mem_map(0xcc008000,4096)
xy=0x817a0000;pipe=xy+64;sample=xy+256
u.mem_write(xy,struct.pack('>6i',2,3,30,3,2,29))
u.mem_write(pipe,bytes(128));u.mem_write(pipe,struct.pack('>14I',16384,0,0,0,1,128,0,0,1,0,1,128,0,1))
u.mem_write(sample,struct.pack('>2I',0x3c608011,0x60632233)+struct.pack('>I',0x4e800020))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'call.c';exe=Path(td)/'call.elf';raw=Path(td)/'call.bin'
 src.write_text('''#include "core/hw/gs_gx.h"
int run(gs_gx_texel_fn s,const int *xy,const gs_gx_pipeline *p,unsigned flags){
 unsigned z[3]={128,128,128};float uv[6]={0,0,1,0,0,1},q[3]={1,1,1};unsigned rgba[3]={0x80404040,0x80408040,0x80804040};
 gs_gx_native_color c={.rgba=0x80404040,.tcc=1,.tfx=(flags>>2)&3,.target_height=64,
 .fog_enabled=(flags>>1)&1,.fog_color=0x123456,.fog={64,128,255}};
 if(flags&16)return gs_gx_draw_shaded_triangle(0,0,64,2,3,30,29,xy,0,s,p);
 return gs_gx_draw_native_triangle_ex(0,0,64,2,3,30,29,xy,uv,q,flags&1?rgba:0,4,4,0,0,0,0,s,p,z,&c);
}''')
 subprocess.run([cc,'-O2','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817c0000','-Wl,-e,run','-Wl,--defsym,gs_gx_draw_native_triangle_ex='+hex(syms['gs_gx_draw_native_triangle_ex']),'-Wl,--defsym,gs_gx_draw_shaded_triangle='+hex(syms['gs_gx_draw_shaded_triangle']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text','-j','.rodata',str(exe),str(raw)],check=True)
 u.mem_write(0x817c0000,raw.read_bytes());syms['test_run']=0x817c0000
# Direct attributes used by the real rendering paths: POS XYZ F32, CLR RGBA8,
# TEX ST F32. Keep descriptor state across submissions like the real CP.
desc={};pending=None;draws=0;written=0;payload=hashlib.sha256();configuration=[]
names={v:k for k,v in syms.items() if k.startswith(('GX_','ps_guMtx')) or k in ('guOrtho','gs_gx_dump_flush')}
def end():
 global pending,draws
 if pending is None:return
 expected,count,attributes=pending
 assert written==expected,(count,attributes,expected,written)
 pending=None;draws+=1
def gpu(uc,address,size,user):
 global pending,written
 if address==syms['gx_vtx_report']:end();return
 name=names.get(address)
 if not name:return
 reg=lambda n:uc.reg_read(UC_PPC_REG_0+n)
 if name=='GX_ClearVtxDesc':desc.clear()
 if name=='GX_SetVtxDesc':desc[reg(3)]=reg(4)
 if name in ('GX_SetNumTexGens','GX_SetNumTevStages'):configuration.append((name,reg(3)))
 if name in ('GX_SetVtxDesc','GX_SetTevOp'):configuration.append((name,reg(3),reg(4)))
 if name in ('GX_SetBlendMode','GX_SetAlphaCompare','GX_SetTevColorIn','GX_SetTevAlphaIn'):configuration.append((name,*[reg(i) for i in range(3,8 if name!='GX_SetBlendMode' else 7)]))
 if name=='GX_Begin':
  end();n=reg(5);width=0
  for attr,kind in desc.items():
   if not kind:continue
   assert kind==1,(attr,kind)
   assert attr in (9,11,12,13,14),attr
   width+=12 if attr==9 else 4 if attr in (11,12) else 8
  pending=(n*width,n,dict(desc));written=0
 if name=='gs_gx_dump_flush':uc.reg_write(UC_PPC_REG_3,1)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
def fifo(uc,access,address,size,value,user):
 global written
 assert pending is not None,('vertex payload outside primitive',hex(address),size)
 written+=size;payload.update((value&((1<<(8*size))-1)).to_bytes(size,'big'))
u.hook_add(UC_HOOK_CODE,gpu);u.hook_add(UC_HOOK_MEM_WRITE,fifo,begin=0xcc008000,end=0xcc008003)
for name,value in [('initialized',1),('render_enabled',1),('surface_enabled',1),('efb_width',640),('efb_height',64),('surface',0),('texture',0x91200000),('texture_in_flight',0)]:word(syms[name],value)
word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|(1<<7))
call('gs_mem_init')
for blend in (0,1,2):
 word(pipe+6*4,bool(blend));word(pipe+8*4,2 if blend==2 else 1);word(pipe+10*4,1)
 for flag in range(16):
  # HIGHLIGHT adds vertex alpha: AS>128 cannot use source-alpha blend.
  expected=not(blend==1 and flag>>2==2)
  assert call('test_run',sample,xy,pipe,flag)==expected,(blend,flag)
  end()
print('PASS native GX vertex stream',draws,'primitives: color/fog/TFX and blend/additive-alpha combinations have exact descriptor byte counts; SDK modeled, no GPU pixel claim')
# Resident pre-shaded route uses the large descriptor; compare all stable
# shader arguments and actual FIFO payload across old/new ELFs.
for blend in (0,1):
 word(pipe+6*4,blend);word(pipe+8*4,2 if blend else 1);word(pipe+9*4,2);word(pipe+11*4,20)
 assert call('test_run',sample,xy,pipe,16)==1
 end()
print('GX_SEMANTICS',json.dumps({'vertex_sha256':payload.hexdigest(),'configuration_sha256':hashlib.sha256(json.dumps(configuration).encode()).hexdigest(),'primitives':draws},sort_keys=True))
