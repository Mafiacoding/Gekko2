"""Eight genuine LD instructions with full EE retirement."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_frontend_r1276.py').read_text(),'measure_ee_frontend_r1276.py','exec'))
reset_frontend()
for n in range(8):guest(0x80006000+4*n,(55<<26)|(3<<21)|(2<<16))
u.mem_write(ram+0x7400,(0x1234567890abcdef).to_bytes(8,'little'))
samples=[]
for iteration in range(4):
 word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
 word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0')
 u.mem_write(state,bytes(512));u.mem_write(state+48,(0x80007400).to_bytes(8,'big')+bytes(8))
 total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
 assert retired==8 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==0x1234567890abcdef
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006020
 samples.append(total[0])
print('LD_RETIRE_MEASURE',json.dumps({'elf':Path(a.elf).name,'cold':samples[0],'warm':samples[1:],'scope':'Eight RAM LD with full retirement; PPC counts, not Wii cycles/FPS or BIOS workload.'}))
