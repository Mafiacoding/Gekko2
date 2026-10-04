"""Real linked PPC texture/depth/TEV submission, synthetic EFB services.
No physical Wii GPU rasterization or performance claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_vram_sync_r1289.py').read_text(),'verify_vram_sync_r1289.py','exec'))
import math
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.ppc_const import UC_PPC_REG_MSR
u.reg_write(UC_PPC_REG_MSR,0x2000);u.mem_map(0xcc008000,0x1000);u.mem_map(0x90400000,12*1024*1024)
u.mem_write(syms['texture'],struct.pack('>I',0x90400000))
next_alloc=[0x90600000];events=[];fifo_writes=[];orders={};colors={};ops={};alpha={};texobjects={};texmaps={}
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
def evaluate(src,dst):
 regs=[[0]*4 for _ in range(4)]
 for stage in sorted(colors):
  mapid=orders.get(stage,(0,255))[1];tex=src if mapid==0 else dst if mapid==1 else [0]*4
  av,bv,cv,dv=[val(c,regs,tex) for c in colors[stage]];op,bias,scale,clamp,reg=ops[stage]
  assert bias==0 and scale==0
  output=[]
  for i in range(3):
   A,B,C=av[i]&255,bv[i]&255,cv[i]&255
   mix=((A<<8)+(B-A)*(C+(C>>7))+(128 if op==0 else 127))>>8
   v=dv[i]+mix if op==0 else dv[i]-mix
   output.append(max(0,min(255,v)) if clamp else max(-1024,min(1023,v)))
  regs[reg]=output+[src[3]]
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
 if name=='GX_SetTevOp':
  if args[1]==3:colors[args[0]]=[15,15,15,8];ops[args[0]]=[0,0,0,1,0]
  elif args[1]==4:colors[args[0]]=[15,15,15,15];ops[args[0]]=[0,0,0,1,0]
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
  for y in range(ny):
   for x in range(nx):
    src=rgba(source,tw,cols[x]-ox,rows[y]-oy)
    result=evaluate(src,rgba(dest,ww,x,y)) if hwblend else src
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
for psm,ztst,zpsm,frag,coeff,linear in [(0,1,0,128,128,False),(0,2,1,128,64,False),(1,3,0,128,0,False),(0,2,0,0x80000080,255,False),(0,2,1,128,64,True)]:
 call('gs_mem_sync');call('gif_init');call('gs_mem_init');events.clear();fifo_writes.clear();orders.clear();colors.clear();ops.clear()
 for y in range(32):
  for x in range(32):call('gs_mem_write_psmct32',3000*64,64,x,y,texcolor(x,y))
 for y in range(32):
  for x in range(32):
   call('gs_mem_write_psmct32',0,64,x,y,0xab756341)
   call('gs_mem_write_z',16384,64,x,y,zpsm,[127,128,129][(x+y)%3])
 ad(0x4c,(1<<16)|(psm<<24));ad(0x18,0);ad(0x40,31<<16,31<<16)
 tex=3000|(1<<14)|(5<<26)|(5<<30)|(1<<34)|(1<<35);ad(6,tex&0xffffffff,tex>>32);ad(8,5);ad(0x14,0x60 if linear else 0)
 ad(0x4e,8|(zpsm<<24),0);ad(0x47,(1<<16)|(ztst<<17));ad(0x42,0x64,coeff)
 ad(0,6|16|64|256);ad(1,0x80112233);ad(3,16|(16<<16) if linear else 0);ad(5,0,frag);ad(3,528|(528<<16) if linear else 128|(128<<16));ad(5,512|(512<<16),frag)
 assert call('gs_mem_gpu_pending')==1,(psm,ztst,coeff)
 assert any(n=='GX_InitTexObjLOD' and args[1:3]==[0,0] for n,args in events)
 depth_hw=frag<=0xffffff
 base=syms['texture_draw'];flags=struct.unpack('>2I',bytes(u.mem_read(base+6784,8)))
 assert flags==(int(depth_hw),int(coeff in (0,128))),flags
 if depth_hw:
  assert any(n=='GX_SetZTexture' and args[:3]==[2,0x16,frag] for n,args in events)
  expected_func=7 if ztst==1 else 6 if ztst==2 else 4
  assert any(n=='GX_SetZMode' and args[:3]==[1,expected_func,1] for n,args in events)
 assert not any(n=='GX_DrawDone' for n,args in events)
 assert call('gs_mem_sync')==1
 for y in range(32):
  for x in range(32):
   oldz=[127,128,129][(x+y)%3];passes=ztst==1 or (ztst==2 and frag>=oldz) or (ztst==3 and frag>oldz)
   src=texcolor(int(x*.25+.5),int(y*.25+.5));dst=0xab756341
   if linear:
    cs=[texcolor(min(31,x+dx),min(31,y+dy)) for dy in (0,1) for dx in (0,1)];src=0
    for shift in (0,8,16,24):
     hr=[(((cs[row*2]>>shift)&255)+((cs[row*2+1]>>shift)&255))//2 for row in (0,1)]
     src|=((hr[0]+hr[1])//2)<<shift
   rgb=0
   for shift in (0,8,16):
    S=(src>>shift)&255;D=(dst>>shift)&255;v=((S-D)*coeff)//128+D;rgb|=max(0,min(255,v))<<shift
   expected=rgb|((dst if psm else src)&0xff000000) if passes else dst
   assert call('gs_mem_read_psmct32',0,64,x,y)==expected,(psm,ztst,coeff,x,y)
   assert call('gs_mem_read_z',16384,64,x,y,zpsm)==((frag&0xffffff if zpsm==1 else frag) if passes else oldz)
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
print('PASS linked PPC textured GIF->GX, exact alpha/CT24, TEV integer blend, hybrid coefficients, Z24 hardware setup/Z32 preservation, deferred VRAM synchronization; GPU/cache services mocked')
u.hook_del(h);u.hook_del(f)
