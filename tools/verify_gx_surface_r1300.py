"""Linked PPC resident framebuffer sequencing with a synthetic RGB8 EFB.
Checks submitted span geometry/import; does not measure physical Wii GX/FPS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_vram_sync_r1289.py').read_text(),'verify_vram_sync_r1289.py','exec'))
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.ppc_const import UC_PPC_REG_MSR,UC_PPC_REG_FPR1
u.reg_write(UC_PPC_REG_MSR,0x2000)
u.mem_map(0xcc008000,4096);u.mem_map(0x90400000,12*1024*1024)
u.mem_write(syms['texture'],struct.pack('>I',0x90400000))
u.mem_write(syms['efb_width'],struct.pack('>I',128));u.mem_write(syms['efb_height'],struct.pack('>I',64))
services={v:k for k,v in syms.items() if k.startswith('GX_')}
services.update({syms[k]:k for k in ['memalign','free','DCFlushRange','DCInvalidateRange','ps_guMtxIdentity']})
events=[];allocation=[0x90600000];fifo=[];batch=[None];textured=[False];texobj={};texmap=[None];stages=[1];tevcolors={};tevops={};shown=[];efb=bytearray(128*64*3)
def tile(w,x,y):return ((y//4)*(w//4)+x//4)*64+(y%4)*8+(x%4)*2
def finish():
 if batch[0] is None:return
 count,tex,source,program=batch[0];stride=5 if tex else 7
 assert len(fifo)==count*stride,(len(fifo),count,stride)
 vertices=[]
 for i in range(count):
  item=fifo[i*stride:(i+1)*stride]
  xyz=[struct.unpack('>f',v.to_bytes(4,'big'))[0] for _,v in item[:3]]
  if tex:extra=[struct.unpack('>f',v.to_bytes(4,'big'))[0] for _,v in item[3:]]
  else:extra=[v for _,v in item[3:]]
  vertices.append((xyz,extra))
 for i in range(0,count,4):
  q=vertices[i:i+4];x0,y0=map(round,q[0][0][:2]);x1,y1=map(round,q[2][0][:2])
  for y in range(y0,y1):
   for x in range(x0,x1):
    if tex:
     ptr,w,h=source;s=q[0][1][0]+(q[1][1][0]-q[0][1][0])*(x+.5-x0)/(x1-x0)
     t=q[0][1][1]+(q[3][1][1]-q[0][1][1])*(y+.5-y0)/(y1-y0)
     tx=max(0,min(w-1,int(s*w)));ty=max(0,min(h-1,int(t*h)));o=tile(w,tx,ty)
     rgb=[u.mem_read(ptr+o+1,1)[0],*u.mem_read(ptr+o+32,2)]
    else:rgb=q[0][1][:3]
    for inputs,op in program:
     def cv(c):return rgb if c==0 else [16]*3 if c==2 else [240]*3 if c==4 else [0]*3
     aa,bb,cc,dd=map(cv,inputs);assert aa==bb and cc==[0]*3
     rgb=[max(0,min(255,d+a if op==0 else d-a)) for a,d in zip(aa,dd)]
    efb[(y*128+x)*3:(y*128+x)*3+3]=bytes(rgb)
 fifo.clear();batch[0]=None
copy=[(0,0,0,0)]
def service(uc,address,size,user):
 name=services.get(address)
 if not name:return
 args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(8)];events.append(name);ret=0
 if name in ['GX_Begin','GX_CopyTex','GX_CopyDisp','GX_DrawDone']:finish()
 if name=='memalign':ret=allocation[0];allocation[0]+=0x200000
 if name=='ps_guMtxIdentity':uc.mem_write(args[0],struct.pack('>12f',1,0,0,0,0,1,0,0,0,0,1,0))
 if name=='GX_GetYScaleFactor':uc.reg_write(UC_PPC_REG_FPR1,struct.unpack('>Q',struct.pack('>d',1.0))[0])
 if name=='GX_SetDispCopyYScale':ret=64
 if name=='GX_SetNumTevStages':stages[0]=args[0]
 if name=='GX_SetTevColorIn':tevcolors[args[0]]=args[1:5]
 if name=='GX_SetTevColorOp':tevops[args[0]]=args[1]
 if name=='GX_CopyDisp':shown.append(bytes(efb))
 if name=='GX_SetNumTexGens':textured[0]=bool(args[0])
 if name=='GX_InitTexObj':texobj[args[0]]=tuple(args[1:4])
 if name=='GX_LoadTexObj':texmap[0]=texobj[args[0]]
 if name=='GX_Begin':batch[0]=(args[2],textured[0],texmap[0],[(tevcolors[i],tevops[i]) for i in range(1,stages[0])])
 if name=='GX_SetTexCopySrc':copy[0]=tuple(args[:4])
 if name=='GX_CopyTex':
  sx,sy,w,h=copy[0];pixels=bytearray(w*h*4)
  for y in range(h):
   for x in range(w):
    o=tile(w,x,y);p=((y+sy)*128+x+sx)*3
    pixels[o:o+2]=bytes([255,efb[p]]);pixels[o+32:o+34]=efb[p+1:p+3]
  uc.mem_write(args[0],bytes(pixels))
 uc.reg_write(UC_PPC_REG_3,ret);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
def capture(uc,access,address,size,value,user):fifo.append((size,value))
h=u.hook_add(UC_HOOK_CODE,service);f=u.hook_add(UC_HOOK_MEM_WRITE,capture,begin=0xcc008000,end=0xcc008003)
def invoke(name,*args):
 u.reg_write(UC_PPC_REG_1,0x81780000)
 for i,v in enumerate(args[:8]):u.reg_write(UC_PPC_REG_3+i,v&0xffffffff)
 for i,v in enumerate(args[8:]):u.mem_write(0x81780008+i*4,struct.pack('>I',v&0xffffffff))
 u.reg_write(UC_PPC_REG_LR,0x817ff000);u.emu_start(syms[name],0x817ff000,count=10000000)
 assert u.reg_read(UC_PPC_REG_PC)==0x817ff000,name
 return u.reg_read(UC_PPC_REG_3)
call('gs_mem_init');call('gs_gx_set_render_enabled',1)
xy=0x90002000;u.mem_write(xy,bytes(24))
# Distinct alpha in initial untouched RAM, plus a nonalias source page.
call('gs_mem_write_psmct32',0,128,40,40,0xab112233)
call('gs_mem_write_psmct32',200000,64,0,0,0x99887766)
expected={};events.clear()
for n in range(80):
 psm=n%2;x=n%23;y=n%19;rgba=((n*3&255)<<24)|((n*7&255)<<16)|((n*11&255)<<8)|(n*17&255)
 assert invoke('gs_gx_draw_flat_psm',psm,6,0,128,x,y,x+12,y+8,xy,rgba,0)==1
 for yy in range(y,y+8):
  for xx in range(x,x+12):
   prior=expected.get((xx,yy),0);expected[xx,yy]=(rgba if psm==0 else (prior&0xff000000)|(rgba&0xffffff))
 assert call('gs_mem_gpu_pending')==1
 assert call('gs_mem_read_psmct32',200000,64,0,0)==0x99887766
assert events.count('GX_CopyTex')==0 and events.count('GX_DrawDone')==0,events
assert call('gs_mem_read_psmct32',0,128,2,2)==expected[(2,2)]
assert events.count('GX_CopyTex')==1 and events.count('GX_DrawDone')==1
for (x,y),value in expected.items():assert call('gs_mem_read_psmct32',0,128,x,y)==value,(x,y)
assert call('gs_mem_read_psmct32',0,128,40,40)==0xab112233
assert call('gs_mem_gpu_pending')==0
# CPU write closes a new dirty surface before modifying the same pixel.
assert invoke('gs_gx_draw_flat_psm',0,6,0,128,0,0,4,4,xy,0x7f123456,0)==1
call('gs_mem_write_psmct32',0,128,0,0,0xdeadbeef)
assert call('gs_mem_read_psmct32',0,128,0,0)==0xdeadbeef
assert call('gs_mem_read_psmct32',0,128,1,1)==0x7f123456
print('PASS linked PPC: 80 resident draws, no per-draw GPU wait/copy, nonalias reads stay resident, one overlap resolve, CT24 alpha preservation, CPU-write barrier; synthetic EFB only')
# Textured resident submission: flat-pipeline snapshots share the same cache
# and exact mapped triangle coverage as decoded sprite snapshots.
pipe=0x90003000;u.mem_write(pipe,struct.pack('>14I',0,0,0,0,1,0,0,0,0,0,0,0,0,1))
u.mem_write(xy,struct.pack('>6i',4,4,20,4,4,20))
def counter(name,n):
 call(name,n);return (u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_3+1)
a0=counter('gs_gx_source_cache_count',0);a1=counter('gs_gx_source_cache_count',1)
for n in range(12):
 call('gs_gx_source_key',123,456,1)
 assert invoke('gs_gx_draw_flat_pipeline',0,3,0,128,4,4,20,20,xy,0x81335577,0,pipe)==1
assert counter('gs_gx_source_cache_count',0)==a0+11
assert counter('gs_gx_source_cache_count',1)==a1+1
assert call('gs_mem_read_psmct32',0,128,4,4)==0x81335577
assert call('gs_mem_read_psmct32',0,128,10,10)==0x81335577
# A state/source key change must decode again and update alpha/RGB.
call('gs_gx_source_key',124,456,1)
assert invoke('gs_gx_draw_flat_pipeline',0,3,0,128,4,4,20,20,xy,0x916688aa,0,pipe)==1
assert counter('gs_gx_source_cache_count',1)==a1+2
assert call('gs_mem_read_psmct32',0,128,4,4)==0x916688aa
print('PASS resident textured triangle submission, exact CT32 alpha, 11 source-snapshot cache hits and key-change invalidation; synthetic nearest GPU lookup')

# Matching scanout uses GPU-only backup/clamp/copy/restore; VRAM owner remains.
mode=0x90004000;blob=bytearray(64);struct.pack_into('>I7H',blob,0,0,128,64,64,0,0,128,64);u.mem_write(mode,bytes(blob))
assert invoke('gs_gx_draw_flat_psm',0,6,0,128,0,0,8,8,xy,0x8aff0005,0)==1
finish();raw=bytes(efb);resolves=counter('gs_gx_surface_count',2)
assert invoke('gs_gx_present',0x90005000,mode,0,128,0,0,128,64)==1
assert call('gs_mem_gpu_pending')==1 and counter('gs_gx_surface_count',2)==resolves
assert shown[-1]==bytes(max(16,min(240,c)) for c in raw)
finish();assert bytes(efb)==raw # raw RGB restored, never the broadcast-clamped image
assert call('gs_mem_read_psmct32',0,128,0,0)==0x8aff0005
print('PASS linked resident presentation: GPU-only broadcast clamp, owner retained, exact raw EFB restore before later CPU read; synthetic 1:1 scanout')
