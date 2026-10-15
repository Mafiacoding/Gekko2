"""Replay private, real-BIOS GS packets on the linked PPC GX backend.
Run with GEKKO2_BIOS_GX_CASES pointing at captures; GX commands are modeled.
Never put captured guest packets or VRAM in a public source archive.
"""
import os,math
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_native_affine_r1389.py').read_text(),'native_fixture','exec'))
# Larger real BIOS fog triangles require a larger linked-PPC execution budget.
loader_src=Path(__file__).with_name('verify_ppc_gs_memory.py').read_text()
call_src=loader_src[loader_src.index('def call('):loader_src.index('checks=0')]
exec(call_src.replace('count=10000000','count=200000000').replace('range(32)','range(4096)'))
case_dir=Path(os.environ['GEKKO2_BIOS_GX_CASES']);pack=0x817a0000;source_pixels=0x90f00000
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'replay.c';exe=Path(td)/'replay.elf';raw=Path(td)/'replay.bin'
 src.write_text('''#include "core/hw/gs_gx.h"
typedef struct {unsigned h[16];int xy[6];float uv[6],q[3];unsigned rgba[3];gs_gx_pipeline p;gs_gx_native_color c;} packet;
static unsigned sample(int x,int y){(*(volatile unsigned*)0x817b0000)++;return ((const unsigned*)0x90f00000)[y*(*(volatile unsigned*)0x90effff0)+x];}
int replay(packet *a){return gs_gx_draw_native_stq_triangle(a->h[0],a->h[1],a->h[2],a->h[3],a->h[4],a->h[5],a->h[6],a->xy,a->uv,a->h[15]?a->q:0,a->h[14]?a->rgba:0,a->h[7],a->h[8],a->h[9],a->h[10],a->h[11],a->h[12],sample,&a->p,a->h[13],&a->c);}
''')
 subprocess.run([cc,'-O2','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817d0000','-Wl,-e,replay','-Wl,--defsym,gs_gx_draw_native_stq_triangle='+hex(syms['gs_gx_draw_native_stq_triangle']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text',str(exe),str(raw)],check=True)
 u.mem_write(0x817d0000,raw.read_bytes())
 for row in subprocess.check_output([a.nm,str(exe)],text=True).splitlines():
  if row.endswith(' replay'):syms['replay_bios']=int(row.split()[0],16)
returns={};trace=[]
tracked={syms[n]:n for n in ['gs_gx_prepare_flat','gs_gx_surface_depth_range','gs_gx_surface_depth_clip','surface_ensure_height'] if n in syms}
def trace_helpers(uc,address,size,user):
 if address in returns:trace.append((returns[address],uc.reg_read(UC_PPC_REG_3)))
 if address in tracked:returns[uc.reg_read(UC_PPC_REG_LR)]=tracked[address]
u.hook_add(UC_HOOK_CODE,trace_helpers)
results=[]
for path in sorted(case_dir.glob('case-*.bin')):
 data=path.read_bytes();h=struct.unpack('<16I',data[:64]);tw,th=h[7:9]
 assert len(data)==208+tw*th*4+4*1024*1024
 meta=struct.unpack('<52I',data[:208]);pixels=struct.unpack('<'+str(tw*th)+'I',data[208:208+tw*th*4]);vram=data[208+tw*th*4:]
 # Drop modeled pending commands and create a fresh surface for each packet.
 word(syms['g_gpu_pending'],0);call('gs_mem_init');u.mem_write(syms['g_gs_mem'],vram)
 for name,value in [('initialized',1),('render_enabled',1),('surface_enabled',1),('efb_width',640),('efb_height',512),('surface',0),('surface_pixels',0),('texture',0x91000000),('texture_in_flight',0)]:word(syms[name],value)
 u.mem_write(syms['surface_seeded'],b'\0'*(640*512//16))
 u.mem_write(pack,struct.pack('>52I',*meta));u.mem_write(source_pixels,struct.pack('>'+str(tw*th)+'I',*pixels));word(source_pixels-16,tw);word(counter,0)
 saved=len(begins);trace.clear();returns.clear()
 # The old 512-row extent falsely aliases these BIOS color/depth buffers.
 original_height=meta[-1];word(pack+204,0)
 unbounded=call('replay_bios',pack)
 assert not unbounded and len(begins)==saved
 word(pack+204,original_height);word(counter,0)
 accepted=call('replay_bios',pack);decoded=int.from_bytes(u.mem_read(counter,4),'big');checks=0
 if accepted:
  assert any(b[0]==0x90 and b[2]==3 for b in begins[saved:])
  ptr=int.from_bytes(u.mem_read(syms['surface'],4),'big');dirty=u.mem_read(ptr+24,640*512);alpha=u.mem_read(ptr+24+640*512,640*512)
  xy=struct.unpack('<6i',data[64:88]);uv=struct.unpack('<6f',data[88:112]);q=struct.unpack('<3f',data[112:124]);col=struct.unpack('<4I',data[192:208]);bw=h[2]
  def edge(ax,ay,bx,by,x,y):return (bx-ax)*(y-ay)-(by-ay)*(x-ax)
  area=edge(*xy[:4],*xy[4:]);inv=1.0/area
  def wrap(x,n,c):return min(n-1,max(0,x)) if c else x&(n-1)
  def a(x,y):return pixels[wrap(y,th,h[10])*tw+wrap(x,tw,h[9])]>>24
  for y in range(h[4],h[6]+1):
   for x in range(h[3],h[5]+1):
    if not dirty[y*bw+x]:continue
    b0=edge(*xy[2:6],x,y)*inv;b1=edge(*xy[4:6],*xy[:2],x,y)*inv;b2=edge(*xy[:4],x,y)*inv
    qi=b0*q[0]+b1*q[1]+b2*q[2];iq=1.0/qi
    tx=(b0*uv[0]+b1*uv[2]+b2*uv[4])*iq*tw;ty=(b0*uv[1]+b1*uv[3]+b2*uv[5])*iq*th
    if h[11]:tx-=.5;ty-=.5
    ix,iy=math.floor(tx),math.floor(ty);want=a(ix,iy)
    if h[11]:
     fx,fy=int((tx-ix)*16),int((ty-iy)*16)
     top=(a(ix,iy)*(16-fx)+a(ix+1,iy)*fx)>>4;bot=(a(ix,iy+1)*(16-fx)+a(ix+1,iy+1)*fx)>>4
     want=(top*(16-fy)+bot*fy)>>4
    va=col[0]>>24
    if not col[1]:want=va
    elif col[2]==0:want=want*va//128
    elif col[2]==2:want+=va
    want=min(255,want);assert alpha[y*bw+x]==want,(path.name,x,y,alpha[y*bw+x],want);checks+=1
 layout=struct.unpack('>8I',u.mem_read(syms['texture_draw'],32))
 results.append({'coverage':list(layout[2:6]),'helpers':list(trace),'case':path.name,'scissor_target_height':original_height,'unbounded_extent_rejected':True,'accepted':bool(accepted),'CPU_source_texels':decoded,'GX_batches':len(begins)-saved,'exact_alpha_checks':checks})
 print(json.dumps(results[-1]),flush=True)
print(json.dumps({'real_BIOS_GX_replay':results,'physical_GPU_pixels_verified':False,'Wii_FPS_measured':False}))
assert sum(x['accepted'] for x in results)>=3,results
