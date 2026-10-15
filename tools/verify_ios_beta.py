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
ios_handle=u.hook_add(UC_HOOK_CODE,ios_hook)
mounted=ram+0x8000;word(mounted,1)
assert call('wii_arm_ios_start',0,mounted)==0
for m in [1,2]:
 mode[0]=m;assert call('wii_arm_ios_start',222,mounted)==0xffffffff
 assert int.from_bytes(u.mem_read(mounted,4),'big')==1 and reloads[0]==0
 assert call('wii_arm_ios_active')==58
print('PASS linked PPC ARM OFF and absent/stub IOS222 preserve IOS58 and mounted storage without reloading')

# Saved choices made in the launcher can select IOS before the first guest.
# They cannot reload an IOS underneath an existing guest session.
mask=(1<<16)|(1<<18)
mode[0]=1
assert call('wii_arm_ios_prepare_first_boot',mask,0,mounted)==0xffffffff
assert call('wii_arm_ios_prepare_first_boot',mask,1,mounted)==0
assert call('wii_arm_ios_prepare_first_boot',1<<16,0,mounted)==0
assert call('wii_arm_ios_prepare_first_boot',1<<18,0,mounted)==0
assert reloads[0]==0 and int.from_bytes(u.mem_read(mounted,4),'big')==1
print('PASS pending first-boot selection, live-session refusal and explicit ARM/IOS conjunction')

# Successful IOS selection and rollback use the actual linked startup code.
u.hook_del(ios_handle)
current=[58];scenario=['success'];fat_calls=[0];opens=[0];reloaded=[]
noop={syms[n] for n in ['fatUnmount','WPAD_Shutdown','PAD_Init','WPAD_Init','WPAD_SetDataFormat','WPAD_SetIdleTimeout'] if n in syms}
for name in ['__io_wiisd','__io_usbstorage']:
 if name in syms:noop.add(int.from_bytes(u.mem_read(syms[name]+28,4),'big'))
def transport(uc,address,size,unused):
 r=0
 if address in noop:pass
 elif address==syms['IOS_GetVersion']:r=current[0]
 elif address==syms['ES_GetTMDViewSize']:word(uc.reg_read(UC_PPC_REG_5),92)
 elif address==syms['ES_GetTMDView']:
  data=bytearray(92);data[12:20]=struct.pack('>Q',0x1000000de);data[88:92]=struct.pack('>HH',4,24);uc.mem_write(uc.reg_read(UC_PPC_REG_5),bytes(data))
 elif address==syms['IOS_ReloadIOS']:
  slot=uc.reg_read(UC_PPC_REG_3);reloaded.append(slot)
  if scenario[0]=='reload_fail' and slot==222:r=0xffffffff
  else:current[0]=slot
 elif address==syms['IOS_Open']:
  opens[0]+=1;r=0xfffffffa if scenario[0]=='missing_mload' else 7
 elif address==syms['IOS_Close']:pass
 elif address==syms['fatInitDefault']:
  fat_calls[0]+=1;r=0 if scenario[0]=='remount_fail' and fat_calls[0]==1 else 1
 else:return
 uc.reg_write(UC_PPC_REG_3,r);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,transport)
for label,result,expected_ios,sequence in [('success',1,222,[222]),('missing_mload',0xfffffffd,58,[222,58]),('reload_fail',0xfffffffe,58,[222]),('remount_fail',0xfffffffb,58,[222,58])]:
 current[0]=58;scenario[0]=label;fat_calls[0]=0;opens[0]=0;reloaded.clear();word(mounted,1)
 assert call('wii_arm_ios_start',222,mounted)==result,label
 assert call('wii_arm_ios_active')==expected_ios and reloaded==sequence,(label,reloaded)
 assert int.from_bytes(u.mem_read(mounted,4),'big')==1,label
print('PASS installed IOS222/MLOAD activation and rollback after MLOAD, reload and SD failures')
