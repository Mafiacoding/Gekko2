#!/usr/bin/env python3
"""Run real linked PPC wire/queue routines; not Wii networking or timing."""
import argparse,struct,subprocess,sys
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import UC_PPC_REG_0,UC_PPC_REG_1,UC_PPC_REG_3,UC_PPC_REG_LR,UC_PPC_REG_PC
p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--nm',required=True);a=p.parse_args()
e=a.elf.read_bytes();assert e[:6]==b'\x7fELF\x01\x02'
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x80000000,0x1800000)
offset=struct.unpack_from('>I',e,28)[0];stride,count=struct.unpack_from('>HH',e,42)
for i in range(count):
 kind,off,addr,_,size,mem,_,_=struct.unpack_from('>8I',e,offset+i*stride)
 if kind==1 and size:u.mem_write(addr,e[off:off+size])
syms={row.split()[2]:int(row.split()[0],16) for row in subprocess.check_output([a.nm,str(a.elf)],text=True).splitlines() if len(row.split())==3}
stop=0x81700000;stack=0x817f0000;data=0x81600000;packet=data+0x1000;q=data+0x2000;dest=data+0x4000;checks=0
# EABI small-data bases used by linked libc functions, not synthesized emitters.
for reg,name in ((2,'_SDA2_BASE_'),(13,'_SDA_BASE_')):
 if name in syms:u.reg_write(UC_PPC_REG_0+reg,syms[name])
def call(name,*args):
 global checks
 u.reg_write(UC_PPC_REG_1,stack);u.reg_write(UC_PPC_REG_LR,stop)
 for n,v in enumerate(args,3):u.reg_write(UC_PPC_REG_0+n,v)
 for n in range(14,32):u.reg_write(UC_PPC_REG_0+n,0x12340000+n)
 u.emu_start(syms[name],stop,count=1000000)
 assert u.reg_read(UC_PPC_REG_PC)==stop and u.reg_read(UC_PPC_REG_1)==stack
 assert all(u.reg_read(UC_PPC_REG_0+n)==0x12340000+n for n in range(14,32))
 checks+=1;return u.reg_read(UC_PPC_REG_3)
# Runtime sample: 23 Q fields, then C profile layout ticks/samples/overflows/entries/work.
values=[0]*23;values[0]=0x123456789abcdef0;values[4]=0x787dff;values[22]=1
scope=list(range(1,15))+[99,3]+list(range(20,34))+[(1<<40)+n for n in range(14)]
present=list(range(100,114))+[11,0]+[0]*28
u.mem_write(data,struct.pack('>'+'Q'*(23+88),*(values+scope+present)))
u.mem_write(packet,b'\xa5'*1018)
assert call('runtime_telemetry_encode',packet,1015,data)==0 and bytes(u.mem_read(packet,1))==b'\xa5'
assert call('runtime_telemetry_encode',packet,1016,data)==1016
wire=bytes(u.mem_read(packet,1016));assert bytes(u.mem_read(packet+1016,2))==b'\xa5\xa5'
import importlib.util
spec=importlib.util.spec_from_file_location('receiver',Path(__file__).resolve().parent/'windows-telemetry/receiver.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
r=m.decode(wire);assert r['boot']==values[0] and r['cpu']['ticks']==list(range(1,15)) and r['cpu']['samples']==99 and r['cpu']['work'][13]==(1<<40)+13
assert r['commit']!='unknown'
u.mem_write(q,bytes(4*1016+8));assert call('runtime_telemetry_pop',q,dest)==0
for cycle in range(3):
 for n in range(4):
  u.mem_write(packet,bytes([n])+wire[1:]);assert call('runtime_telemetry_push',q,packet)==1
 assert call('runtime_telemetry_push',q,packet)==0
 for n in range(4):assert call('runtime_telemetry_pop',q,dest)==1 and bytes(u.mem_read(dest,1))==bytes([n])
 assert call('runtime_telemetry_pop',q,dest)==0
print(f'PASS {checks} actual linked PPC encoding/bounds/queue/ABI checks; C/Python wire compatibility and source identity. No physical network test.')
