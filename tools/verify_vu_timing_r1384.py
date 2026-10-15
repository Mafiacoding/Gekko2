#!/usr/bin/env python3
"""Independent timed-FMAC oracles executed from the actual Wii ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
fields=['vf','vi','acc','mem','micro','pc','bd','bt','ed','retired','unknown','pipeline','timing']
definition='typedef struct {uint32_t vf[32][4],vi[32],acc[4];uint8_t mem[16384],micro[16384];uint32_t pc,bd,bt,ed;uint64_t retired,unknown;vu_pipeline_t pipeline;vu_timing_t timing;} context;'
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/hw/vu.h"\n'+definition+'\nconst unsigned sizes[]={sizeof(context),'+','.join('offsetof(context,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 size,*offsets=struct.unpack('>14I',raw.read_bytes());layout=dict(zip(fields,offsets))
ctx=0x93000000;u.mem_map(ctx,0x10000)
def get_vi(n):return int.from_bytes(u.mem_read(ctx+layout['vi']+n*4,4),'big')
def put_vi(n,v):word(ctx+layout['vi']+n*4,v)
def put_vf(n,l,v):word(ctx+layout['vf']+n*16+l*4,v)
def reset_context():
 setup();u.mem_write(ctx,bytes(size));put_vf(0,3,0x3f800000)
def step(upper,lower=0):
 pc=int.from_bytes(u.mem_read(ctx+layout['pc'],4),'big')
 u.mem_write(ctx+layout['micro']+pc,struct.pack('<II',lower,upper))
 args=[ctx+layout['vf'],ctx+layout['vi'],ctx+layout['acc'],ctx+layout['mem'],16383,ctx+layout['micro'],16383,ctx+layout['pc'],ctx+layout['bd'],ctx+layout['bt'],ctx+layout['ed'],ctx+layout['retired'],ctx+layout['unknown'],ctx+layout['pipeline'],ctx+layout['timing']]
 for i,value in enumerate(args[8:]):word(0x81780008+i*4,value)
 call('vu_micro_step_timed',*args[:8])
NOP=0x2ff
reset_context();put_vf(1,0,0x3f800000);put_vf(2,0,0xbf800000);put_vi(16,0x20);put_vi(17,0xaaaa)
step((8<<21)|(2<<16)|(1<<11)|(3<<6)|40,(22<<25)|(4<<16)|15)
assert get_vi(4)==0 and get_vi(16)==0x20 and get_vi(17)==0xaaaa
for _ in range(3):step(NOP)
assert get_vi(17)==0xaaaa
step(NOP,(22<<25)|(4<<16)|15);assert get_vi(4)==1 and get_vi(17)==8 and get_vi(16)==0x61
reset_context();put_vf(1,0,0x3f800000);put_vf(2,0,0)
div=0x80000000|(2<<16)|(1<<11)|(14<<6)|60
step(NOP,div);step(NOP);step(NOP);step(NOP,21<<25);step(NOP,div|3)
assert get_vi(16)&0xc30==0x820
reset_context();put_vf(1,0,0x3f800000);put_vf(2,0,0xbf800000)
step((8<<21)|(2<<16)|(1<<11)|(3<<6)|40);step(NOP,(21<<25)|0x100)
for _ in range(4):step(NOP)
assert get_vi(17)==8 and get_vi(16)&0xfcf==0x101
print('PASS actual Wii ELF: delayed FMAC/read query, same-clock FSSET/FDIV and issued-MAC snapshots')
# Optional owner's micro image checks an actual compiled hot specialization.
import os
micro_path=os.environ.get('GEKKO2_VU_MICRO')
if micro_path:
 candidates=[]
 for low,high in struct.iter_unpack('<II',Path(micro_path).read_bytes()):
  w=high&0x01ffffff;fn=w&63;fs=w>>11&31;ft=w>>16&31;mask=w>>21&15
  if fn<16 and fs and ft and fs!=ft and mask:candidates.append(w)
 assert candidates,'micro image has no suitable independent arithmetic oracle'
 w=candidates[0];fn=w&63;fs=w>>11&31;ft=w>>16&31;mask=w>>21&15
 reset_context();word(syms['gekko2_optimization_mask'],int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')|4)
 for lane in range(4):put_vf(fs,lane,0x3f800000);put_vf(ft,lane,0xbf800000)
 def count_upper():
  hi=call('vu_aot_executed_count',0);return hi<<32|u.reg_read(UC_PPC_REG_4)
 before=count_upper();step(w);after=count_upper();assert after==before+1
 for _ in range(4):step(NOP)
 status=1 if fn<4 else 2 if 8<=fn<12 else 0
 mac=mask if fn<4 else mask<<4 if 8<=fn<12 else 0
 assert get_vi(17)==mac and get_vi(16)&15==status
 print('PASS actual hot native timed-AOT arithmetic, independent MAC/STATUS oracle')
