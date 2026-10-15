"""Execute the linked interrupt-thread regression through actual PPC functions."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
fields=['in_use','status','priority','wait_type','pc','next_pc','gpr']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_hle_thread.c"\ntypedef __typeof__(g) scheduler;\nconst unsigned layout[] __attribute__((section(".test_layout")))={sizeof(ee_tcb_t),offsetof(scheduler,threads),offsetof(scheduler,thread_count),'+','.join('offsetof(ee_tcb_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-I'+str(root/'source'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.test_layout',str(obj),str(raw)],check=True)
 stride,threads,count,*offsets=struct.unpack('>'+str(3+len(fields))+'I',raw.read_bytes());t=dict(zip(fields,offsets))
checks=0
for gate in (0x10000,0x10002,0x10001):
 setup();call('ee_hle_thread_init');word(state+off['pc'],0x1000);word(state+off['next_pc'],0x1004);word(state+off['cop0']+12*4,0x10001)
 u.mem_write(state+29*16,struct.pack('>QQ',0x1ff000,0));u.mem_write(state+18*16,struct.pack('>QQ',0x1122334455667788,0x99aabbccddeeff00))
 call('ee_hle_thread_try_handle',state,47,0x1000,0)
 word(state+off['pc'],0x1000);word(state+off['next_pc'],0x1004)
 original=bytes(u.mem_read(state,size))
 # The checkpoint accessor is stripped from Wii builds. Observe the
 # real getter's load address, then subtract the compiler-derived offset.
 from unicorn import UC_HOOK_MEM_READ
 reads=[]
 def read_counter(uc,access,address,length,value,user):
  if length==4 and address<0x81700000:reads.append(address)
 hook=u.hook_add(UC_HOOK_MEM_READ,read_counter)
 assert call('ee_hle_thread_get_current_thread_id')==1
 u.hook_del(hook);assert len(reads)==1,reads
 g=reads[0]-(count+4);root_tcb=g+threads;child=root_tcb+stride
 word(g+count,2);word(root_tcb+t['priority'],30)
 for field,value in [('in_use',1),('status',4),('priority',10),('wait_type',1),('pc',0x2000),('next_pc',0x2004)]:word(child+t[field],value)
 u.mem_write(child+t['gpr']+29*16,struct.pack('>QQ',0x2ff000,0))
 saved=bytes(u.mem_read(root_tcb+t['gpr'],512))
 word(state+off['pc'],0x3000);word(state+off['next_pc'],0x3004);word(state+off['cop0']+12*4,gate)
 u.mem_write(state+29*16,struct.pack('>QQ',0x3ff000,0));u.mem_write(state+4*16,struct.pack('>QQ',2,0))
 assert call('ee_hle_thread_try_handle',state,0xffffffcc,0x3000,0)==1
 assert call('ee_hle_thread_get_current_thread_id')==1
 assert int.from_bytes(u.mem_read(child+t['status'],4),'big')==2
 assert bytes(u.mem_read(root_tcb+t['gpr'],512))==saved
 if gate!=0x10001:
  call('ee_hle_thread_check_preempt',state);assert call('ee_hle_thread_get_current_thread_id')==1
  u.mem_write(state,original)
 call('ee_hle_thread_check_preempt',state);assert call('ee_hle_thread_get_current_thread_id')==2
 assert int.from_bytes(u.mem_read(state+off['pc'],4),'big')==0x2000
 assert int.from_bytes(u.mem_read(state+29*16,8),'big')==0x2ff000
 if gate!=0x10001:
  assert bytes(u.mem_read(root_tcb+t['gpr'],512))==original[:512]
  assert int.from_bytes(u.mem_read(root_tcb+t['pc'],4),'big')==0x1000
 checks+=1
print(json.dumps({'linked_PPC_interrupt_context_cases':checks,'all_passed':True,'scope':'Production syscall/preemption functions; generic test addresses; not game progression.'}))
