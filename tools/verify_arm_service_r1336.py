"""Run actual ARM926 big-endian worker code. IOS syscalls are mocked;
not a physical-Wii speed or loader validation. No private game data."""
from pathlib import Path
import argparse,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
p=argparse.ArgumentParser();p.add_argument('elf');a=p.parse_args();data=Path(a.elf).read_bytes()
assert data[:7]==b'\x7fELF\x01\x02\x01' and struct.unpack_from('>H',data,18)[0]==40
phoff=struct.unpack_from('>I',data,28)[0];phsize,phnum=struct.unpack_from('>HH',data,42)
u=Uc(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN)
load_base=min(struct.unpack_from('>I',data,phoff+n*phsize+8)[0] for n in range(phnum) if struct.unpack_from('>I',data,phoff+n*phsize)[0]==1)
u.mem_map(load_base,0x20000);u.mem_map(0x10000000,0x20000)
for n in range(phnum):
 typ,offset,virt,phys,size,memsize,flags,align=struct.unpack_from('>8I',data,phoff+n*phsize)
 if typ==1:
  assert virt>=load_base and virt+memsize<=load_base+0x20000 and size<=memsize
  u.mem_write(virt,data[offset:offset+size])
syms={}
for row in subprocess.check_output(['nm','-n',a.elf],text=True).splitlines():
 cols=row.split()
 if len(cols)==3:syms[cols[2]]=int(cols[0],16)
stop=0x1001f000;stack=syms['ios_thread_stack'];acks=[];queue=[];registered=[]
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]
def words(addr,*values):u.mem_write(addr,struct.pack('>'+str(len(values))+'I',*(v&0xffffffff for v in values)))
def ios(uc,addr,size,unused):
 name=next((s for s in ('ios_queue_create','ios_queue_receive','ios_register','ios_ack','ios_invalidate','ios_flush') if syms[s]==addr),None)
 if not name:return
 args=[uc.reg_read(r) for r in regs]
 if name=='ios_queue_create':assert args[1]==16;uc.reg_write(UC_ARM_REG_R0,5)
 elif name=='ios_register':
  registered.append(bytes(uc.mem_read(args[0],12)));assert args[1]==5;uc.reg_write(UC_ARM_REG_R0,0)
 elif name=='ios_queue_receive':
  if not queue:uc.reg_write(UC_ARM_REG_PC,stop);return
  words(args[1],queue.pop(0));uc.reg_write(UC_ARM_REG_R0,0)
 elif name=='ios_ack':acks.append(tuple(args[:2]))
 uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,ios)
def call(name,*args):
 u.reg_write(UC_ARM_REG_SP,stack-256);u.reg_write(UC_ARM_REG_LR,stop)
 for reg,v in zip(regs,args):u.reg_write(reg,v)
 if len(args)>4:words(stack-256,*args[4:])
 u.emu_start(syms[name],stop,count=2000000)
 assert u.reg_read(UC_ARM_REG_PC)==stop,name
 return u.reg_read(UC_ARM_REG_R0)
header=0x10001000;source=0x10002000;out=0x10005000;vectors=0x10009000;msg=0x10000000
def submit(blocks=1,options=0):
 words(header,0x474b4131,1,2,3,blocks,options,0,0)
 stride=512 if options&1 else 1024
 words(vectors,header,32,source,blocks*384,out,blocks*stride)
 words(msg,7,0,0,1,2,1,vectors)
 return stride
raw=bytes(range(256))+bytes([128])*128
u.mem_write(source,raw*8)
expected=bytes(c for y in range(256) for c in ([max(0,min(255,(((max(0,y-16)*149)>>6)+1)>>1))]*3+[128]))
assert call('gekko2_arm_dispatch',0,0,0,0,0,out,32)==32
assert bytes(u.mem_read(out,16))==struct.pack('>4I',0x474b4131,1,1,8)
for blocks in (1,3,8):
 submit(blocks);assert call('gekko2_ios_handle',msg)==blocks*1024
 assert bytes(u.mem_read(out,blocks*1024))==expected*blocks
for opt in (1,3):
 submit(1,opt);assert call('gekko2_ios_handle',msg)==512
 packed=bytearray();matrix=((-4,0,-3,1),(2,-2,3,-1),(-3,1,-4,0),(3,-1,2,-2))
 for i in range(256):
  c=expected[i*4];d=matrix[(i//16)&3][i&3] if opt&2 else 0;c=max(0,min(255,c+d))>>3
  packed+=struct.pack('<H',c|(c<<5)|(c<<10))
 assert bytes(u.mem_read(out,512))==packed
def reject(change):
 submit();change();u.mem_write(out,bytes([0x55])*1024)
 assert call('gekko2_ios_handle',msg)==((-101)&0xffffffff)
 assert bytes(u.mem_read(out,1024))==bytes([0x55])*1024
reject(lambda:words(header+16,9));reject(lambda:words(header+20,4))
reject(lambda:words(vectors+4,31));reject(lambda:words(vectors+16,source))
reject(lambda:words(vectors+8,0xffffffe0));reject(lambda:words(msg+16,3))
# Actual entry point creates/registers the queue and handles open/caps/CSC.
path=0x1000a000;u.mem_write(path,b'/dev/gekko2\0')
words(msg,1,0,0,path,0,0)
capmsg=msg+64;words(capmsg,6,0,0,0,0,0,out+0x3000,32)
jobmsg=msg+128;submit(3);job=bytes(u.mem_read(msg,32));u.mem_write(jobmsg,job)
words(msg,1,0,0,path,0,0)
queue[:]=[msg,capmsg,jobmsg];call('_start')
assert registered==[b'/dev/gekko2\0'] and acks==[(msg,0),(capmsg,32),(jobmsg,3072)]
assert bytes(u.mem_read(out,3072))==expected*3
print('PASS actual ARM ELF: CSC 1/3/8-block RGB32, RGB16/dither, resource-manager queue/open/caps/ioctlv, rejected spans/counts/overlap')
print('IOS kernel/cache operations mocked; physical Wii loader, ARM throughput and acceleration unverified')
