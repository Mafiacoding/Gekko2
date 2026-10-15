"""Linked Wii THREADMAN no-event scan cost and exact state comparison.
The clock is the existing instruction counter; this is not a wall-time test.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
fields=['instructions_executed','pc','next_pc']
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/iop/iop_core.h"\nconst unsigned layout[]={sizeof(iop_state_t),'+','.join('offsetof(iop_state_t,'+f+')' for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 isize,*offsets=struct.unpack('>4I',raw.read_bytes());io=dict(zip(fields,offsets))
ist=call('iop_core_get_state');original=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
snapshots=[];cost={}
for fast in [0,1]:
 call('iop_hle_thread_init');u.mem_write(ist,bytes(isize))
 word(syms['gekko2_optimization_mask'],(original&~(1<<21))|((1<<21) if fast else 0))
 word(ist+31*4,0x1234)
 assert call('iop_hle_thread_try_handle',ist,0x144)==1
 call('iop_hle_thread_tick',ist) # Warm the derived hint, without exposing it.
 total[0]=0;active[0]=True
 for tick in range(1,1001):
  u.mem_write(ist+io['instructions_executed'],struct.pack('>Q',tick))
  call('iop_hle_thread_tick',ist)
 active[0]=False;cost['on' if fast else 'off']=total[0]
 if fast:
  call('iop_hle_thread_deadline_stat',0);assert u.reg_read(UC_PPC_REG_4)==1001
 # The public mutable-blob accessor is dead-stripped in the Wii application.
 # Derive the private checkpoint size from this source and locate that unique
 # ELF object without adding test exports to production.
 with tempfile.TemporaryDirectory() as d:
  src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
  src.write_text('#include "hw/iop_hle_thread.c"\nconst unsigned test_size __attribute__((section(".test_layout")))=sizeof(g);\n')
  subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-I'+str(root/'source'),'-c',str(src),'-o',str(obj)],check=True)
  subprocess.run([oc,'-O','binary','-j','.test_layout',str(obj),str(raw)],check=True)
  n=struct.unpack('>I',raw.read_bytes())[0]
 objects=[]
 for row in subprocess.check_output([a.nm,'-S',a.elf],text=True).splitlines():
  parts=row.split()
  if len(parts)==4 and parts[3]=='g' and int(parts[1],16)==n:objects.append(int(parts[0],16))
 assert len(objects)==1;blob=objects[0]
 snapshots.append(bytes(u.mem_read(ist,isize))+bytes(u.mem_read(blob,n)))
assert snapshots[0]==snapshots[1]
print('IOP_DEADLINE_COST '+json.dumps(cost))
print('PASS 1000 linked THREADMAN ticks: identical CPU/checkpoint state, no synthetic progress')
