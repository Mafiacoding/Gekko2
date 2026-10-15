"""Exact 32-byte ARM capability wire validation, diagnostics and cache order."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
phase=[0];events=[];closes=[0]
good=[0x474b4131,1,1,8,0,0,0,0]
def ipc(uc,address,size,user):
 if address==syms['IOS_Open']:uc.reg_write(UC_PPC_REG_3,7)
 elif address==syms['IOS_Close']:closes[0]+=1;uc.reg_write(UC_PPC_REG_3,0)
 elif address==syms['DCFlushRange']:events.append('flush')
 elif address==syms['DCInvalidateRange']:events.append('invalidate')
 elif address==syms['IOS_Ioctl']:
  events.append('query');raw=good.copy()
  if 1<=phase[0]<=4:raw[phase[0]-1]=0 if phase[0]!=2 else 2
  out=uc.reg_read(UC_PPC_REG_7);assert uc.reg_read(UC_PPC_REG_8)==32 and out%32==0
  uc.mem_write(out,struct.pack('>8I',*raw));uc.reg_write(UC_PPC_REG_3,31 if phase[0]==0 else 32)
 else:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ipc)
mask=syms['gekko2_optimization_mask'];word(mask,int.from_bytes(u.mem_read(mask,4),'big')|(1<<16))
for p in range(6):
 phase[0]=p;events.clear();call('arm_worker_begin_session');call('arm_worker_reset')
 assert events==['flush','query','invalidate'],events
 assert call('arm_worker_diagnostic',3)==(p+1 if p<5 else 0)
 assert call('arm_worker_available')==(1 if p==5 else 0)
 for i in range(8):
  expected=good[i]
  if 1<=p<=4 and i==p-1:expected=0 if p!=2 else 2
  assert call('arm_worker_diagnostic',4+i)==expected
assert closes[0]==5
print('PASS transport length, magic, version, capability and block-limit declines; exact raw BE diagnostics; flush/query/invalidate order; valid worker acceptance')
