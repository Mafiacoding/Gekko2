"""Execute the shipped ARM ELF capability handler, not a PPC transport mock."""
from pathlib import Path
import struct,subprocess,argparse
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn.arm_const import *
p=argparse.ArgumentParser();p.add_argument('elf');p.add_argument('--nm',required=True);a=p.parse_args()
data=Path(a.elf).read_bytes();assert data[:6]==b'\x7fELF\x01\x02'
h=struct.unpack_from('>16sHHIIIIIHHHHHH',data);u=Uc(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN)
u.mem_map(0x13700000,0x80000);u.mem_map(0x13800000,0x10000);u.mem_map(0x10000000,0x10000)
for i in range(h[10]):
 t,off,va,pa,fs,ms,flags,align=struct.unpack_from('>8I',data,h[5]+i*h[9])
 if t==1:u.mem_write(va,data[off:off+fs])
syms={}
for line in subprocess.check_output([a.nm,a.elf],text=True).splitlines():
 s=line.split()
 if len(s)==3:syms[s[2]]=int(s[0],16)
flushes=[]
def platform(uc,address,size,user):
 if address==syms['ios_flush']:
  flushes.append((uc.reg_read(UC_ARM_REG_R0),uc.reg_read(UC_ARM_REG_R1)))
 elif address!=syms['ios_invalidate']:return
 uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,platform)
message=0x10005000;out=0x10004000;end=0x1380f000
u.mem_write(message,struct.pack('>8I',6,0,0,0,0,0,out,32));u.mem_write(out,b'\xa5'*32)
u.reg_write(UC_ARM_REG_SP,0x13808000);u.reg_write(UC_ARM_REG_LR,end);u.reg_write(UC_ARM_REG_R0,message)
u.emu_start(syms['gekko2_ios_handle'],end,count=100000)
assert u.reg_read(UC_ARM_REG_PC)==end and u.reg_read(UC_ARM_REG_R0)==32
words=struct.unpack('>8I',u.mem_read(out,32));assert words==(0x474b4131,1,1,8,0,0,0,0),words
assert flushes==[(out,32)],flushes
print('PASS shipped big-endian ARM ELF returns32 and exact capabilities 474b4131,00000001,00000001,00000008,0,0,0,0; output flush covers32 bytes')
