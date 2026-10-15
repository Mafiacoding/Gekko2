"""Actual linked PPC IPU MMIO/CSC plus asynchronous worker epoch ownership.
IOS transport is mocked, not evidence of an installed or faster ARM service.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
from unicorn import UC_HOOK_CODE
initial=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
word(syms['gekko2_optimization_mask'],initial&~(1<<16))
setup();call('ipu_init');call('ee_intc_init');call('dma_init');call('dma_bind_ee_ram',ram,0x400000)
input_addr=ram+0x1000;output_addr=ram+0x2000
source=bytes(range(256))+bytes([128])*128;u.mem_write(input_addr,source)
call('ipu_mmio_write32',0x10002000,0x70000001)
for dma_base,addr,qwc,chcr in [(0x1000b000,0x2000,64,0x100),(0x1000b400,0x1000,24,0x101)]:
 for offset,value in [(16,addr),(32,qwc),(0,chcr)]:assert call('dma_mmio_write32',dma_base+offset,value)==1
expected=bytes(c for y in range(256) for c in ([max(0,min(255,(((max(0,y-16)*149)>>6)+1)>>1))]*3+[128]))
assert bytes(u.mem_read(output_addr,1024))==expected
# 64-bit KSEG IPU command access reaches registers, never fake RAM.
call('ee_mem_write64',state,0xb0002000,0,0x40000000)
call('ee_mem_read64',state,0xb0002000)
assert u.reg_read(UC_PPC_REG_3)==0x80000000
# SQ and LQ exercise full 128-bit FIFOs through actual interpreter opcodes.
call('ipu_init');u.mem_write(state+16,struct.pack('>QQ',0xb0007010,0));u.mem_write(state+32,struct.pack('>QQ',0x0123456789abcdef,0xfedcba9876543210))
word(syms['gekko2_optimization_mask'],initial&~1)
u.mem_write(ram+0x200000,struct.pack('<I',(0x1f<<26)|(1<<21)|(2<<16)))
word(state+off['pc'],0x200000);word(state+off['next_pc'],0x200004)
assert call('ee_core_step_n',1)==1
call('ipu_mmio_write32',0x10002000,0x40000000);dest=0x81750000;call('ipu_mmio_read32',0x10002000,dest)
assert bytes(u.mem_read(dest,4))==bytes.fromhex('efcdab89')
# Real LQ consumes exactly one output QWC, preserving little-endian halves.
call('ipu_init');call('dma_init');call('ipu_mmio_write32',0x10002000,0x70000001)
for offset,value in [(16,0x1000),(32,24),(0,0x101)]:call('dma_mmio_write32',0x1000b400+offset,value)
u.mem_write(state+16,struct.pack('>QQ',0xb0007000,0))
u.mem_write(ram+0x200000,struct.pack('<I',(0x1e<<26)|(1<<21)|(3<<16)))
word(state+off['pc'],0x200000);word(state+off['next_pc'],0x200004)
assert call('ee_core_step_n',1)==1
lo,hi=struct.unpack('<QQ',expected[:16]);assert bytes(u.mem_read(state+48,16))==struct.pack('>QQ',lo,hi)
# Async transport emulator. It retains request vectors until callback.
queue=[];opens=[0]
def ipc_hook(uc,address,size,user):
 if address==syms['IOS_Open']:
  opens[0]+=1;uc.reg_write(UC_PPC_REG_3,7)
 elif address==syms['IOS_Ioctl']:
  out=uc.reg_read(UC_PPC_REG_7);uc.mem_write(out,struct.pack('>8I',0x474b4131,1,1,8,0,0,0,0));uc.reg_write(UC_PPC_REG_3,32)
 elif address==syms['IOS_IoctlvAsync']:
  vec=uc.reg_read(UC_PPC_REG_7);callback=uc.reg_read(UC_PPC_REG_8)
  header,hs,inp,isize,out,osize=struct.unpack('>6I',uc.mem_read(vec,24))
  queue.append((callback,out,osize,bytes(uc.mem_read(header,32)),bytes(uc.mem_read(inp,isize))))
  uc.reg_write(UC_PPC_REG_3,0)
 elif address==syms['DCInvalidateRange']:pass
 else:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ipc_hook)
word(syms['gekko2_optimization_mask'],initial|(1<<16));call('arm_worker_reset')
assert opens[0]==1 and call('arm_worker_submit_csc',input_addr,0x70000001,0,0)==1
assert call('arm_worker_take',output_addr,1024)==0
callback,out,n,header,data=queue[-1];assert header[:8]==struct.pack('>2I',0x474b4131,1) and data==source and n==1024
assert call('arm_worker_submit_csc',input_addr,0x70000001,0,0)==0
u.mem_write(out,expected)
def finish(address,result):
 u.reg_write(UC_PPC_REG_1,0x81780000);u.reg_write(UC_PPC_REG_3,result);u.reg_write(UC_PPC_REG_4,0);u.reg_write(UC_PPC_REG_LR,0x817ff000)
 u.emu_start(address,0x817ff000,count=100000)
 assert u.reg_read(UC_PPC_REG_PC)==0x817ff000
finish(callback,1024);assert call('arm_worker_take',output_addr,1024)==1
assert bytes(u.mem_read(output_addr,1024))==expected
assert call('arm_worker_submit_csc',input_addr,0x70000001,0,0)==1
callback,out,n,*_=queue[-1];call('arm_worker_reset')
assert call('arm_worker_submit_csc',input_addr,0x70000001,0,0)==0
u.mem_write(output_addr,bytes([0x33])*1024);finish(callback,n)
assert call('arm_worker_take',output_addr,n)==0xffffffff and bytes(u.mem_read(output_addr,n))==bytes([0x33])*n
assert opens[0]==1
print('PASS linked PPC CSC/DMA byte order, 64/128-bit IPU access, asynchronous ownership and stale cold-boot rejection')
