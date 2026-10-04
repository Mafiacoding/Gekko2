"""Actual Wii ELF contiguous GS span oracle and display equivalence."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
import random,json
from unicorn import UC_HOOK_CODE
rng=random.Random(1282);vram=rng.randbytes(4*1024*1024);u.mem_write(syms['g_gs_mem'],vram)
out=0x81700000;cases=0
for bw in [0,64,128,640,1024]:
 for y in [0,1,7,8,15,16,31,32,63,255]:
  for x in [0,1,7,8,63,64,127]:
   bp=[0,64,0x1400,1048572][cases%4];count=[1,2,8,64,79][cases%5]
   u.mem_write(out-4,b'ABCD');u.mem_write(out+count*4,b'WXYZ')
   call('gs_mem_read_psmct32_span',out,bp,bw,x,y,count)
   got=struct.unpack('>'+str(count)+'I',bytes(u.mem_read(out,count*4)))
   def reference(px):
    bx=(px&63)>>3;by=(y&31)>>3
    block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2)
    word=(px&1)|((y&1)<<1)|((px&6)<<1)|((y&6)<<3)
    off=(bp*4+((y//32)*max(1,bw//64)+px//64)*8192+block*256+word*4)&0xffffffff
    return int.from_bytes(vram[off:off+4],'little') if off<=len(vram)-4 else 0
   assert list(got)==[reference(x+n) for n in range(count)],(bp,bw,x,y)
   assert bytes(u.mem_read(out-4,4))==b'ABCD' and bytes(u.mem_read(out+count*4,4))==b'WXYZ'
   cases+=1
# Count actual PPC work for 640 native-width pixels: same bytes via scalar API.
active=[False];total=[0]
def counter(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,counter);u.ctl_remove_cache(0x80000000,0x81800000)
total[0]=0;active[0]=True
scalar=[call('gs_mem_read_psmct32',0x1400,640,x,100) for x in range(640)]
active[0]=False;old=total[0];total[0]=0;active[0]=True
call('gs_mem_read_psmct32_span',out,0x1400,640,0,100,640)
active[0]=False
assert struct.unpack('>640I',bytes(u.mem_read(out,2560)))==tuple(scalar)
print('PASS',cases,'actual Wii ELF GS span cases: pages/columns, zero BW, odd X, final-VRAM boundaries and output guards')
print('GS_SPAN_MEASURE',json.dumps({'elf':Path(a.elf).name,'scalar_PPC':old,'span_PPC':total[0],'pixels':640,'scope':'Actual compiled PPC instructions for equal contiguous GS reads, not Wii cycles/FPS; scanout CPU work only.'}))

# Native-width presentation: byte oracle for conversion and vertical filtering.
def trunc(v,d):return v//d if v>=0 else -((-v)//d)
def color(a,b):
 r1,g1,b1=[max(16,min(240,(a>>k)&255)) for k in [0,8,16]]
 r2,g2,b2=[max(16,min(240,(b>>k)&255)) for k in [0,8,16]]
 y1=(77*r1+150*g1+29*b1)//256;y2=(77*r2+150*g2+29*b2)//256
 cb=(trunc(112*(b1+b2)-74*(g1+g2)-38*(r1+r2),512)+128)&255
 cr=(trunc(112*(r1+r2)-94*(g1+g2)-18*(b1+b2),512)+128)&255
 return (y1<<24)|(cb<<16)|(y2<<8)|cr
for dw,sh,dh in [(8,3,7),(64,4,4),(16,7,3)]:
 rows=[]
 for y in range(sh):
  row=[]
  for x in range(dw):
   value=rng.getrandbits(32);call('gs_mem_write_psmct32',64,64,x+7,y+31,value);row.append(value)
  rows.append([color(row[x],row[x+1]) for x in range(0,dw,2)])
 step=(sh<<16)//dh;fy=(step-65536)//2;expected=[]
 for y in range(dh):
  row=0 if fy<0 else fy>>16;fraction=0 if fy<0 else fy&65535
  if row>=sh-1:row=sh-1;fraction=0
  for x in range(dw//2):
   value=rows[row][x]
   if fraction:
    value=0
    for c in [0,8,16,24]:
     lane=(((rows[row][x]>>c)&255)*(65536-fraction)+((rows[row+1][x]>>c)&255)*fraction+32768)>>16
     value|=lane<<c
   expected.append(value)
  fy+=step
 u.mem_write(0x81780008,struct.pack('>I',sh))
 call('gs_blit_scaled_psmct32_to_xfb',out,dw,dh,64,64,7,31,dw)
 assert bytes(u.mem_read(out,len(expected)*4))==struct.pack('>'+str(len(expected))+'I',*expected),(dw,sh,dh)
print('PASS actual Wii ELF native-width scanout conversion/filter byte oracle: columns/pages, upsample, identity and downsample')
