"""Actual PPC MLOAD RAM jobs and BIOS fallback, mocked IOS memory transport.
 * No console access or measured ARM speedup is claimed.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
fields=['ram','ram_size','cop0','pc','next_pc']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),'+','.join('offsetof(iop_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 isize,*offsets=struct.unpack('>6I',raw.read_bytes());io=dict(zip(fields,offsets))
ist=call('iop_core_get_state');src_addr=ram+0x40000;dst_addr=ram+0x80000;n=65536
initial=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
mode=[0];cursor=[0];requests=[]
def ipc(uc,address,size,user):
 if address==syms['IOS_Open']:
  assert bytes(uc.mem_read(uc.reg_read(UC_PPC_REG_3),11))==b'/dev/mload\0'
  r=-6 if mode[0]==1 else 7
 elif address==syms['IOS_Close']:r=0
 elif address==syms['IOS_Seek']:
  cursor[0]=uc.reg_read(UC_PPC_REG_4);requests.append(('seek',cursor[0]));r=-22 if mode[0]==2 else 1
 elif address in [syms['IOS_Read'],syms['IOS_Write']]:
  p=uc.reg_read(UC_PPC_REG_4);length=uc.reg_read(UC_PPC_REG_5);a=cursor[0]|0x80000000
  request='read' if address==syms['IOS_Read'] else 'write';requests.append((request,length))
  count=length-32 if mode[0]==3 else length
  if mode[0]!=4:
   if request=='read':uc.mem_write(p,bytes(uc.mem_read(a,count)))
   else:uc.mem_write(a,bytes(uc.mem_read(p,count)))
  cursor[0]+=length;r=-5 if mode[0]==5 else 1
 elif address==syms['DCInvalidateRange']:r=0
 else:return
 uc.reg_write(UC_PPC_REG_3,r&0xffffffff);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ipc)
pattern=bytes(range(256))*256
def reset(m=0,on=True):
 call('arm_ram_reset');mode[0]=m;requests.clear()
 word(syms['gekko2_optimization_mask'],(initial|(1<<14))&~(1<<23)|((1<<23) if on else 0))
 u.mem_write(src_addr,pattern);u.mem_write(dst_addr,bytes([0xa5])*n)
def stat(k):
 call('arm_ram_stat',k);return (u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4)
for operation in ['copy','fill']:
 for m in range(6):
  reset(m)
  if operation=='copy':r=call('arm_ram_copy',dst_addr,src_addr,n)
  else:r=call('arm_ram_fill',dst_addr,0x6d,n)
  assert r==(1 if m==0 else 0),(operation,m,r)
  if not m:
   assert bytes(u.mem_read(dst_addr,n))==(pattern if operation=='copy' else bytes([0x6d])*n)
   assert stat(2)==1 and stat(3)==n and call('arm_ram_available')==1
  else:assert stat(2)==0 and stat(4)>0
# Tiny, misaligned, overlapping, wrapped/out-of-range and disabled jobs never
# enter MLOAD or mutate RAM.
for d,s,length,on in [(dst_addr,src_addr,32,True),(dst_addr+1,src_addr,n,True),(src_addr+32,src_addr,n,True),(0x93ffffe0,src_addr,n,True),(dst_addr,src_addr,n,False)]:
 reset(0,on);before=bytes(u.mem_read(src_addr,n))+bytes(u.mem_read(dst_addr,n))
 assert call('arm_ram_copy',d,s,length)==0 and not requests
 assert bytes(u.mem_read(src_addr,n))+bytes(u.mem_read(dst_addr,n))==before
# Real IOP A0 MEMCPY/MEMSET: partial failed ARM work is overwritten by the
# original CPU path; return registers and guest PC match the ordinary helper.
for function,value in [(0x2a,0x40000),(0x2b,0x6d)]:
 for m in range(6):
  reset(m);u.mem_write(ist,bytes(isize));word(ist+io['ram'],ram);word(ist+io['ram_size'],0x200000)
  for reg,val in [(4,0x80080000),(5,value),(6,n),(9,function),(31,0x100000)]:word(ist+reg*4,val)
  assert call('iop_hle_bios_try_handle',ist,0xa0)==1
  expected=pattern if function==0x2a else bytes([0x6d])*n
  assert bytes(u.mem_read(dst_addr,n))==expected,(function,m)
  assert int.from_bytes(u.mem_read(ist+8,4),'big')==0x80080000
  assert int.from_bytes(u.mem_read(ist+io['pc'],4),'big')==0x100000
# ARM writes must also be seen by an already compiled IOP owner.
pattern=struct.pack('<8I',*([(9<<26)|(2<<21)|(2<<16)|9]+[(9<<26)|(2<<21)|(2<<16)|1]*7))+bytes(n-32)
reset();u.mem_write(ist,bytes(isize));word(ist+io['ram'],ram);word(ist+io['ram_size'],0x200000)
for name in ['iop_jit_reset_for_test','iop_intc_init','iop_timers_init','iop_asyncio_init','iop_hle_thread_init']:call(name)
u.mem_write(dst_addr,struct.pack('<8I',*([(9<<26)|(2<<21)|(2<<16)|1]*8)))
def grant():
 word(ist+io['pc'],0x80080000);word(ist+io['next_pc'],0x80080004)
 assert call('iop_core_step_n',8)==8
grant();assert int.from_bytes(u.mem_read(ist+8,4),'big')==8
assert call('arm_ram_copy',dst_addr,src_addr,n)==1
grant();assert int.from_bytes(u.mem_read(ist+8,4),'big')==24
assert call('arm_ram_fill',dst_addr,0,n)==1
grant();assert int.from_bytes(u.mem_read(ist+8,4),'big')==24
print('PASS linked PPC ARM copy/fill, status 1, missing/partial/error data, guarded spans, exact BIOS CPU fallback and warm IOP code invalidation')
