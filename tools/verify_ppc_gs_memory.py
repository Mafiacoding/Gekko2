"""Execute the emitted Wii GS memory helpers on a big-endian PPC CPU."""
import argparse,struct,subprocess
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import UC_PPC_REG_1,UC_PPC_REG_2,UC_PPC_REG_3,UC_PPC_REG_13,UC_PPC_REG_LR,UC_PPC_REG_PC
p=argparse.ArgumentParser();p.add_argument('elf');p.add_argument('--nm',required=True);a=p.parse_args()
nm=subprocess.check_output([a.nm,a.elf],text=True);syms={l.split()[2]:int(l.split()[0],16) for l in nm.splitlines() if len(l.split())==3 and l.split()[0].isalnum()}
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x80000000,24*1024*1024)
e=open(a.elf,'rb').read();off=struct.unpack_from('>I',e,28)[0];stride,num=struct.unpack_from('>HH',e,42)
for i in range(num):
 t,f,v,pa,sz,ms,flags,al=struct.unpack_from('>8I',e,off+i*stride)
 if t==1 and sz:u.mem_write(v,e[f:f+sz])
u.reg_write(UC_PPC_REG_1,0x81780000)
for sym,reg in [('_SDA_BASE_',UC_PPC_REG_13),('_SDA2_BASE_',UC_PPC_REG_2)]:
 if sym in syms:u.reg_write(reg,syms[sym])
def call(name,*args):
 u.reg_write(UC_PPC_REG_1,0x81780000)
 for i,v in enumerate(args):u.reg_write(UC_PPC_REG_3+i,v)
 u.reg_write(UC_PPC_REG_LR,0x817ff000)
 u.emu_start(syms[name],0x817ff000,count=10000000)
 assert u.reg_read(UC_PPC_REG_PC)==0x817ff000, ('instruction budget exhausted',name)
 return u.reg_read(UC_PPC_REG_3)
checks=0
for x in [0,1,7,63]:
 for value in [0x12345678,0x80000001,0xaabbccdd,0xffffffff]:
  call('gs_mem_write_psmct32',64,64,x,3,value)
  assert call('gs_mem_read_psmct32',64,64,x,3)==value
  off=256+{0:72,1:76,7:124,63:5500}[x]
  assert bytes(u.mem_read(syms['g_gs_mem']+off,4))==struct.pack('<I',value)
  checks+=2
call('gs_mem_write_psmct16',64,64,0,0,0x801f)
call('gs_mem_write_psmct16',64,64,1,0,0xfc00)
assert call('gs_mem_read_psmct16',64,64,0,0)==0x801f
assert call('gs_mem_read_psmct16',64,64,1,0)==0xfc00
assert call('gs_mem_read_psmct32',64,64,0,0)==0x801f
assert call('gs_mem_read_psmct32',64,64,1,0)==0xfc00
call('gs_mem_write_psmct16',64,64,8,0,0xfc00)
assert call('gs_mem_read_psmct32',64,64,0,0)==0xfc00801f
assert bytes(u.mem_read(syms['g_gs_mem']+256,4))==b'\x1f\x80\x00\xfc'
checks+=4
for psm, value, width in [(0,0x12345678,4),(1,0xabcdef,3),(2,0x9876,2),(10,0xbeef,2)]:
 call('gs_mem_write_z',0,64,0,0,psm,value)
 assert call('gs_mem_read_z',0,64,0,0,psm)==value
 assert bytes(u.mem_read(syms['g_gs_mem']+6144,width))==value.to_bytes(width,'little')
 checks+=2
for psm,x,y,value,off,shift in [(0x13,1,0,0xab,4,0),(0x13,0,2,0xcd,33,0),(0x14,1,0,5,4,0),(0x14,0,2,14,32,4)]:
 call('gs_mem_write_index',0,128,x,y,psm,value)
 assert call('gs_mem_read_index',0,128,x,y,psm)==value
 raw=bytes(u.mem_read(syms['g_gs_mem']+off,1))[0]
 assert (raw>>shift)&(15 if psm==0x14 else 255)==value
 checks+=2
call('gs_mem_write_psmct32',0,64,0,0,0x12345678)
call('gs_mem_write_index',0,64,0,0,0x24,10)
call('gs_mem_write_index',0,64,0,0,0x2c,11)
assert call('gs_mem_read_psmct32',0,64,0,0)==0xba345678
checks+=1
print(f'PASS {checks} actual Wii ELF GS color/depth memory checks on big-endian PPC')
