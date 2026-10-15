"""Linked PPC resident Gouraud routing/metadata. GX device calls are mocked;
this does not assert hardware pixel rasterization accuracy."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
from unicorn import UC_HOOK_MEM_INVALID
def bad(uc,access,address,size,value,user):
 pc=uc.reg_read(UC_PPC_REG_PC);print('BAD',hex(pc),max((v,k) for k,v in syms.items() if v<=pc),hex(address),flush=True);return False
u.hook_add(UC_HOOK_MEM_INVALID,bad)
u.mem_map(0xcc008000,0x1000)
u.reg_write(UC_PPC_REG_2,syms.get('_SDA2_BASE_',syms['__SDATA2_START__']+0x8000))
u.reg_write(UC_PPC_REG_13,syms.get('_SDA_BASE_',syms['__SDATA_START__']+0x8000))
gpu={v:k for k,v in syms.items() if k.startswith('GX_') or k in ('ps_guMtxIdentity','c_guMtxIdentity','guOrtho')}
gpu_calls=[]
def device(uc,address,size,user):
 if address not in gpu:return
 gpu_calls.append(gpu[address]);uc.reg_write(UC_PPC_REG_3,0)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,device)
surf=0x81600000;xy=0x817e0000;colors=xy+32;pipeline=xy+64
word(syms['surface'],surf)
for name,value in [('initialized',1),('render_enabled',1),('surface_enabled',1),('efb_width',128),('efb_height',64)]:word(syms[name],value)
assert call('gs_gx_surface_begin',surf,0,128,128,64)==1
assert call('gs_mem_gpu_bind',syms['resolve_surface'],0)==1
u.mem_write(xy,struct.pack('>6i',0,0,16,0,0,16))
u.mem_write(colors,struct.pack('>3I',0x80ff0000,0x8000ff00,0x800000ff))
u.mem_write(pipeline,bytes(64))
for iteration in range(2):
 word(0x81780000+8,colors);word(0x81780000+12,0);word(0x81780000+16,pipeline)
 assert call('gs_gx_draw_gouraud_triangle',0,0,128,0,0,16,16,xy)==1
 assert call('gs_mem_gpu_pending')==1
call('gs_gx_surface_count',6);assert u.reg_read(UC_PPC_REG_4)==2
call('gs_gx_surface_count',2);assert u.reg_read(UC_PPC_REG_4)==0
assert not any(n in ('GX_CopyTex','GX_DrawDone') for n in gpu_calls),gpu_calls
assert int.from_bytes(u.mem_read(surf+20,4),'big')>0
assert bytes(u.mem_read(surf+24+640*512+2*128+2,1))==b'\x80'
print('PASS 2 linked-PPC resident Gouraud submissions, dirty metadata, pending VRAM; no CopyTex/DrawDone/resolve')
