"""Count EE cold/warm frontend PPC work, including ordinary CPU retirement."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_cache_r1267.py').read_text(),'verify_ppc_cache_r1267.py','exec'))
from unicorn import UC_HOOK_CODE
import json
active=[False];total=[0]
def count_hook(uc,address,size,data):
 if active[0]:total[0]+=1
u.hook_add(UC_HOOK_CODE,count_hook);u.ctl_remove_cache(0x80000000,0x81800000)
reset_frontend()
for n in range(8):guest(0x80006000+4*n,(9<<26)|(2<<21)|(2<<16)|1)
results=[]
for iteration in range(4):
 word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
 word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
 u.mem_write(state+32,bytes(16))
 total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
 assert retired==8 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==8
 results.append(total[0])
print('EE_FRONTEND_MEASURE',json.dumps({'elf':Path(a.elf).name,'retired_per_sample':8,'cold':results[0],'warm':results[1:],'scope':'PPC instruction counts for eight ADDIU instructions, complete EE epilogue. Not Wii cycles/FPS or a BIOS benchmark.'}))
