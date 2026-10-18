#!/usr/bin/env python3
"""Run actual delivered PPC worker/UI APIs with mocked SDK network/thread calls.
No physical Wii, Windows network or latency evidence is produced.
"""
import argparse,struct,subprocess
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_0,UC_PPC_REG_1,UC_PPC_REG_3,UC_PPC_REG_4,UC_PPC_REG_LR,UC_PPC_REG_PC,UC_PPC_REG_MSR
p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--nm',required=True);a=p.parse_args()
e=a.elf.read_bytes();u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x80000000,0x1800000)
offset=struct.unpack_from('>I',e,28)[0];stride,count=struct.unpack_from('>HH',e,42)
for i in range(count):
 kind,off,addr,_,size,mem,_,_=struct.unpack_from('>8I',e,offset+i*stride)
 if kind==1 and size:u.mem_write(addr,e[off:off+size])
syms={row.split()[2]:int(row.split()[0],16) for row in subprocess.check_output([a.nm,str(a.elf)],text=True).splitlines() if len(row.split())==3}
for reg,name in ((2,'_SDA2_BASE_'),(13,'_SDA_BASE_')):
 if name in syms:u.reg_write(UC_PPC_REG_0+reg,syms[name])
u.reg_write(UC_PPC_REG_MSR,1<<13)
stop=0x81700000;stack=0x817f0000;worker_stack=0x817d0000;data=0x81600000
names=('IRQ_Disable','IRQ_Restore','gettime','LWP_CreateThread','net_init','if_config','net_socket','net_fcntl','net_sendto','net_recvfrom','net_close','usleep')
hooks={syms[name]:name for name in names if name in syms}
entry=None;worker_context=None;mode='valid';last_packet=None;last_peer=None;pending=None;previous=None;thread_calls=0;net_calls=0;checks=0;fd=17

def ret(machine,value=0,low=None):
 machine.reg_write(UC_PPC_REG_3,value&0xffffffff)
 if low is not None:machine.reg_write(UC_PPC_REG_4,low)
 machine.reg_write(UC_PPC_REG_PC,machine.reg_read(UC_PPC_REG_LR))
def hook(machine,address,size,user):
 global entry,last_packet,last_peer,pending,previous,thread_calls,net_calls
 # libogc IRQ guards disable FP and rely on the Wii lazy-FPU exception.
 # Unicorn has no OS trap handler: model that platform detail explicitly.
 word=struct.unpack('>I',bytes(machine.mem_read(address,4)))[0]
 if (word&0xfc0007fe)==0x7c0002e6:
  reg=(word>>21)&31;tbr=((word>>16)&31)|(((word>>11)&31)<<5)
  if tbr in (268,269):
   machine.reg_write(UC_PPC_REG_0+reg,0x12340000 if tbr==268 else 0x12345678)
   machine.reg_write(UC_PPC_REG_PC,address+4);return
 if word&~(31<<21)==0x7c000124:
  rs=(word>>21)&31;machine.reg_write(UC_PPC_REG_MSR,machine.reg_read(UC_PPC_REG_0+rs)|(1<<13))
  machine.reg_write(UC_PPC_REG_PC,address+4);return
 name=hooks.get(address)
 if name is None:return
 reg=lambda n:machine.reg_read(UC_PPC_REG_0+n)
 if name=='gettime':ret(machine,0x12345678,0x12340000)
 elif name in ('IRQ_Disable','IRQ_Restore','net_close'):ret(machine)
 elif name=='net_fcntl':ret(machine,-10 if mode=='nonblock_fail' else 0)
 elif name=='LWP_CreateThread':entry=reg(4);thread_calls+=1;ret(machine,-12 if mode=='thread_fail' else 0)
 elif name=='net_init':net_calls+=1;ret(machine,-6 if mode=='net_fail' else 0)
 elif name=='if_config':
  if mode=='dhcp_fail':ret(machine,-7)
  else:
   for n,text in ((3,b'192.168.1.3\0'),(4,b'255.255.255.0\0'),(5,b'192.168.1.1\0')):machine.mem_write(reg(n),text)
   ret(machine)
 elif name=='net_socket':ret(machine,-8 if mode=='socket_fail' else fd)
 elif name=='net_sendto':
  last_packet=bytes(machine.mem_read(reg(4),reg(5)));last_peer=bytes(machine.mem_read(reg(7),reg(8)))
  assert len(last_packet)==1016 and last_packet[:4]==b'GK2T'
  ack=b'GK2A'+last_packet[128:144]
  if mode=='stale' and previous is not None:ack=previous
  if mode=='wrong_boot':ack=ack[:4]+bytes(8)+ack[12:]
  pending=ack;previous=b'GK2A'+last_packet[128:144]
  ret(machine,-9 if mode=='send_fail' else 1016)
 elif name=='net_recvfrom':
  if pending is None or mode=='no_ack':ret(machine,-11)
  else:
   ack=pending;pending=None;peer=last_peer
   if mode=='wrong_peer':peer=peer[:7]+bytes([peer[7]^1])+peer[8:]
   machine.mem_write(reg(4),ack);machine.mem_write(reg(7),peer);machine.mem_write(reg(8),struct.pack('>I',len(peer)));ret(machine,len(ack))
 elif name=='usleep':ret(machine);machine.emu_stop()
u.hook_add(UC_HOOK_CODE,hook)
def call(name,*args):
 global checks
 u.reg_write(UC_PPC_REG_1,stack);u.reg_write(UC_PPC_REG_LR,stop)
 for n,arg in enumerate(args,3):u.reg_write(UC_PPC_REG_0+n,arg)
 for n in range(14,32):u.reg_write(UC_PPC_REG_0+n,0x12340000+n)
 u.emu_start(syms[name],stop,count=2000000)
 assert u.reg_read(UC_PPC_REG_PC)==stop,name
 assert u.reg_read(UC_PPC_REG_1)==stack
 assert all(u.reg_read(UC_PPC_REG_0+n)==0x12340000+n for n in range(14,32)),name
 checks+=1;return u.reg_read(UC_PPC_REG_3)
def status():
 call('runtime_telemetry_get_status',data+256)
 return struct.unpack('>5iI3Q',bytes(u.mem_read(data+256,48)))
def apply(enabled=1,ip=0xc0a80102,expected=0):
 u.mem_write(data,struct.pack('>IH2xi',ip,26890,enabled));assert call('runtime_telemetry_apply',data)==(expected&0xffffffff)

def worker():
 global worker_context
 if worker_context is None:
  assert entry is not None;u.reg_write(UC_PPC_REG_1,worker_stack);u.reg_write(UC_PPC_REG_LR,stop);pc=entry
 else:u.context_restore(worker_context);pc=u.reg_read(UC_PPC_REG_PC)
 u.emu_start(pc,stop,count=3000000)
 assert u.reg_read(UC_PPC_REG_PC)!=stop,'worker unexpectedly returned'
 worker_context=u.context_save()

assert call('runtime_telemetry_state')==0 and call('runtime_telemetry_test')==0xffffffff and thread_calls==net_calls==0
mode='thread_fail';apply(expected=-1);s=status();assert s[0]==3 and s[2]==1 and s[3]==-12
mode='net_fail';apply();assert thread_calls==2 and status()[0]==1
worker();s=status();assert s[0]==3 and s[2]==2 and s[3]==-6
mode='valid';apply()
assert call('runtime_telemetry_test')==0;worker();s=status();assert s[0]==2 and s[4]==1 and s[5]==0xc0a80103 and s[6]==1
# Every new test clears old confirmation. Old sequence, wrong boot or peer cannot confirm it.
for setting in ('stale','wrong_boot','wrong_peer','no_ack'):
 mode=setting;assert call('runtime_telemetry_test')==0 and status()[4]==0;worker();assert status()[4]==0,setting
mode='valid';assert call('runtime_telemetry_test')==0;worker();assert status()[4]==1
# Same persistent worker retries network setup after a new apply generation.
mode='dhcp_fail';apply();worker();s=status();assert s[0]==3 and s[2]==3 and s[3]==-7
mode='socket_fail';apply();worker();s=status();assert s[0]==3 and s[2]==4 and s[3]==-8
mode='nonblock_fail';apply();worker();s=status();assert s[0]==3 and s[2]==5 and s[3]==-10
mode='valid';apply();assert call('runtime_telemetry_test')==0;worker();assert status()[4]==1 and thread_calls==2
mode='send_fail';assert call('runtime_telemetry_test')==0;worker();s=status();assert s[0]==2 and s[2]==7 and s[3]==-9 and s[4]==0 and s[8]>0
mode='valid';apply(ip=0xc0a80104);assert status()[4]==0 and call('runtime_telemetry_test')==0;worker();assert status()[4]==1 and last_peer[4:8]==bytes([192,168,1,4])
apply()
for _ in range(4):assert call('runtime_telemetry_test')==0
assert call('runtime_telemetry_test')==0xfffffffe
worker();assert status()[4]==1
apply(enabled=0);before=net_calls;worker();assert status()[0]==0 and call('runtime_telemetry_test')==0xffffffff and net_calls==before
assert thread_calls==2
print(f'PASS {checks} actual linked PPC API/ABI checks: default OFF, persistent worker, menu probe/ACK, stale/wrong-boot/peer rejection, thread/net/DHCP/socket/nonblock/send failures, bounded queue, retry/target update and OFF. SDK/IRQ/FPU/timebase modeled; no hardware network evidence.')
