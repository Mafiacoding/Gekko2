"""Actual linked PPC RAM client: caps, vectors, verification and failure fallback."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask=syms['gekko2_optimization_mask'];word(mask,int.from_bytes(u.mem_read(mask,4),'big')|(1<<23))
phase=[0];jobs=[]
def ipc(uc,address,size,user):
 if address==syms['IOS_Open']:result=5
 elif address==syms['IOS_Close']:result=0
 elif address==syms['IOS_Ioctl']:
  out=uc.reg_read(UC_PPC_REG_7);uc.mem_write(out,struct.pack('>8I',0x474b4131,1,3,8,0,0,0,0));result=32
 elif address==syms['IOS_Ioctlv']:
  assert uc.reg_read(UC_PPC_REG_4)==2 and uc.reg_read(UC_PPC_REG_5)==1 and uc.reg_read(UC_PPC_REG_6)==1
  vectors=uc.reg_read(UC_PPC_REG_7);h,hs,d,n=struct.unpack('>4I',u.mem_read(vectors,16));assert hs==32
  header=bytes(u.mem_read(h,32));assert header[:8]==struct.pack('>2I',0x474b4131,1)
  jobs.append((d,n,header[11]));uc.mem_write(d,bytes([header[11]])*n)
  if phase[0]==1:uc.mem_write(d+n-1,b'\xa5')
  result=n if phase[0]!=2 else 0xffffffff
 else:return
 uc.reg_write(UC_PPC_REG_3,result);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ipc)
for value in (0,0x7b,255):
 assert call('arm_ram_worker_fill',ram,value,65536)==1
 assert bytes(u.mem_read(ram,65536))==bytes([value])*65536
phase[0]=1;assert call('arm_ram_worker_fill',ram,0,65536)==0
phase[0]=2;assert call('arm_ram_worker_fill',ram,0,65536)==0
assert call('arm_ram_worker_fill',ram+1,0,65536)==0 and len(jobs)==5
print('PASS linked PPC ARM bulk client: complete vectors/caps, three verified fills; short/malformed result and corrupted last byte rejected; misaligned span never submitted')
