"""Actual linked Wii-PPC scanout upload: preserve full RGB gamut."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0],'gx_fixture','exec'))
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_4, UC_PPC_REG_5
def platform(uc,address,size,user):
 if address in {syms[n] for n in ['PPCDCacheFlushAsync','PPCDCacheInvalidate'] if n in syms}:
  uc.reg_write(UC_PPC_REG_3,0);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,platform)
for x in range(256):
 for y in range(4):call('gs_mem_write_psmct32',0,256,x,y,x|((255-x)<<8)|(x<<16))
dst=0x81740000
assert call('gs_gx_pack_rgba8_profile_impl',dst,4096,0,256,0,0,256,4)==4096
out=bytes(u.mem_read(dst,4096))
for x in range(256):
 for y in range(4):
  t=(x//4)*64+y*8+(x%4)*2
  assert out[t]==255 and out[t+1]==x and out[t+32]==255-x and out[t+33]==x,(x,y)
  assert call('gs_mem_read_psmct32',0,256,x,y)==x|((255-x)<<8)|(x<<16)
print('PASS linked PPC: all 256 RGB levels preserved, no RGB-black lift, opaque scanout alpha, unchanged GS VRAM')
