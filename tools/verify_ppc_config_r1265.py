"""Actual emitted Wii config RPC and MMIO checks; shared HLE, not JIT blocks."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_mc_bootconfig_r1264.py').read_text(),'verify_ppc_mc_bootconfig_r1264.py','exec'))
call('cdvd_config_reset')
def config_rpc(fno,data=b'',recv=8,transfer=None):
 d,h,cd=0x80002000,0x80004000,0x80005000
 for address,value in [(d,0x80003000),(d+4,0x10000),(d+8,len(data) if transfer is None else transfer),(d+16,h),(d+20,0x11000),(d+24,64),(h,64),(h+8,0x8000000a),(h+0x1c,cd),(h+0x20,fno),(h+0x24,len(data)),(h+0x28,0x80006000),(h+0x2c,recv),(0x80007000,12)]:guest(address,value)
 u.mem_write(ram+0x3000,data or b'\0');u.mem_write(ram+0x6000,b'\xcd'*0x408)
 call('sif_cmd_iop_track_bind_sid',cd,0x80000593)
 word(state+off['pc'],0x80007000);word(state+off['next_pc'],0x80007004);word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0')
 for reg,value in [(3,119),(4,d),(5,2)]:u.mem_write(state+off['gpr']+reg*16,struct.pack('>Q',value))
 call('ee_core_step_n',1)
 return struct.unpack('<2I',bytes(u.mem_read(ram+0x6000,8)))
assert config_rpc(0xe,struct.pack('<I',1|(1<<8)|(2<<16)))==(1,0)
payload=bytes(range(1,31))+bytes(0x400-30)
assert config_rpc(0x11,payload)==(2,0)
for v in (0,1,2):call('iop_cdvd_mmio_write8',0x1f402017,v)
call('iop_cdvd_mmio_write8',0x1f402016,0x40)
out=0x81741000
call('iop_cdvd_mmio_read8',0x1f402018,out);assert bytes(u.mem_read(out,1))==b'\0'
for j in range(2):
 call('iop_cdvd_mmio_write8',0x1f402016,0x41);actual=[]
 for k in range(16):call('iop_cdvd_mmio_read8',0x1f402018,out);actual.append(bytes(u.mem_read(out,1))[0])
 expected=list(range(j*15+1,j*15+16));assert actual==expected+[sum(expected)&255]
assert config_rpc(0xe,struct.pack('<I',(1<<8)|(2<<16)))==(1,0)
assert config_rpc(0x10,recv=0x408)==(2,0)
assert bytes(u.mem_read(ram+0x6008,31))==bytes(range(1,31))+b'\xcd'
assert config_rpc(0xf)==(1,0)
assert config_rpc(0x10,recv=0x408)==(0,0x80)
assert config_rpc(0xe,struct.pack('<I',(1<<8)|(2<<16)))==(1,0)
assert config_rpc(0x10,recv=23)==(1,0x80)
assert config_rpc(0xe,struct.pack('<I',0),recv=4)==(0xcdcdcdcd,0xcdcdcdcd)
u.mem_write(ram+0x3000,bytes([7])+bytes(15))
call('cdvd_config_open',1,1,1);assert call('cdvd_config_write',ram+0x3000)==0
assert config_rpc(0xe,struct.pack('<I',(1<<8)|(1<<16)))==(1,0)
assert config_rpc(0x10,recv=0x408)==(0,1)
assert bytes(u.mem_read(ram+0x6008,1))==b'\x07'
assert config_rpc(0xe,struct.pack('<I',1|(1<<8)|(2<<16)))==(1,0)
assert config_rpc(0x11,payload,transfer=15)==(1,0x80)
print('PASS 19 actual Wii ELF config checks: shared RPC/MMIO data, checksum, completed count, bounds and errors')
