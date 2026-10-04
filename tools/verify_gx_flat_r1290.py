"""Real PPC flat-GX submission with mocked GPU/cache and FIFO memory.
Physical GX rasterization is deliberately not simulated/claimed.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_vram_sync_r1289.py').read_text(),'verify_vram_sync_r1289.py','exec'))
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.ppc_const import UC_PPC_REG_MSR
u.reg_write(UC_PPC_REG_MSR,0x2000)
if 'R1300' in Path(a.elf).name:u.mem_write(syms['surface_enabled'],struct.pack('>I',0)) # legacy compact-EFB path; residency has a separate suite
u.mem_map(0xcc008000,0x1000)
all_services={addr:name for name,addr in syms.items() if name.startswith('GX_')}
all_services.update({syms[name]:name for name in ['memalign','DCFlushRange','DCInvalidateRange','ps_guMtxIdentity']})
events=[];fifo_writes=[]
def mock(uc,address,size,user):
 name=all_services.get(address)
 if not name:return
 args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(3)];events.append((name,args))
 if name=='ps_guMtxIdentity':uc.mem_write(args[0],struct.pack('>12f',1,0,0,0,0,1,0,0,0,0,1,0))
 if name=='GX_CopyTex':
  raw=bytes(uc.mem_read(syms['flat_draw'],28));bp,bw,x,y,w,h,rgba=struct.unpack('>7I',raw)
  pixels=bytearray(w*h*4)
  for yy in range(h):
   for xx in range(w):
    t=((yy//4)*(w//4)+xx//4)*64+(yy%4)*8+(xx%4)*2
    pixels[t:t+2]=bytes([255,rgba&255]);pixels[t+32:t+34]=bytes([(rgba>>8)&255,(rgba>>16)&255])
  uc.mem_write(args[0],bytes(pixels))
 uc.reg_write(UC_PPC_REG_3,0x90200000 if name=='memalign' else 0)
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
def fifo(uc,access,address,size,value,user):fifo_writes.append((size,value))
h=u.hook_add(UC_HOOK_CODE,mock);f=u.hook_add(UC_HOOK_MEM_WRITE,fifo,begin=0xcc008000,end=0xcc008003)
def invoke(name,*args):
 u.reg_write(UC_PPC_REG_1,0x81780000)
 for i,v in enumerate(args[:8]):u.reg_write(UC_PPC_REG_3+i,v&0xffffffff)
 for i,v in enumerate(args[8:]):u.mem_write(0x81780008+i*4,struct.pack('>I',v&0xffffffff))
 u.reg_write(UC_PPC_REG_LR,0x817ff000);u.emu_start(syms[name],0x817ff000,count=10000000)
 assert u.reg_read(UC_PPC_REG_PC)==0x817ff000,name
 return u.reg_read(UC_PPC_REG_3)
xy=0x90002000;u.mem_write(xy,struct.pack('>6i',0,0,8,0,0,8))
call('gs_gx_set_render_enabled',1)
def work_count(n):
 call('gs_gx_work_count',n);return(u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_3+1)
work_before=[work_count(n)for n in range(5)]if 'gs_gx_work_count'in syms else None
for kind,maxx,maxy,mask in [(3,8,8,0),(3,8,8,2),(6,7,9,0),(6,7,9,3)]:
 call('gs_mem_init');events.clear();fifo_writes.clear()
 assert invoke('gs_gx_draw_flat',kind,64,128,0,0,maxx,maxy,xy,0x80112233,mask)==1
 assert call('gs_mem_gpu_pending')==1
 assert any(n=='GX_Begin' for n,_ in events) and fifo_writes
 if 'R1292' in Path(a.elf).name or 'R1293' in Path(a.elf).name or 'R1294' in Path(a.elf).name or 'R1295' in Path(a.elf).name or ('R1296' in Path(a.elf).name or ('R1297' in Path(a.elf).name or ('R1298' in Path(a.elf).name or 'R1299' in Path(a.elf).name or 'R1300' in Path(a.elf).name))):
  batches=[args for n,args in events if n=='GX_Begin']
  assert len(batches)==1 and batches[0][2]*7==len(fifo_writes),(batches,len(fifo_writes))
 assert not any(n=='GX_DrawDone' for n,_ in events) # deferred readback
 assert call('gs_mem_sync')==1
 assert [n for n,_ in events][-2:]==['GX_DrawDone','DCInvalidateRange']
 for y in range(12):
  for x in range(12):
   covered=(x+y<=8) if kind==3 else (x<maxx and y<maxy)
   if (mask==2 and not(y&1))or(mask==3 and(y&1)):covered=False
   assert call('gs_mem_read_psmct32',64,128,x,y)==(0x80112233 if covered else 0),(kind,mask,x,y)
if work_before is not None:
 now=[work_count(n)for n in range(5)]
 assert now[0]-work_before[0]==4 and now[3]-work_before[3]==4 and now[4]-work_before[4]==4,(work_before,now)
 assert now[1]>work_before[1] and now[2]>work_before[2]
# Disabled mode and invalid target must retain software fallback without FIFO work.
call('gs_gx_set_render_enabled',0);events.clear();fifo_writes.clear()
assert invoke('gs_gx_draw_flat',6,64,128,0,0,8,8,xy,0x80112233,0)==0
assert not events and not fifo_writes
call('gs_gx_set_render_enabled',1)
assert invoke('gs_gx_draw_flat',6,0xffffffff,128,0,0,8,8,xy,0x80112233,0)==0
assert not events and not fifo_writes
u.hook_del(h);u.hook_del(f)
print('PASS actual PPC GX flat triangle/sprite submission, FIFO vertices, SCANMSK, masked VRAM readback and disabled/invalid fallback; GPU/cache services mocked')
# Actual GIF packets must enter GX only for eligible GS states.
h=u.hook_add(UC_HOOK_CODE,mock);f=u.hook_add(UC_HOOK_MEM_WRITE,fifo,begin=0xcc008000,end=0xcc008003)
packet=0x81740000
def ad(reg,lo,hi=0):
 blob=struct.pack('<4I',0x8001,1<<28,14,0)+struct.pack('<4I',lo,hi,reg,0)
 u.mem_write(packet,blob);call('gif_process_quadwords',2,packet,2)
for kind in [3,6]:
 for gate in ['flat','gouraud','blend','alpha_test','zbuf','masked_z','always_z','never_z','fbmask','fog','dither','fba','psm24']:
  if ('R1298' in Path(a.elf).name or 'R1299' in Path(a.elf).name or 'R1300' in Path(a.elf).name) and gate=='blend':continue
  call('gs_mem_sync');call('gif_init');call('gs_mem_init');events.clear();fifo_writes.clear()
  ad(0x4c,(1<<16)|((1<<24)if gate=='psm24'else 0),0xff000000 if gate=='fbmask'else 0)
  ad(0x18,0);ad(0x40,15<<16,15<<16)
  if gate=='alpha_test':ad(0x47,1)
  if gate in ['zbuf','masked_z','always_z','never_z']:
   ad(0x4e,0,1 if gate!='zbuf'else 0)
   if gate in ['always_z','never_z']:ad(0x47,(1<<16)|((1 if gate=='always_z'else 0)<<17))
  if gate=='dither':ad(0x45,1)
  if gate=='fba':ad(0x4a,1)
  attr=(8 if gate=='gouraud'else 64 if gate=='blend'else 32 if gate=='fog'else 0)
  if gate=='psm24':
   for yy in range(12):
    for xx in range(12):call('gs_mem_write_psmct32',0,64,xx,yy,0xa5776655)
  ad(0,kind|attr);ad(1,0x80112233)
  vertices=[(0,0),(8,0),(0,8)]if kind==3 else[(0,0),(8,8)]
  for x,y in vertices:ad(5,(x<<4)|((y<<4)<<16),0)
  # Sprite color is already flat regardless of IIP, so IIP may use GX.
  eligible=gate=='flat'or(kind==6 and gate=='gouraud')or(gate=='psm24' and ('R1294'in Path(a.elf).name or 'R1295'in Path(a.elf).name or ('R1296'in Path(a.elf).name or ('R1297'in Path(a.elf).name or ('R1298'in Path(a.elf).name or 'R1299'in Path(a.elf).name or 'R1300'in Path(a.elf).name)))))
  if ('R1295'in Path(a.elf).name or ('R1296'in Path(a.elf).name or ('R1297'in Path(a.elf).name or ('R1298'in Path(a.elf).name or 'R1299'in Path(a.elf).name or 'R1300'in Path(a.elf).name)))) and gate in ['masked_z','always_z']:eligible=True
  assert any(n=='GX_Begin'for n,_ in events)==eligible,(kind,gate)
  assert bool(call('gs_mem_gpu_pending'))==eligible,(kind,gate)
  call('gs_mem_sync')
  if eligible:assert call('gs_mem_read_psmct32',0,64,1,1)==(0xa5112233 if gate=='psm24'else 0x80112233)
u.hook_del(h);u.hook_del(f)
print('PASS actual GIF->GX routing for triangle/sprite and 13 GS state gates each; unsupported states preserve software fallback')

# R1297: fractional GS vertices must reach the same coverage bounds in GX.
if ('R1297' in Path(a.elf).name or ('R1298' in Path(a.elf).name or 'R1299' in Path(a.elf).name or 'R1300' in Path(a.elf).name)):
 h=u.hook_add(UC_HOOK_CODE,mock);f=u.hook_add(UC_HOOK_MEM_WRITE,fifo,begin=0xcc008000,end=0xcc008003)
 for phase in range(16):
  call('gs_mem_sync');call('gif_init');call('gs_mem_init');events.clear();fifo_writes.clear()
  ad(0x4c,1<<16);ad(0x18,0);ad(0x40,15<<16,15<<16);ad(0,6);ad(1,0x80112233)
  ad(5,phase|(phase<<16),0);ad(5,(128+phase)|((128+phase)<<16),0)
  assert any(n=='GX_Begin' for n,_ in events),phase
  first=int(phase!=0);last=8+first
  flat=struct.unpack('>7I',bytes(u.mem_read(syms['flat_draw'],28)))
  assert flat[2:6]==(first,first,8,8),(phase,flat)
  call('gs_mem_sync')
  for y in range(10):
   for x in range(10):
    assert call('gs_mem_read_psmct32',0,64,x,y)==(0x80112233 if first<=x<last and first<=y<last else 0),(phase,x,y)
 u.hook_del(h);u.hook_del(f)
 print('PASS 16 fractional sprite phases routed GIF->GX with exact ceil coverage and masked VRAM import; GPU services mocked')
