"""Linked PPC startup failure survives IPU resets and retries at new session."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
phase=[0];opens=[0]
def ipc(uc,address,size,user):
 if address==syms['IOS_Open']:
  opens[0]+=1;uc.reg_write(UC_PPC_REG_3,0xfffffffA if phase[0]==0 else 7)
 elif address==syms['arm_loader_start']:uc.reg_write(UC_PPC_REG_3,1)
 elif address==syms['usleep']:uc.reg_write(UC_PPC_REG_3,0)
 elif address==syms['IOS_Ioctl']:
  out=uc.reg_read(UC_PPC_REG_7);uc.mem_write(out,struct.pack('>8I',0x474b4131,1,1,8,0,0,0,0));uc.reg_write(UC_PPC_REG_3,32)
 else:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ipc)
mask=syms['gekko2_optimization_mask'];word(mask,int.from_bytes(u.mem_read(mask,4),'big')|(1<<16))
call('arm_worker_begin_session');call('arm_worker_reset')
assert opens[0]==33 and call('arm_worker_available')==0
assert call('arm_worker_diagnostic',0)==0xfffffffa
assert call('arm_worker_diagnostic',2)==33
call('arm_worker_reset');assert opens[0]==33
call('arm_worker_stat',3);assert u.reg_read(UC_PPC_REG_4)==1
phase[0]=1;call('arm_worker_begin_session');call('arm_worker_reset')
assert opens[0]==34 and call('arm_worker_available')==1
assert call('arm_worker_diagnostic',1)==32 and call('arm_worker_diagnostic',2)==1
print('PASS bounded connection failure persists across IPU resets; next guest session retries and validates capabilities')
