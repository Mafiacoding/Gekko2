"""Linked Wii PPC resident RGB/blend, exact CPU Z/alpha shadow oracle.
GX services and EFB are synthetic; no Dolphin, physical Wii or FPS claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_vram_sync_r1289.py').read_text(), 'verify_vram_sync_r1289.py', 'exec'))
import math
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.ppc_const import UC_PPC_REG_MSR
# Reuse the compact oracle's integer GX TEV evaluator, not its test driver.
oracle = Path(__file__).with_name('verify_gx_texture_r1299.py').read_text()
exec(compile(oracle[oracle.index('def rgba('):oracle.index('def service(')], 'tev_evaluator', 'exec'))
u.reg_write(UC_PPC_REG_MSR, 0x2000)
u.mem_map(0xcc008000, 0x1000);u.mem_map(0x90400000, 12*1024*1024)
u.mem_write(syms['texture'], struct.pack('>I',0x90400000))
u.mem_write(syms['efb_width'],struct.pack('>I',128));u.mem_write(syms['efb_height'],struct.pack('>I',64))
allocation=[0x90600000];events=[];fifo=[];batch=[None];texobjects={};texmaps={}
colors={};ops={};orders={};alpha={};alpha_ops={};state={'gens':1,'stages':1,'atest':(7,0,0,7,0)}
efb=bytearray(128*64*3);copy=[(0,0,0,0)];copy_history=[]
services={addr:name for name,addr in syms.items() if name.startswith('GX_')}
services.update({syms[name]:name for name in ['memalign','free','DCFlushRange','DCInvalidateRange','ps_guMtxIdentity']})
def tile(w,x,y):return ((y//4)*(w//4)+x//4)*64+(y%4)*8+(x%4)*2
def predicate(func,value,ref):return [False,value<ref,value==ref,value<=ref,value>ref,value!=ref,value>=ref,True][func]
def finish():
 if batch[0] is None:return
 count,gens,maps,program,atest=batch[0];stride=3+2*gens
 assert gens in (1,2) and len(fifo)==count*stride,(gens,len(fifo),count)
 verts=[]
 for i in range(count):verts.append([struct.unpack('>f',v.to_bytes(4,'big'))[0] for _,v in fifo[i*stride:(i+1)*stride]])
 for i in range(0,count,4):
  q=verts[i:i+4];x0,y0=map(round,q[0][:2]);x1,y1=map(round,q[2][:2])
  for y in range(y0,y1):
   for x in range(x0,x1):
    def sample(mapid,coord):
     ptr,w,h=maps[mapid];j=3+coord*2
     s=q[0][j]+(q[1][j]-q[0][j])*(x+.5-x0)/(x1-x0)
     t=q[0][j+1]+(q[3][j+1]-q[0][j+1])*(y+.5-y0)/(y1-y0)
     return rgba(ptr,w,max(0,min(w-1,int(s*w))),max(0,min(h-1,int(t*h))))
    result=evaluate(sample(0,0),sample(1,1) if gens==2 else [0]*4,[0]*4,program)
    lf,lr,op,rf,rr=atest;a=result[3]&255;left=predicate(lf,a,lr);right=predicate(rf,a,rr)
    accepted=left and right if op==0 else left or right if op==1 else left!=right if op==2 else left==right
    if accepted:efb[(y*128+x)*3:(y*128+x)*3+3]=bytes(result[:3])
 fifo.clear();batch[0]=None
def service(uc,address,size,user):
 name=services.get(address)
 if not name:return
 args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(8)];events.append(name);ret=0
 if name in ('GX_Begin','GX_CopyTex','GX_DrawDone'):finish()
 if name=='memalign':ret=allocation[0];allocation[0]+=0x200000
 if name=='ps_guMtxIdentity':uc.mem_write(args[0],struct.pack('>12f',1,0,0,0,0,1,0,0,0,0,1,0))
 if name=='GX_SetNumTexGens':state['gens']=args[0]
 if name=='GX_SetNumTevStages':state['stages']=args[0]
 if name=='GX_SetAlphaCompare':state['atest']=tuple(args[:5])
 if name=='GX_SetTevOrder':orders[args[0]]=args[1:3]
 if name=='GX_SetTevColorIn':colors[args[0]]=args[1:5]
 if name=='GX_SetTevColorOp':ops[args[0]]=args[1:6]
 if name=='GX_SetTevAlphaIn':alpha[args[0]]=args[1:5]
 if name=='GX_SetTevAlphaOp':alpha_ops[args[0]]=args[1:6]
 if name=='GX_SetTevOp':
  if args[1]==3:
   colors[args[0]]=[15,15,15,8];ops[args[0]]=[0,0,0,1,0]
   alpha[args[0]]=[7,7,7,4];alpha_ops[args[0]]=[0,0,0,1,0]
  else:raise AssertionError(('unexpected TEV op',args[1]))
 if name=='GX_InitTexObj':texobjects[args[0]]=tuple(args[1:4])
 if name=='GX_LoadTexObj':texmaps[args[1]]=texobjects[args[0]]
 if name=='GX_Begin':
  stages=state['stages']
  program=tuple({k:list(v) for k,v in record.items() if k<stages} for record in (colors,ops,orders,alpha,alpha_ops))
  batch[0]=(args[2],state['gens'],dict(texmaps),program,state['atest'])
 if name=='GX_SetTexCopySrc':copy[0]=tuple(args[:4])
 if name=='GX_CopyTex':
  sx,sy,w,h=copy[0];copy_history.append(copy[0]);packed=bytearray(w*h*4)
  for y in range(h):
   for x in range(w):
    t=tile(w,x,y);p=((sy+y)*128+sx+x)*3
    packed[t:t+2]=bytes([255,efb[p]]);packed[t+32:t+34]=efb[p+1:p+3]
  uc.mem_write(args[0],bytes(packed))
 uc.reg_write(UC_PPC_REG_3,ret);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
def capture(uc,access,address,size,value,user):fifo.append((size,value))
h=u.hook_add(UC_HOOK_CODE,service);f=u.hook_add(UC_HOOK_MEM_WRITE,capture,begin=0xcc008000,end=0xcc008003)
packet=0x81740000
def ad(reg,lo,hi=0):
 blob=struct.pack('<4I',0x8001,1<<28,14,0)+struct.pack('<4I',lo,hi,reg,0)
 u.mem_write(packet,blob);call('gif_process_quadwords',2,packet,2)
def counter(name,index):
 call(name,index);return (u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_3+1)
call('gs_gx_set_render_enabled',1)
comparisons=0;accepted=0
# Repeated sprites exercise source-cache reuse, resident GPU destination
# snapshots, mixed PABE passes and exact CPU metadata without RGB readback.
for case,(psm,zpsm,ztst,frag,cc,coeff,pabe) in enumerate([
 (0,0,1,0x80000080,2,64,0),(1,1,2,128,2,129,0),
 (0,2,3,128,2,255,0),(1,10,2,65536,2,1,0),
 (0,0,2,0x80000080,2,192,1),(1,1,3,128,2,64,1),
 (0,1,1,128,0,7,0),(0,1,1,128,1,7,0),
 (1,1,1,128,1,7,0),(0,1,1,128,2,0,1),
]):
 assert call('gs_mem_sync')==1
 call('gif_init');call('gs_mem_init');expected={};zexpected={}
 def source(x,y):
  a=150 if cc in (0,1) else [127,128,129][(x+y)%3]
  return (a<<24)|((x*19+y*43)&255)|(((x*47+y*11)&255)<<8)|(((x*7+y*23)&255)<<16)
 for y in range(16):
  for x in range(32):
   call('gs_mem_write_psmct32',3000*64,64,x,y,source(x,y))
   dest=0xab756341;expected[x,y]=dest
   old=frag-1 if 8<=x<24 else frag+1
   old&=0xffff if zpsm in (2,10) else 0xffffff if zpsm==1 else 0xffffffff
   zexpected[x,y]=old
   call('gs_mem_write_psmct32',0,128,x+4,y+3,dest)
   call('gs_mem_write_z',16384,128,x+4,y+3,zpsm,old)
 ad(0x4c,(2<<16)|(psm<<24));ad(0x18,0);ad(0x40,127<<16,63<<16)
 tex=3000|(1<<14)|(5<<26)|(4<<30)|(1<<34)|(1<<35)
 ad(6,tex&0xffffffff,tex>>32);ad(8,5);ad(0x14,0)
 ad(0x4e,8|(zpsm<<24));ad(0x47,(1<<16)|(ztst<<17));ad(0x42,0x44|(cc<<4),coeff)
 ad(0x49,pabe);ad(0,6|16|64|256);ad(1,0x80112233)
 events.clear();copy_history.clear();before=counter('gs_gx_resident_pipeline_count',1)
 for draw in range(4):
  ad(3,0);ad(5,64|(48<<16),frag);ad(3,512|(256<<16));ad(5,576|(304<<16),frag)
  assert call('gs_mem_gpu_pending')==1
  assert counter('gs_gx_resident_pipeline_count',1)==before+draw+1,(case,draw)
  assert events.count('DCInvalidateRange')==0,(case,events)
  for y in range(16):
   for x in range(32):
    old=zexpected[x,y];passes=ztst==1 or (ztst==2 and frag>=old) or (ztst==3 and frag>old)
    if passes:
     src=source(x,y);dst=expected[x,y];factor=coeff if cc==2 else src>>24 if cc==0 else 128 if psm else dst>>24
     rgb=0
     for shift in (0,8,16):
      S=(src>>shift)&255;D=(dst>>shift)&255
      value=S if pabe and src>>24<128 else ((S-D)*factor)//128+D
      rgb|=max(0,min(255,value))<<shift
     expected[x,y]=rgb|((dst if psm else src)&0xff000000)
     zexpected[x,y]=frag&(0xffff if zpsm in (2,10) else 0xffffff if zpsm==1 else 0xffffffff)
  # Shadow Z reads are disjoint, so they cannot close the color owner.
  assert call('gs_mem_read_z',16384,128,4,3,zpsm)==zexpected[0,0]
  assert call('gs_mem_gpu_pending')==1 and events.count('DCInvalidateRange')==0
  accepted+=1
 assert copy_history==[(4,3,32,16)]*4,('GPU destination rectangle',case,copy_history)
 for i,name in enumerate(events):
  if name=='GX_CopyTex':assert events[i+1:i+3]==['GX_PixModeSync','GX_InvalidateTexAll']
 assert call('gs_mem_sync')==1
 for y in range(16):
  for x in range(32):
   assert call('gs_mem_read_psmct32',0,128,x+4,y+3)==expected[x,y],('RGB/alpha',case,x,y,hex(expected[x,y]))
   assert call('gs_mem_read_z',16384,128,x+4,y+3,zpsm)==zexpected[x,y],('Z',case,x,y)
   comparisons+=2
 assert events.count('DCInvalidateRange')==1,(case,events)
print(f'PASS linked PPC resident pipeline: {accepted} GIF sprites, {comparisons} RGB/alpha/Z comparisons, fixed and uniform AS/AD blend, mixed PABE, Z16/24/32, no per-draw color readback; synthetic GX only')
