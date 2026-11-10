#!/usr/bin/env python3
"""Delivered PPC VIF barriers and UV decode/rendering; synthetic streams only."""
from pathlib import Path
source=Path(__file__).with_name('verify_beta12_22_linked.py').read_text()
exec(compile(source.split('_,vo=')[0],'verify_beta12_22_linked.py','exec'))
_,vo=layout('core/hw/vu.h','vu1_state_t',['running','tpc','micro'])
_,fo=layout('core/hw/vif.h','vif_state_t',['mark','stall_words','codes_processed'])
_,go=layout('core/hw/gif.h','gif_state_t',['cur_u','cur_v'])
vu=call('vu1_get_state');vif=call('vif1_get_state');gif=call('gif_get_state');inp=ram+0x1000
for cmd in (0x10,0x11,0x13,0x14,0x15,0x17,0x4a):
 call('vif_init');call('vu1_init');call('gif_init')
 u.mem_write(vu+vo['micro'],struct.pack('<4I',0x800003bf,0x400002ff,0x800003bf,0x2ff))
 # running is a byte, with tpc derived from the target ABI.
 u.mem_write(vu+vo['running'],b'\1');word(vu+vo['tpc'],0)
 data=[cmd<<24|(1<<16 if cmd==0x4a else 0)]
 if cmd==0x4a:data.extend([0x800003bf,0x400002ff])
 data.append(0x07001234);data.extend([0]*(8-len(data)));u.mem_write(inp,struct.pack('<8I',*data))
 assert call('vif_accept_quadwords',1,inp,2)==2
 assert int.from_bytes(u.mem_read(vif+fo['mark'],4),'big')==0
 assert int.from_bytes(u.mem_read(vif+fo['codes_processed'],8),'big')==0
 for n in range(4):call('vif_service')
 assert int.from_bytes(u.mem_read(vif+fo['mark'],4),'big')==0
 call('vu1_tick');call('vu1_tick');call('vif_service')
 assert int.from_bytes(u.mem_read(vif+fo['mark'],4),'big')==0x1234
 assert int.from_bytes(u.mem_read(vif+fo['stall_words'],4),'big')==0
print('PASS 7 linked PPC VIF barriers: held while VU runs, exactly-once replay after E/delay')
def ad(reg,lo,hi=0):
 u.mem_write(inp,struct.pack('<8I',0x8001,1<<28,14,0,lo,hi,reg,0))
 call('gif_process_quadwords',1,inp,2)
for fraction in range(16):
 call('gif_init');call('gs_mem_init');ad(0x4c,1<<16);ad(6,128|(1<<14)|(2<<26)|(2<<30),12)
 ad(0,3|16|256);ad(0x14,(1<<5)|(1<<6));ad(1,0x80808080)
 for y in range(4):
  for x in range(4):call('gs_mem_write_psmct32',128*64,64,x,y,0x80000000|x*64)
 ad(3,(16+fraction)|(8<<16))
 assert struct.unpack('>d',u.mem_read(gif+go['cur_u'],8))[0]==1+fraction/16
 for x,y in ((0,0),(64,0),(0,64)):ad(5,x|(y<<16))
 f=fraction+8 if fraction<8 else fraction-8;left=0 if fraction<8 else 64;right=left+64
 expected=0x80000000|((left*(16-f)+right*f)>>4)
 assert call('gs_mem_read_psmct32',0,64,1,1)==expected,(fraction,hex(expected))
print('PASS 16 linked PPC fractional UV decode, vertex assembly and bilinear triangle pixels')
for wide in (0,1):
 width=640;pairs=320;active_pairs=240;left=40;pixels=[0x23450000+n for n in range(pairs*3)]
 u.mem_write(inp,struct.pack('>'+str(len(pixels))+'I',*pixels))
 assert call('gs_xfb_fit_4_3',inp,width,3,wide)==1
 data=struct.unpack('>'+str(len(pixels))+'I',u.mem_read(inp,len(pixels)*4))
 for y in range(3):
  for x in range(pairs):
   expected=pixels[y*pairs+x]
   if wide:expected=0x10801080 if x<left or x>=left+active_pairs else pixels[y*pairs+((2*(x-left)+1)*pairs)//(2*active_pairs)]
   assert data[y*pairs+x]==expected
print('PASS linked PPC XFB 4:3 identity and 16:9 pillarbox, 1920 YCbCr pairs')
