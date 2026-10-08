"""Linked PPC async completion wakes IPU without guest MMIO polling.
IOS requests/callbacks are mocked; scheduler/IPU execute the Wii ELF.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ipu_arm_r1335.py').read_text(),'verify_ipu_arm_r1335.py','exec'))
call('ipu_init');call('dma_init');call('ee_intc_init')
u.mem_write(input_addr,source)
call('ipu_mmio_write32',0x10002000,0x70000001)
assert call('ipu_input_write',input_addr,24)==8
call('ipu_service')
assert call('ipu_input_write',input_addr+128,16)==8
call('ipu_service')
assert call('ipu_input_write',input_addr+256,8)==8
call('ipu_service')
assert call('arm_worker_completion_ready')==0
callback,out,n,header,data=queue[-1];assert data==source
u.mem_write(out,expected);finish(callback,n)
assert call('arm_worker_completion_ready')==1
call('system_run_interleaved',0,1)
assert call('arm_worker_completion_ready')==0
got=0
for g in range(16):
 got+=call('ipu_output_read',output_addr+got*16,64-got)
 call('ipu_service')
 if got==64:break
assert got==64 and bytes(u.mem_read(output_addr,1024))==expected
print('PASS linked PPC scheduler wakes completed ARM CSC with no guest IPU polling')
