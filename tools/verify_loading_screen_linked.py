#!/usr/bin/env python3
"""Render the actual Wii loading/error function on simulated PPC into private XFB.
Only video/cache/elapsed formatting services are stubbed; pixels use linked code.
Not physical GX/VI execution. Optionally save a synthetic PPM preview outside Git.
"""
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_LR,UC_PPC_REG_PC,UC_PPC_REG_3,UC_PPC_REG_4
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0].replace('count=10000000','count=80000000'), 'loader', 'exec'))
u.mem_map(0x92000000,4096);u.mem_map(0x93000000,1024*1024)
def word(at,value):u.mem_write(at,struct.pack('>I',value))
word(syms['rmode'],0x92000000);word(syms['xfb'],0x93000000)
u.mem_write(0x92000100,b'Starting PlayStation 2 BIOS\0')
u.mem_write(0x92000200,b'Waiting for the first emulated picture...\0')
stubs={syms[n] for n in ['DCFlushRange','VIDEO_SetNextFramebuffer','VIDEO_Flush']}
def platform(uc,address,size,user):
 if address==syms['snprintf']:
  text=b'Elapsed 1:05 - emulation may take a long time.\0'
  dest=uc.reg_read(UC_PPC_REG_3);limit=uc.reg_read(UC_PPC_REG_4)
  assert limit>=len(text);uc.mem_write(dest,text);uc.reg_write(UC_PPC_REG_3,len(text)-1)
 elif address not in stubs:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,platform)
for height in [480,576]:
 u.mem_write(0x92000004,struct.pack('>3H',640,height,height))
 for error in [0,1]:
  size=640*height*2;u.mem_write(0x93000000,bytes(size)+b'\xa5'*64)
  call('draw_loading_screen',0x92000100,0x92000200,0,65000,error)
  assert bytes(u.mem_read(0x93000000+size,64))==b'\xa5'*64,'XFB overflow'
  pixels=bytes(u.mem_read(0x93000000,size))
  # A supported font scale must produce bright title glyphs in this region.
  bright=0
  for y in range(170*height//480,212*height//480):
   row=pixels[y*1280:(y+1)*1280]
   bright+=sum(row[x*2]>100 for x in range(42,500))
  assert bright>200,(height,error,'missing title',bright)
  print('PASS linked loading screen',height,'error='+str(error),'title_pixels='+str(bright),'bounded XFB')
# Preview is synthetic launcher UI, never guest/owner pixels.
import os
preview=os.environ.get('GEKKO2_LOADING_PREVIEW')
if preview:
 def clip(x):return max(0,min(x,255))
 rgb=bytearray()
 for i in range(0,len(pixels),4):
  y0,cb,y1,cr=pixels[i:i+4];cb-=128;cr-=128
  for y in [y0,y1]:rgb.extend([clip(y+(359*cr>>8)),clip(y-((88*cb+183*cr)>>8)),clip(y+(454*cb>>8))])
 Path(preview).write_bytes(('P6\n640 '+str(height)+'\n255\n').encode()+rgb)
