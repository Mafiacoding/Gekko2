"""Independent architectural observers and external-write residency fences.
Actual linked PPC, synthetic programs, mocked platform services only.
"""
from pathlib import Path
prefix=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction sequences')[0]
exec(compile(prefix,'verify_iop_blocks_internal.py','exec'))
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\n#include "core/iop/iop_core.h"\nconst unsigned offsets[]={offsetof(ee_state_t,gpr_generation),offsetof(iop_state_t,gpr_generation)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 egen,igen=struct.unpack('>2I',raw.read_bytes())
def qreg(st,n):return int.from_bytes(u.mem_read(st+n*16,8),'big')
def counter(n):
 hi=call(n);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
checks=0
for epoch in [0,0xfffffffe]:
 for change in range(1,9):
  for dest in [1,2,3]:
   extension_setup();a0=0x123456789abcdef0;b0=0x0f0e0d0c0b0a0908
   u.mem_write(state+16,struct.pack('>QQ',a0,0x9988776655443322));u.mem_write(state+32,struct.pack('>QQ',b0,0x7766554433221100))
   word(state+egen,epoch)
   iw=(1<<21)|(2<<16)|(dest<<11)|0x25
   for n in range(8):u.mem_write(ram+base+n*4,struct.pack('<I',iw))
   expected=[a0,b0];observed=[]
   # The two input registers alone form the independent guest oracle.
   def observer(uc,address,size,user):
    if address not in {syms['ee_core_block_commit'],syms.get('ee_core_block_boundary',0)}:return
    v=expected[0]|expected[1];assert qreg(state,dest)==v,('canonical EE helper view',change,dest,len(observed))
    if dest<3:expected[dest-1]=v
    observed.append(v)
    if len(observed) in [change,change+1]:
     r=1 if len(observed)==change else 2;val=0xfedcba9876543210 if r==1 else 0xa5a5a5a55a5a5a5a
     expected[r-1]=val;u.mem_write(state+r*16,struct.pack('>QQ',val,0x9988776655443322 if r==1 else 0x7766554433221100))
     word(state+egen,(int.from_bytes(u.mem_read(state+egen,4),'big')+1)&0xffffffff)
   h=u.hook_add(UC_HOOK_CODE,observer)
   assert call('ee_core_step_n',8)==8;u.hook_del(h)
   assert len(observed)==8 and qreg(state,1)==expected[0] and qreg(state,2)==expected[1]
   checks+=1
for epoch in [0,0xfffffffe]:
 for change in range(1,8):
  regs=[0]*32;regs[1]=0x12345678;regs[2]=0x0f0e0d0c
  iw=(1<<21)|(2<<16)|(3<<11)|0x25
  setup([iw]*8,regs);word(ist+igen,epoch);seen=[];expected=[regs[1],regs[2]]
  def iobserver(uc,address,size,user):
   if address!=syms['iop_core_block_prepare']:return
   prev=uc.reg_read(UC_PPC_REG_6)
   if prev==0xffffffff:return
   assert int.from_bytes(u.mem_read(ist+12,4),'big')==expected[0]|expected[1]
   seen.append(prev)
   if len(seen) in [change,change+1]:
    r=1 if len(seen)==change else 2;v=0xfedcba98 if r==1 else 0xa5a55a5a
    expected[r-1]=v;word(ist+r*4,v);word(ist+igen,(int.from_bytes(u.mem_read(ist+igen,4),'big')+1)&0xffffffff)
  h=u.hook_add(UC_HOOK_CODE,iobserver)
  assert call('iop_core_step_n',8)==8;u.hook_del(h)
  assert int.from_bytes(u.mem_read(ist+12,4),'big')==expected[0]|expected[1];checks+=1
if enabled:
 assert counter('ppc_dynarec_get_resident_blocks')>0
 assert counter('ppc_dynarec_get_resident_loads')>0
 print('RESIDENT_COMPILE_COUNTERS',counter('ppc_dynarec_get_resident_blocks'),counter('ppc_dynarec_get_resident_loads'),counter('ppc_dynarec_get_resident_refresh_edges'))
print('PASS',checks,'independent canonical helper views, external GPR changes, destination aliases and generation wrap residency oracles')
