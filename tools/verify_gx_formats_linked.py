#!/usr/bin/env python3
"""Actual PPC GS decode -> GX upload, DISPLAY pack and CT16 import.
GPU rasterization is not simulated; the exercised conversion/import code is real.
"""
from pathlib import Path
source=Path(__file__).with_name('verify_beta12_22_linked.py').read_text()
exec(compile(source.split('_,vo=')[0],'verify_beta12_22_linked.py','exec'))
_,go=layout('core/hw/gif.h','gif_state_t',['tex_psm','tex_tbp0','tex_tbw','texa_ta0','texa_ta1','texa_aem','tex_cpsm','clut_cache'])
fields=['tw','th','columns','rows','column','row','coverage.bp','coverage.bw','coverage.psm',
        'coverage.width','coverage.height','coverage.right','pipeline.fba','pipeline.fbmask','pipeline.ate','pipeline.atst','pipeline.afail']
size,do=layout('core/hw/gs_gx.h','gs_gx_texture_draw',fields)
gif=call('gif_get_state');dptr=0x817c0000;out=0x817d0000;tex=out+0x1000
formats=(0,1,2,10,0x13,0x14,0x1b,0x24,0x2c,0x30,0x31,0x32,0x3a)
def field(key,value):word(dptr+do[key],value)
def expand(v):return ((v&31)<<3)|(((v>>5)&31)<<11)|(((v>>10)&31)<<19)|((128 if v&0x8000 else 64 if v&0x7fff else 0)<<24)
checks=0
for fmt in formats:
 call('gif_init');call('gs_mem_init');u.mem_write(dptr,bytes(size));field('tw',4);field('th',4)
 for key,value in [('tex_psm',fmt),('tex_tbp0',128),('tex_tbw',128),('texa_ta0',64),('texa_ta1',128),('texa_aem',1),('tex_cpsm',0)]:word(gif+go[key],value)
 for i in range(256):word(gif+go['clut_cache']+i*4,0x80000000+i*0x010307)
 expected=[]
 for y in range(4):
  for x in range(4):
   n=y*4+x;v=(0x11223344+n*0x19372715)&0xffffffff if n else 0
   is16=fmt in (2,10,0x32,0x3a)
   if is16:v&=65535
   if fmt>=0x30:call('gs_mem_write_z',128*64,128,x,y,fmt&15,v)
   elif is16:call('gs_mem_write_psmct16s' if fmt==10 else 'gs_mem_write_psmct16',128*64,128,x,y,v)
   elif fmt<=1:call('gs_mem_write_psmct32',128*64,128,x,y,v)
   else:call('gs_mem_write_index',128*64,128,x,y,fmt,n);v=0x80000000+n*0x010307
   if fmt in (1,0x31):v=(v&0xffffff)|((64 if v&0xffffff else 0)<<24)
   if is16:v=expand(v)
   expected.append(v)
 assert call('gs_gx_pack_texture',out,64,dptr,syms['gs_sample_texel_uncached'])==64
 data=bytes(u.mem_read(out,64))
 for n,v in enumerate(expected):
  t=(n//4)*8+(n%4)*2
  assert (data[t],data[t+1],data[t+32],data[t+33])==(v>>24,v&255,v>>8&255,v>>16&255),(fmt,n)
  checks+=1
print(f'PASS {checks} actual PPC texels: all 13 GS formats uploaded as GX RGBA8')
for fmt in (0,1,2,10):
 call('gs_mem_init')
 for y in range(4):
  for x in range(4):
   call('gs_mem_write_psmct16s' if fmt==10 else 'gs_mem_write_psmct16' if fmt==2 else 'gs_mem_write_psmct32',0,64,x,y,0x80a5b6c7)
 # SysV PPC32 ninth argument uses the first outgoing stack slot.
 word(0x81780008,fmt)
 assert call('gs_gx_pack_rgba8_psm',out,64,0,64,0,0,4,4)==64
 data=bytes(u.mem_read(out,64));v=0xa5b6c7
 if fmt in (2,10):
  r,g,b=0xb6c7&31,(0xb6c7>>5)&31,(0xb6c7>>10)&31
  v=((r<<3)|(r>>2))|(((g<<3)|(g>>2))<<8)|(((b<<3)|(b>>2))<<16)
 assert (data[0],data[1],data[32],data[33])==(255,v&255,v>>8&255,v>>16&255)
 u.mem_write(dptr,bytes(size))
 for key,value in [('tw',4),('th',4),('columns',4),('rows',4),('coverage.bp',0),('coverage.bw',64),('coverage.psm',fmt),('coverage.width',4),('coverage.height',4),('pipeline.fba',1)]:field(key,value)
 u.mem_write(dptr+do['coverage.right'],struct.pack('>H',1))
 u.mem_write(out,bytes(64));u.mem_write(out+1,b'\x11');u.mem_write(out+32,b'\x22\x33');u.mem_write(tex,bytes([0x40])+bytes(63))
 u.mem_write(ram,bytes(4));assert call('gs_gx_import_texture',ram,0x400000,out,64,tex,64,dptr)==1
 value=int.from_bytes(u.mem_read(ram,2 if fmt in (2,10) else 4),'little')
 expected=0x9882 if fmt in (2,10) else 0x00332211 if fmt==1 else 0xc0332211
 assert value==expected,(fmt,hex(value),hex(expected))
print('PASS actual PPC four-format DISPLAY packing and framebuffer import, including CT16/CT16S alpha/FBA')
