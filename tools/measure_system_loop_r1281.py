"""A real EE/IOP scalar/branch/delay loop through the actual Wii scheduler.
Synthetic code, mocked allocation/cache maintenance, PPC counts not real FPS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_iop_frontend_r1281.py').read_text(),'measure_iop_frontend_r1281.py','exec'))
# Preserve the address/layout fixture, and run genuine guest instructions.
program=[(9<<26)|(2<<21)|(2<<16)|1,(4<<26)|0xfffe,0]
for n,iw in enumerate(program):
 u.mem_write(ram+0x6000+4*n,struct.pack('<I',iw));u.mem_write(io_ram+0x6000+4*n,struct.pack('<I',iw))
samples=[]
for iteration in range(4):
 call('ee_intc_init');call('ee_timers_init');call('iop_intc_init');call('iop_timers_init')
 call('ee_timers_mmio_write32',0x10000020,0xffff);call('ee_timers_mmio_write32',0x10000010,0x83)
 u.mem_write(syms['g_bus_tick_counter'],struct.pack('>Q',100))
 if 'system_profile_reset' in syms:call('system_profile_reset')
 word(state+off['pc'],0x80006000);word(state+off['next_pc'],0x80006004)
 word(state+off['cop0']+12*4,0);word(state+off['cop0']+9*4,0)
 u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0');u.mem_write(state,bytes(512))
 u.mem_write(ios,bytes(128));word(ios+ioff['pc'],0x80006000);word(ios+ioff['next_pc'],0x80006004)
 word(ios+ioff['cop0']+12*4,0);u.mem_write(ios+ioff['halted'],b'\0');u.mem_write(ios+ioff['idle'],b'\0')
 total[0]=0;active[0]=True;result=call('system_run_interleaved',0,512);active[0]=False
 assert result==0 and int.from_bytes(bytes(u.mem_read(state+32,8)),'big')==1366
 assert int.from_bytes(bytes(u.mem_read(ios+8,4)),'big')==171
 assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==4096
 assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80006004
 assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==0x80006008
 samples.append(total[0])
print('SYSTEM_LOOP_MEASURE',json.dumps({'elf':Path(a.elf).name,'samples':samples,'EE_retired':4096,'IOP_retired':512,'scope':'Synthetic real interleaved ADDIU/BEQ/NOP with HBLNK timer; includes new profiling overhead. PPC counts, no Wii cycles/FPS or BIOS workload.'}))
