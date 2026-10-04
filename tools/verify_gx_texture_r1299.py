"""Real linked PPC texture/depth/TEV submission, synthetic EFB services.
No physical Wii GPU rasterization or performance claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_vram_sync_r1289.py').read_text(),'verify_vram_sync_r1289.py','exec'))
import math
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.ppc_const import UC_PPC_REG_MSR
if 'R1300' in Path(a.elf).name:u.mem_write(syms['surface_enabled'],struct.pack('>I',0)) # test legacy TEV/depth capture independently
u.reg_write(UC_PPC_REG_MSR,0x2000);u.mem_map(0xcc008000,0x1000);u.mem_map(0x90400000,12*1024*1024)
u.mem_write(syms['texture'],struct.pack('>I',0x90400000))
next_alloc=[0x90600000];events=[];fifo_writes=[];orders={};colors={};ops={};alpha={};texobjects={};texmaps={}
alpha_ops={};gpu={'stages':1,'color':1,'z':(0,7,0),'atest':(7,0,0,7,0),'zbias':0};draws=[]
services={addr:name for name,addr in syms.items() if name.startswith('GX_')}
services.update({syms[n]:n for n in ['memalign','free','DCFlushRange','DCInvalidateRange','ps_guMtxIdentity']})
def rgba(ptr,w,x,y):
 t=((y//4)*(w//4)+x//4)*64+(y%4)*8+(x%4)*2
 ar=bytes(u.mem_read(ptr+t,2));gb=bytes(u.mem_read(ptr+t+32,2));return [ar[1],gb[0],gb[1],ar[0]]
def val(code,regs,tex):
 if code==15:return [0]*3
 if code==8:return tex[:3]
 if code==0:return regs[0][:3]
 if code in [2,4,6]:return regs[code//2][:3]
 raise AssertionError(('unexpected TEV input',code))
def evaluate(src,dst,depth,program):
 colors,ops,orders,alpha,alpha_ops=program
 regs=[[0]*4 for _ in range(4)]
 for stage in sorted(colors):
  mapid=orders.get(stage,(0,255))[1];tex=src if mapid==0 else dst if mapid==1 else depth if mapid==3 else [0]*4
  av,bv,cv,dv=[val(c,regs,tex) for c in colors[stage]];op,bias,scale,clamp,reg=ops[stage]
  assert bias==0 and scale in (0,3)
  output=[]
  for i in range(3):
   A,B,C=av[i]&255,bv[i]&255,cv[i]&255
   mix=((A<<8)+(B-A)*(C+(C>>7))+(0 if scale==3 else 128 if op==0 else 127))>>8
   v=dv[i]+mix if op==0 else dv[i]-mix
   if scale==3:v>>=1
   output.append(max(0,min(255,v)) if clamp else max(-1024,min(1023,v)))
  def ai(code):
   if code==7:return 0
   if code==4:return tex[3]
   if code in (0,1,2,3):return regs[code][3]
   raise AssertionError(('unexpected alpha input',code))
  aa,bb,cc,dd=[ai(c) for c in alpha[stage]]
  aop,abias,ascale,aclam,areg=alpha_ops[stage];assert abias==0 and ascale==0
  aa&=255;bb&=255;cc&=255
  amix=((aa<<8)+(bb-aa)*(cc+(cc>>7))+(128 if aop==0 else 127))>>8
  av=dd+amix if aop==0 else dd-amix
  av=max(0,min(255,av)) if aclam else max(-1024,min(1023,av))
  regs[reg][:3]=output;regs[areg][3]=av
 return regs[0]
def service(uc,address,size,user):
 name=services.get(address)
 if not name:return
 args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(8)];events.append((name,args))
 ret=0
 if name=='memalign':ret=next_alloc[0];next_alloc[0]+=0x200000
 if name=='ps_guMtxIdentity':uc.mem_write(args[0],struct.pack('>12f',1,0,0,0,0,1,0,0,0,0,1,0))
 if name=='GX_SetTevOrder':orders[args[0]]=(args[1],args[2])
 if name=='GX_SetTevColorIn':colors[args[0]]=args[1:5]
 if name=='GX_SetTevColorOp':ops[args[0]]=args[1:6]
 if name=='GX_SetTevAlphaIn':alpha[args[0]]=args[1:5]
 if name=='GX_SetTevAlphaOp':alpha_ops[args[0]]=args[1:6]
 if name=='GX_SetNumTevStages':gpu['stages']=args[0]
 if name=='GX_SetColorUpdate':gpu['color']=args[0]
 if name=='GX_SetZMode':gpu['z']=tuple(args[:3])
 if name=='GX_SetAlphaCompare':gpu['atest']=tuple(args[:5])
 if name=='GX_SetZTexture':gpu['zbias']=args[2]
 if name=='GX_Begin' and gpu['color']:
  stages=gpu['stages'];program=tuple({k:list(v) if isinstance(v,list) else v for k,v in record.items() if k<stages} for record in (colors,ops,orders,alpha,alpha_ops))
  draws.append((program,gpu['z'],gpu['atest'],gpu['zbias']))
 if name=='GX_SetTevOp':
  if args[1]==3:colors[args[0]]=[15,15,15,8];ops[args[0]]=[0,0,0,1,0];alpha[args[0]]=[7,7,7,4];alpha_ops[args[0]]=[0,0,0,1,0]
  elif args[1]==4:colors[args[0]]=[15,15,15,15];ops[args[0]]=[0,0,0,1,0];alpha[args[0]]=[7,7,7,0];alpha_ops[args[0]]=[0,0,0,1,0]
 if name=='GX_InitTexObj':texobjects[args[0]]=(args[1],args[2],args[3])
 if name=='GX_LoadTexObj':texmaps[args[1]]=texobjects[args[0]]
 if name=='GX_CopyTex':
  base=syms['texture_draw'];meta=struct.unpack('>8I',bytes(uc.mem_read(base,32)));bp,bw,dx,dy,ww,hh,_,psm=meta
  ox,oy,tw,th,nx,ny=struct.unpack('>2i4I',bytes(uc.mem_read(base+2080,24)))
  cols=struct.unpack('>'+str(nx)+'i',bytes(uc.mem_read(base+2104,nx*4)))
  rows=struct.unpack('>'+str(ny)+'i',bytes(uc.mem_read(base+4664,ny*4)))
  hwdepth,hwblend=struct.unpack('>2I',bytes(uc.mem_read(base+6784,8)))
  packed=bytearray(ww*hh*4);source=texmaps[0][0]
  if hwblend:dest=texmaps[1][0]
  def predicate(func,value,ref):
   return [False,value<ref,value==ref,value<=ref,value>ref,value!=ref,value>=ref,True][func]
  selected=list(draws);draws.clear()
  if hwdepth:
   depthptr=struct.unpack('>I',bytes(uc.mem_read(syms['depth_texture'],4)))[0]
  for y in range(ny):
   for x in range(nx):
    src=rgba(source,tw,cols[x]-ox,rows[y]-oy)
    dst=rgba(dest,ww,x,y) if hwblend else [0]*4
    dep=rgba(depthptr,ww,x,y) if hwdepth else [0]*4
    zvalue=(dep[0]<<16)|(dep[1]<<8)|dep[2];result=[0]*4
    for program,zmode,atest,zbias in selected:
     candidate=evaluate(src,dst,dep,program)
     lf,lref,operation,rf,rref=atest
     left=predicate(lf,candidate[3]&255,lref);right=predicate(rf,candidate[3]&255,rref)
     accepted=(left and right) if operation==0 else (left or right) if operation==1 else (left!=right) if operation==2 else (left==right)
     if not accepted:continue
     enabled,function,write=zmode
     if enabled and not predicate(function,zbias,zvalue):continue
     result=candidate
     if enabled and write:zvalue=zbias
    t=((y//4)*(ww//4)+x//4)*64+(y%4)*8+(x%4)*2
    packed[t:t+2]=bytes([255,result[0]]);packed[t+32:t+34]=bytes(result[1:3])
  uc.mem_write(args[0],bytes(packed))
 uc.reg_write(UC_PPC_REG_3,ret);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
def fifo(uc,access,address,size,value,user):fifo_writes.append((size,value))
h=u.hook_add(UC_HOOK_CODE,service);f=u.hook_add(UC_HOOK_MEM_WRITE,fifo,begin=0xcc008000,end=0xcc008003)
packet=0x81740000
def ad(reg,lo,hi=0):
 blob=struct.pack('<4I',0x8001,1<<28,14,0)+struct.pack('<4I',lo,hi,reg,0);u.mem_write(packet,blob);call('gif_process_quadwords',2,packet,2)
def texcolor(x,y):return (((x*31+y*17)&255)<<24)|((x*19+y*43)&255)|(((x*47+y*11)&255)<<8)|(((x*7+y*23)&255)<<16)
call('gs_gx_set_render_enabled',1)
def old_depth(x,y,frag,zpsm):
 if zpsm==0:
  base=frag&0xff000000
  return [base|127,base|128,base|129,((base-0x01000000)&0xffffffff)|129,((base+0x01000000)&0xffffffff)|127][(x+y)%5]
 if zpsm in (2,10):return [127,128,129,65535][(x+y)%4]
 return [127,128,129][(x+y)%3]
for psm,ztst,zpsm,frag,coeff,linear,pabe in [(0, 1, 0, 128, 128, False, 0), (0, 2, 1, 128, 64, False, 0), (1, 3, 0, 128, 0, False, 0), (0, 2, 0, 2147483776, 255, False, 0), (0, 2, 1, 128, 64, True, 0), (0, 2, 2, 128, 129, False, 0), (1, 3, 10, 128, 192, False, 0), (0, 2, 2, 2147483776, 255, False, 0), (0, 3, 10, 65536, 1, False, 0), (0, 1, 0, 128, 128, False, 1), (0, 2, 1, 128, 64, False, 1), (1, 3, 0, 128, 0, False, 1), (0, 2, 0, 2147483776, 255, False, 1), (0, 2, 1, 128, 64, True, 1), (0, 2, 2, 128, 129, False, 1), (1, 3, 10, 128, 192, False, 1), (0, 2, 2, 2147483776, 255, False, 1), (0, 3, 10, 65536, 1, False, 1)]:
 call('gs_mem_sync');call('gif_init');call('gs_mem_init');events.clear();fifo_writes.clear();orders.clear();colors.clear();ops.clear()
 for y in range(32):
  for x in range(32):call('gs_mem_write_psmct32',3000*64,64,x,y,texcolor(x,y))
 for y in range(32):
  for x in range(32):
   call('gs_mem_write_psmct32',0,64,x,y,0xab756341)
   call('gs_mem_write_z',16384,64,x,y,zpsm,old_depth(x,y,frag,zpsm))
 ad(0x4c,(1<<16)|(psm<<24));ad(0x18,0);ad(0x40,31<<16,31<<16)
 tex=3000|(1<<14)|(5<<26)|(5<<30)|(1<<34)|(1<<35);ad(6,tex&0xffffffff,tex>>32);ad(8,5);ad(0x14,0x60 if linear else 0)
 ad(0x4e,8|(zpsm<<24),0);ad(0x47,(1<<16)|(ztst<<17));ad(0x42,0x64,coeff)
 ad(0x49,pabe);ad(0,6|16|64|256);ad(1,0x80112233);ad(3,16|(16<<16) if linear else 0);ad(5,0,frag);ad(3,528|(528<<16) if linear else 128|(128<<16));ad(5,512|(512<<16),frag)
 assert call('gs_mem_gpu_pending')==1,(psm,ztst,coeff)
 assert any(n=='GX_InitTexObjLOD' and args[1:3]==[0,0] for n,args in events)
 depth_hw=(1 if zpsm in (1,2,10) else (0 if pabe else 1 if ztst in (0,1) else 2))
 base=syms['texture_draw'];flags=struct.unpack('>2I',bytes(u.mem_read(base+6784,8)))
 assert flags==(depth_hw,1),flags
 if depth_hw:
  assert any(n=='GX_SetZTexture' and args[:3]==[2,0x16,min(frag,65536) if zpsm in (2,10) else frag&0xffffff] for n,args in events)
  expected_func=7 if ztst==1 else 6 if ztst==2 else 4
  assert any(n=='GX_SetZMode' and args[:3]==[1,expected_func,1] for n,args in events)
 if depth_hw==2:
  assert any(n=='GX_SetAlphaCompare' and args[:2]==[1,frag>>24] for n,args in events)
  assert any(n=='GX_SetAlphaCompare' and args[:2]==[2,frag>>24] for n,args in events)
  assert sum(n=='GX_Begin' for n,args in events)==3
  highptr=texmaps[3][0]
  for xx,yy in [(0,0),(1,0),(2,0),(3,0),(4,0)]:
   assert rgba(highptr,32,xx,yy)[3]==old_depth(xx,yy,frag,zpsm)>>24
  assert [args[0] for n,args in events if n=='GX_SetNumTevStages'][-1]<=16
 if pabe:
  assert any(n=='GX_SetAlphaCompare' and args[:2]==[1,128] for n,args in events)
  assert any(n=='GX_SetAlphaCompare' and args[:2]==[6,128] for n,args in events)
  assert sum(n=='GX_Begin' for n,args in events)==(3 if depth_hw else 2)
  assert any(n=='GX_SetZCompLoc' and args[0]==0 for n,args in events) or not depth_hw
 assert not any(n=='GX_DrawDone' for n,args in events)
 assert call('gs_mem_sync')==1
 for y in range(32):
  for x in range(32):
   oldz=old_depth(x,y,frag,zpsm);passes=ztst==1 or (ztst==2 and frag>=oldz) or (ztst==3 and frag>oldz)
   src=texcolor(int(x*.25+.5),int(y*.25+.5));dst=0xab756341
   if linear:
    cs=[texcolor(min(31,x+dx),min(31,y+dy)) for dy in (0,1) for dx in (0,1)];src=0
    for shift in (0,8,16,24):
     hr=[(((cs[row*2]>>shift)&255)+((cs[row*2+1]>>shift)&255))//2 for row in (0,1)]
     src|=((hr[0]+hr[1])//2)<<shift
   rgb=0
   for shift in (0,8,16):
    S=(src>>shift)&255;D=(dst>>shift)&255;v=S if pabe and (src>>24)<128 else ((S-D)*coeff)//128+D;rgb|=max(0,min(255,v))<<shift
   expected=rgb|((dst if psm else src)&0xff000000) if passes else dst
   assert call('gs_mem_read_psmct32',0,64,x,y)==expected,(psm,ztst,coeff,x,y)
   assert call('gs_mem_read_z',16384,64,x,y,zpsm)==((frag&0xffff if zpsm in (2,10) else frag&0xffffff if zpsm==1 else frag) if passes else oldz)
for psm in [0,1]:
 call('gs_mem_sync');call('gif_init');call('gs_mem_init');events.clear();colors.clear();ops.clear();orders.clear()
 for y in range(32):
  for x in range(32):
   call('gs_mem_write_psmct32',0,64,x,y,0xab756341);call('gs_mem_write_z',16384,64,x,y,1,[127,128,129][(x+y)%3])
 ad(0x4c,(1<<16)|(psm<<24));ad(0x18,0);ad(0x40,31<<16,31<<16)
 ad(0x4e,8|(1<<24),0);ad(0x47,(1<<16)|(2<<17));ad(0x42,0x64,128);ad(0,3|64);ad(1,0x80112233)
 for x,y in [(0,0),(31,0),(0,31)]:ad(5,(x<<4)|((y<<4)<<16),128)
 assert call('gs_mem_gpu_pending')==1
 assert call('gs_mem_sync')==1
 for y in range(32):
  for x in range(32):
   oldz=[127,128,129][(x+y)%3];covered=x+y<=31 and oldz<=128
   assert call('gs_mem_read_psmct32',0,64,x,y)==((0xab112233 if psm else 0x80112233) if covered else 0xab756341),(psm,x,y)
   assert call('gs_mem_read_z',16384,64,x,y,1)==(128 if covered else oldz)
print('PASS linked PPC flat triangle depth+TEV blend, exact GS edge coverage and CT24 destination-alpha preservation')
print('PASS linked PPC textured GIF->GX, exact alpha/CT24, all FIX blend coefficients, Z24 hardware setup/Z32 preservation, deferred VRAM synchronization; GPU/cache services mocked')


# Actual linked shader programs, not a Python reproduction of the midpoint
# compiler. Exhaust every scalar source/destination pair for all FIX factors;
# exercise all 27 A/B/D selector programs with boundary/random-like pairs.
import numpy as np
src_all=np.repeat(np.arange(256,dtype=np.int32),256)
dst_all=np.tile(np.arange(256,dtype=np.int32),256)
edge=np.array([0,1,2,3,63,64,65,126,127,128,129,190,191,192,193,252,253,254,255],dtype=np.int32)
src_edge=np.repeat(edge,len(edge));dst_edge=np.tile(edge,len(edge))
def array_program(S,T):
 regs=[np.zeros_like(S) for _ in range(4)]
 def inp(code):
  if code==15:return np.zeros_like(S)
  if code==8:return None
  if code in (0,2,4,6):return regs[code//2]
  raise AssertionError(('unexpected color selector',code))
 for stage in sorted(colors):
  mapid=orders.get(stage,(0,255))[1]
  tex=S if mapid==0 else T if mapid==1 else np.zeros_like(S)
  values=[tex if code==8 else inp(code) for code in colors[stage]]
  av,bv,cv,dv=values;op,bias,scale,clamp,reg=ops[stage]
  assert bias==0 and scale in (0,3)
  A,B,C=av&255,bv&255,cv&255
  mix=((A<<8)+(B-A)*(C+(C>>7))+(0 if scale==3 else 128 if op==0 else 127))>>8
  result=dv+mix if op==0 else dv-mix
  if scale==3:result=result>>1
  regs[reg]=np.clip(result,0,255) if clamp else np.clip(result,-1024,1023)
 return regs[0]
def wide_call(name,args):
 u.reg_write(UC_PPC_REG_1,0x81780000)
 for i,value in enumerate(args[:8]):u.reg_write(UC_PPC_REG_3+i,value&0xffffffff)
 if len(args)>8:u.mem_write(0x81780008,struct.pack('>'+'I'*(len(args)-8),*[v&0xffffffff for v in args[8:]]))
 u.reg_write(UC_PPC_REG_LR,0x817ff000)
 u.emu_start(syms[name],0x817ff000,count=10000000)
 assert u.reg_read(UC_PPC_REG_PC)==0x817ff000
 return u.reg_read(UC_PPC_REG_3)
xy=packet+0x100;pipe=packet+0x200
u.mem_write(xy,struct.pack('>6i',0,0,3,0,0,3))
programs=0;comparisons=0
call('gs_mem_sync');call('gs_mem_init')
for aa in range(3):
 for bb in range(3):
  for dd in range(3):
   for coefficient in range(256):
    call('gs_mem_sync');events.clear();colors.clear();ops.clear();orders.clear()
    u.mem_write(pipe,struct.pack('>14I',0,0,0,0,1,0,1,aa,bb,2,dd,coefficient,0,1))
    assert wide_call('gs_gx_draw_flat_pipeline',[0,3,0,64,0,0,3,3,xy,0x80112233,0,pipe])==1
    stages=[args[0] for name,args in events if name=='GX_SetNumTevStages'][-1]
    assert stages<=16 and len(colors)==stages,(aa,bb,dd,coefficient,stages)
    assert struct.unpack('>2I',bytes(u.mem_read(syms['texture_draw']+6784,8)))==(0,1)
    S,T=(src_all,dst_all) if (aa,bb,dd)==(0,1,1) else (src_edge,dst_edge)
    A=S if aa==0 else T if aa==1 else np.zeros_like(S)
    B=S if bb==0 else T if bb==1 else np.zeros_like(S)
    D=S if dd==0 else T if dd==1 else np.zeros_like(S)
    expected=np.clip(((A-B)*coefficient)//128+D,0,255)
    result=array_program(S,T)
    assert np.array_equal(result,expected),(aa,bb,dd,coefficient,np.where(result!=expected)[0][:4])
    programs+=1;comparisons+=len(S)
call('gs_mem_sync')

# Uniform source/destination alpha can turn a dynamic GS coefficient into an
# exact fixed hardware program. Uniform PABE-low is a source pass-through;
# uniform PABE-high admits normal blending. Original caller state is immutable.
for cc,fix,source_alpha,dest_alpha,pabe,hardware in [
 (0,7,63,171,0,1),(0,7,192,171,0,1),(1,7,128,63,0,1),
 (1,7,128,192,0,1),(2,255,63,171,1,1),(2,129,192,171,1,1),
 (0,7,63,171,1,1),(0,7,192,171,1,1)]:
 call('gs_mem_sync');events.clear();colors.clear();ops.clear();orders.clear()
 src=(source_alpha<<24)|0x112233;dest=(dest_alpha<<24)|0x756341
 for y in range(4):
  for x in range(4):call('gs_mem_write_psmct32',0,64,x,y,dest)
 caller=struct.pack('>14I',0,0,0,0,1,0,1,0,1,cc,1,fix,pabe,1)
 u.mem_write(pipe,caller)
 assert wide_call('gs_gx_draw_flat_pipeline',[0,3,0,64,0,0,3,3,xy,src,0,pipe])==1
 assert bytes(u.mem_read(pipe,56))==caller
 assert struct.unpack('>2I',bytes(u.mem_read(syms['texture_draw']+6784,8)))==(0,hardware)
 assert call('gs_mem_sync')==1
 for y in range(4):
  for x in range(4):
   coefficient=source_alpha if cc==0 else dest_alpha if cc==1 else fix
   rgb=0
   for shift in (0,8,16):
    S=(src>>shift)&255;D=(dest>>shift)&255
    v=S if pabe and source_alpha<128 else max(0,min(255,((S-D)*coefficient)//128+D))
    rgb|=v<<shift
   expected=(src&0xff000000)|rgb if x+y<=3 else dest
   assert call('gs_mem_read_psmct32',0,64,x,y)==expected,(cc,fix,source_alpha,dest_alpha,pabe,x,y)
print('PASS uniform AS/AD and PABE threshold branches: actual GX submission, exact deferred VRAM and unchanged guest pipeline')
print(f'PASS {programs} actual linked TEV programs / {comparisons} independent scalar comparisons: all FIX 0..255, every A/B/D selector, signed rounding, boosted factors, single final clamp and <=16 stages')
u.hook_del(h);u.hook_del(f)
