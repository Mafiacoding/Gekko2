"""Actual linked PPC startup guard with mocked ES/IOS transport."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
from unicorn import UC_HOOK_CODE
mode=[0];reloads=[0]
def ios_hook(uc,address,size,unused):
 if address==syms['IOS_GetVersion']:r=58
 elif address==syms['ES_GetTMDViewSize']:
  assert uc.reg_read(UC_PPC_REG_3)==1 and uc.reg_read(UC_PPC_REG_4)==222
  if mode[0]==1:r=0xffffff96
  else:word(uc.reg_read(UC_PPC_REG_5),92);r=0
 elif address==syms['ES_GetTMDView']:
  dest=uc.reg_read(UC_PPC_REG_5);data=bytearray(92)
  data[12:20]=struct.pack('>Q',0x1000000de);data[88:92]=struct.pack('>HH',65280,3);uc.mem_write(dest,bytes(data));r=0
 elif address==syms['IOS_ReloadIOS']:reloads[0]+=1;r=0xffffffff
 else:return
 uc.reg_write(UC_PPC_REG_3,r);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ios_hook)
mounted=ram+0x8000;word(mounted,1)
assert call('wii_arm_ios_start',0,mounted)==0
for m in [1,2]:
 mode[0]=m;assert call('wii_arm_ios_start',222,mounted)==0xffffffff
 assert int.from_bytes(u.mem_read(mounted,4),'big')==1 and reloads[0]==0
 assert call('wii_arm_ios_active')==58
print('PASS linked PPC ARM OFF and absent/stub IOS222 preserve IOS58 and mounted storage without reloading')
