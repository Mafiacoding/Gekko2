"""Eight EE ADDIU retirements with a real active HBLNK timer. No FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_frontend_r1276.py').read_text(),'measure_ee_frontend_r1276.py','exec'))
reset_frontend()
for n in range(8):guest(0x80006000+4*n,(9<<26)|(2<<21)|(2<<16)|1)
call('ee_timers_init');call('ee_intc_init')
call('ee_timers_mmio_write32',0x10000020,0xffff);call('ee_timers_mmio_write32',0x10000010,0x83)
samples=[]
for iteration in range(4):
 word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
 word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0');u.mem_write(state,bytes(512))
 u.mem_write(syms['g_bus_tick_counter'],struct.pack('>Q',100))
 total[0]=0;active[0]=True;retired=call('ee_core_step_n',8);active[0]=False
 assert retired==8 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==8
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006020
 samples.append(total[0])
print('EE_HBLNK_MEASURE',json.dumps({'elf':Path(a.elf).name,'samples':samples,'scope':'Eight actual EE ADDIU with HBLNK enabled and full retirement. PPC instructions, not real Wii cycles/FPS or BIOS workload.'}))
