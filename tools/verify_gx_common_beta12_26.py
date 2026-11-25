"""Execute linked PPC common shader construction and alpha state. GX is mocked."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_gx_extended_beta12_25.py').read_text(),'verify_gx_extended_beta12_25.py','exec'))
from unicorn import UC_HOOK_MEM_WRITE
import re
header=(Path(a.nm).parents[2]/'libogc/include/ogc/gx.h').read_text()
constants={m.group(1):int(m.group(2),0) for m in re.finditer(r'^#define\s+(GX_\w+)\s+(0x[0-9a-fA-F]+|\d+)\b',header,re.M)}
def gx(name):return constants['GX_'+name]
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'shader.c';exe=Path(td)/'shader.elf';raw=Path(td)/'shader.bin'
 src.write_text('''#include "core/hw/gs_gx.h"
int shader(const gs_gx_native_color *c,const unsigned *rgba,const gs_gx_pipeline *p,gs_gx_texel_fn sample,const int *xy,const unsigned *zs){
 float uv[6]={0,0,0,0,0,0};
 return gs_gx_draw_native_triangle_ex(0,0,64,2,2,18,18,xy,uv,0,rgba,4,4,1,1,0,0,sample,p,zs,c);
}
''')
 subprocess.run([cc,'-O2','-msdata=none','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817d0000','-Wl,-e,shader','-Wl,--defsym,gs_gx_draw_native_triangle_ex='+hex(syms['gs_gx_draw_native_triangle_ex']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text','-j','.rodata','-j','.sdata2','-j','.data',str(exe),str(raw)],check=True)
 u.mem_write(0x817d0000,raw.read_bytes());syms['test_shader']=0x817d0000
colorptr=base+4096;rgbaptr=colorptr+128
u.mem_write(rgbaptr,struct.pack('>3I',0x00808080,0x80808080,0xff808080))
u.mem_write(zs,struct.pack('>3I',128,128,128))
p=[16384,0,1,1,1,0,0,0,1,0,1,64,0,1,0,0,0,0,0,1,64,0]
u.mem_write(pipe,struct.pack('>22I',*p))
inputs={};ops={};orders={};kcolors={};ksel={};nstages=[];fifo=[]
def capture(uc,address,size,user):
 name=gxs.get(address)
 if name is None:return
 args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(7)]
 if name=='GX_SetTevColorIn':inputs[args[0]]=args[1:5]
 if name=='GX_SetTevColorOp':ops[args[0]]=args[1:7]
 if name=='GX_SetTevOrder':orders[args[0]]=args[1:4]
 if name=='GX_SetTevKColor':kcolors[args[0]]=int.from_bytes(uc.mem_read(args[1],4),'big')
 if name=='GX_SetTevKColorSel':ksel[args[0]]=args[1]
 if name=='GX_SetNumTevStages':nstages.append(args[0])
 if name=='GX_SetTevOp' and args[1]==gx('REPLACE'):
  inputs[args[0]]=[gx('CC_ZERO'),gx('CC_ZERO'),gx('CC_ZERO'),gx('CC_TEXC')]
  ops[args[0]]=[gx('TEV_ADD'),0,gx('CS_SCALE_1'),1,0,0]
def write_fifo(uc,access,address,size,value,user):fifo.append((size,value))
u.hook_add(UC_HOOK_CODE,capture)
u.hook_add(UC_HOOK_MEM_WRITE,write_fifo,begin=0xcc008000,end=0xcc008003)
def model_color(tex,va,fog_value,channel):
 prev=0
 for stage in range(nstages[-1]):
  ras= fog_value if orders[stage][2]==gx('COLOR1A1') else va
  selection=ksel.get(stage,gx('TEV_KCSEL_K0'))
  konst=(kcolors.get(1 if selection==gx('TEV_KCSEL_K1') else 0,0)>>(24-channel*8))&255
  values={gx('CC_ZERO'):0,gx('CC_CPREV'):prev,gx('CC_TEXC'):tex,gx('CC_RASC'):128,gx('CC_RASA'):ras,gx('CC_KONST'):konst}
  aa,bb,cc,dd=[values[n] for n in inputs[stage]]
  coefficient=cc+(cc>>7)
  scale=2 if ops[stage][2]==gx('CS_SCALE_2') else 1
  prev=max(0,min(255,((aa*(256-coefficient)+bb*coefficient+dd*256)*scale+128)//256))
 return prev
checks=0;max_rgb_model_error=0
for tfx in range(4):
 for fog in range(2):
  inputs.clear();ops.clear();orders.clear();kcolors.clear();ksel.clear();nstages.clear();fifo.clear()
  color=[0xff808080,1,tfx,64,fog,0x604020,0,128,255,0]
  u.mem_write(colorptr,struct.pack('>10I',*color));call('gs_gx_source_key',100+tfx,200+fog,1)
  assert call('test_shader',colorptr,rgbaptr,pipe,sample,base,zs)==1
  expected_stages=1+(tfx>=2)+2*fog
  assert nstages[-1]==expected_stages,(tfx,fog,nstages)
  if tfx>=2:
   assert inputs[1]==[gx('CC_RASA'),gx('CC_RASA'),gx('CC_ZERO'),gx('CC_CPREV')]
   assert orders[1][2]==gx('COLOR0A0')
  if fog:
   stage=1+(tfx>=2)
   assert inputs[stage]==[gx('CC_KONST'),gx('CC_CPREV'),gx('CC_RASA'),gx('CC_ZERO')]
   assert orders[stage][2]==gx('COLOR1A1')
   assert inputs[stage+1]==[gx('CC_ZERO'),gx('CC_CPREV'),gx('CC_KONST'),gx('CC_ZERO')]
   assert kcolors[0]==0x204060ff and kcolors[1]==0xfefefefe and ksel[stage]==gx('TEV_KCSEL_K0') and ksel[stage+1]==gx('TEV_KCSEL_K1')
  # Direct FIFO attributes must match CLR0, optional CLR1, then UV.
  stride=28 if fog else 24
  stream=b''.join(value.to_bytes(size,'big') for size,value in fifo)
  assert len(stream) and len(stream)%(stride*3)==0
  assert stream[12:16]==b'\x80\x80\x80\0'
  if fog:assert stream[16:20]==b'\0'*4 and stream[stride+16:stride+20]==b'\0\0\0\x80' and stream[stride*2+16:stride*2+20]==b'\0\0\0\xff'
  for y in range(3,16):
   for x in range(3,16):
    if x+y>=19:continue
    va=int((128*(x-2)+255*(y-2))/16+.5)
    expected=min(255,32*va//128) if tfx==0 else min(255,32+va) if tfx==2 else 32
    assert u.mem_read(alpha+y*64+x,1)==bytes([expected]),(tfx,fog,x,y,expected)
    for channel,tex in enumerate([0x33,0x22,0x11]):
     gs_color=tex if tfx<2 else min(255,tex+va)
     if fog:gs_color=(va*gs_color+(255-va)*[0x20,0x40,0x60][channel])>>8
     delta=abs(model_color(tex,va,va,channel)-gs_color)
     assert delta<=2,(tfx,fog,channel,va,delta)
     max_rgb_model_error=max(max_rgb_model_error,delta)
    checks+=1
# Untextured colors use passthrough, not a synthetic texture color multiply.
color=[0xff808080,0,0,64,1,0x604020,0,128,255,1]
u.mem_write(colorptr,struct.pack('>10I',*color));inputs.clear();call('gs_gx_source_key',300,400,1)
assert call('test_shader',colorptr,rgbaptr,pipe,sample,base,zs)==1
assert inputs[0]==[gx('CC_ZERO'),gx('CC_ZERO'),gx('CC_ZERO'),gx('CC_RASC')]
assert u.mem_read(alpha+4*64+4,1)==bytes([int((128*2+255*2)/16+.5)])
# Variable vertex alpha is safe for FIX blending; AS-dependent modulation declines.
p[6]=1;p[9]=2;u.mem_write(pipe,struct.pack('>22I',*p))
assert call('test_shader',colorptr,rgbaptr,pipe,sample,base,zs)==1
p[9]=0;u.mem_write(pipe,struct.pack('>22I',*p));before=samples;before_begins=len(begins)
assert call('test_shader',colorptr,rgbaptr,pipe,sample,base,zs)==0
assert samples==before and len(begins)==before_begins
print(f'PASS linked PPC common shader: {checks} exact alpha-shadow pixels, four TFX modes, varying alpha, fog TEV order/attenuation, FIFO color attributes, untextured passthrough, safe FIX blend and AS fallback; CPU TEV model max RGB error={max_rgb_model_error}/255; actual GPU RGB/FPS unverified')
