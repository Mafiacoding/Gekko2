#!/usr/bin/env python3
"""Synthetic GS/XGKICK/IPU/input checks executing the delivered PPC ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
def layout(header,typ,fields):
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'s.c').write_text('#include <stddef.h>\n#include "'+header+'"\nconst unsigned layout[]={sizeof('+typ+'),'+','.join('offsetof('+typ+','+f+')' for f in fields)+'};')
  subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(p/'s.c'),'-o',str(p/'s.o')],check=True)
  subprocess.run([oc,'-O','binary','-j','.rodata',str(p/'s.o'),str(p/'s.bin')],check=True)
  n,*values=struct.unpack('>'+str(1+len(fields))+'I',(p/'s.bin').read_bytes());return n,dict(zip(fields,values))
_,vo=layout('core/hw/vu.h','vu1_state_t',['vi','mem','micro','xgkick_pending','instructions_executed'])
_,go=layout('core/hw/gif.h','gif_state_t',['gif_ctrl','ctx1_xyoffset_x','clut_cache','texclut_cbw','texclut_cov','tex_tbp0','tex_tbw','tex_psm','texa_ta0','texa_ta1','texa_aem'])
ipu_size,io=layout('core/hw/ipu.h','ipu_state_t',['mpeg.dc'])
padsize,_=layout('core/hw/controller_input.h','controller_pad',['menu'])
vu=call('vu1_get_state');gif=call('gif_get_state')
def little(address,value):u.mem_write(address,struct.pack('<I',value))
for start in (0,16368):
 setup();call('vu1_init');call('gif_init');call('gs_mem_init')
 mem=vu+vo['mem'];micro=vu+vo['micro']
 u.mem_write(mem+start,struct.pack('<4I',0x8001,1<<28,14,0))
 for delta,val in ((16,0x1111),(32,0x2222)):
  u.mem_write(mem+((start+delta)&16383),struct.pack('<4I',val,0,0x18,0))
 word(vu+vo['vi']+4,start//16);word(gif+go['gif_ctrl'],8)
 u.mem_write(micro,struct.pack('<4I',0x80000efc,0x400002ff,0x800003bf,0x2ff))
 call('vu1_exec_micro',0)
 assert int.from_bytes(u.mem_read(vu+vo['xgkick_pending'],1),'big')
 little(mem+start,0x8002);word(gif+go['gif_ctrl'],0);call('vu1_tick')
 assert not int.from_bytes(u.mem_read(vu+vo['xgkick_pending'],1),'big')
 assert int.from_bytes(u.mem_read(gif+go['ctx1_xyoffset_x'],4),'big')==0x2222
print('PASS linked PPC XGKICK rejected live tag and physical ring wrap')
for cpsm in (0,2,10):
 call('gif_init');call('gs_mem_init');word(gif+go['texclut_cbw'],1)
 for y,value in ((0,0x800000ff if not cpsm else 0x801f),(1,0x8000ff00 if not cpsm else 0x83e0)):
  for x in range(16):call('gs_mem_write_psmct32' if not cpsm else 'gs_mem_write_psmct16s' if cpsm==10 else 'gs_mem_write_psmct16',0,64,x,y,value)
  word(gif+go['texclut_cov'],y);call('gs_load_clut',0x14,0,cpsm,1,2+y,1)
 for bank,value in ((2,0x800000ff if not cpsm else 0x801f),(3,0x8000ff00 if not cpsm else 0x83e0)):
  assert struct.unpack('>16I',u.mem_read(gif+go['clut_cache']+bank*64,64))==(value,)*16
print('PASS linked PPC CSM2 independent CT32/CT16/CT16S palette banks')
for fmt in (0x30,0x31,0x32,0x3a):
 for n,value in enumerate((0,0x8000,0x7fff,0xffffffff,0x12345678,0x80ff00ff,0xffffff,0x80000000)):
  view=fmt&15;x=n*17;y=n*9
  value&=0xffffff if view==1 else 65535 if view in (2,10) else 0xffffffff
  for key,v in [('tex_tbp0',16383),('tex_tbw',128),('tex_psm',fmt),('texa_ta0',0x40),('texa_ta1',0x80),('texa_aem',1)]:word(gif+go[key],v)
  call('gs_mem_write_z',16383*64,128,x,y,view,value)
  expected=value
  if view==1:expected=value|((0x40 if value else 0)<<24)
  if view in (2,10):
   alpha=0x80 if value&0x8000 else 0x40 if value&0x7fff else 0
   expected=((value&31)<<3)|(((value>>5)&31)<<11)|(((value>>10)&31)<<19)|(alpha<<24)
  assert call('gs_sample_texel_uncached',x,y)==expected,(fmt,n)
print('PASS 32 linked PPC depth texels: PSMZ32/24/16/16S, TEXA/AEM and VRAM wrap')

# The unused public state getter is stripped by --gc-sections in the Wii ELF.
# Verify the linked local state's size before addressing its compiler layout.
state_sizes=[int(row.split()[1],16) for row in subprocess.check_output([a.nm,'-S',a.elf],text=True).splitlines() if len(row.split())==4 and row.split()[3]=='s']
assert state_sizes==[ipu_size],state_sizes
for precision in (0,1,2):
 for next_precision in (0,1,2):
  call('dma_init');call('ipu_init');p=syms['s']
  call('ipu_mmio_write32',0x10002010,precision<<16)
  call('ipu_mmio_write32',0x10002000,0x28000000)
  for c in range(3):word(p+io['mpeg.dc']+4*c,900+c)
  call('ipu_mmio_write32',0x10002010,0x40000000|(next_precision<<16))
  assert struct.unpack('>3I',u.mem_read(p+io['mpeg.dc'],12))==((128,)*3 if not precision else (900,901,902))
print('PASS linked PPC IPU soft-reset DC precision rule')
focus,pads,guest=0x817d0000,0x817d1000,0x817d2000
for buttons,action in ((0x1014,1),(0x1094,3)):
 u.mem_write(pads,bytes(padsize*13));call('controller_focus_init',focus)
 call('controller_map_remote',pads+4*padsize,buttons,0,128,128)
 call('controller_focus_update',focus,pads)
 assert call('controller_session',focus,guest)==action
print('PASS linked PPC multi-button pause and explicit HOME+MINUS exit')
