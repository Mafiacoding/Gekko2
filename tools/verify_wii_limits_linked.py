#!/usr/bin/env python3
"""Execute the delivered benchmark's real PPC kernels, not Wii hardware timing."""
import argparse,random,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE,Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('elf',type=Path);p.add_argument('--nm',required=True);a=p.parse_args()
e=a.elf.read_bytes();assert e[:6]==b'\x7fELF\1\2'
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x80000000,0x1800000)
offset=struct.unpack_from('>I',e,28)[0];stride,count=struct.unpack_from('>HH',e,42)
for i in range(count):
    kind,off,addr,_,size,mem,_,_=struct.unpack_from('>8I',e,offset+i*stride)
    if kind==1 and size and addr>=0x80000000:u.mem_write(addr,e[off:off+size])
syms={row.split()[2]:int(row.split()[0],16) for row in subprocess.check_output([a.nm,str(a.elf)],text=True).splitlines() if len(row.split())==3}
stop=0x81700000;stack=0x817f0000;buffer=0x81000000;checks=0
u.reg_write(UC_PPC_REG_MSR,1<<13)
def call(name,*args,f=None):
    global checks
    u.reg_write(UC_PPC_REG_1,stack);u.reg_write(UC_PPC_REG_LR,stop)
    for n,v in enumerate(args,3):u.reg_write(UC_PPC_REG_0+n,v)
    if f is not None:u.reg_write(UC_PPC_REG_FPR1,struct.unpack('>Q',struct.pack('>d',f))[0])
    for n in range(14,32):u.reg_write(UC_PPC_REG_0+n,0x12340000+n)
    u.emu_start(syms[name],stop,count=16000000)
    assert u.reg_read(UC_PPC_REG_PC)==stop,name+' did not return'
    assert u.reg_read(UC_PPC_REG_1)==stack
    assert all(u.reg_read(UC_PPC_REG_0+n)==0x12340000+n for n in range(14,32))
    checks+=1;return u.reg_read(UC_PPC_REG_3)
rng=random.Random(20261018)
for n in [0,1,2,17,255,4096]+[rng.randrange(1000) for _ in range(18)]:
    seed=rng.getrandbits(32);v=seed
    for _ in range(n):
        v=(v*1664525+1013904223)&0xffffffff;v^=v>>13;v=(((v<<7)|(v>>25))+0x9e3779b9)&0xffffffff
    assert call('limits_integer',seed,n)==v
    def f32(x):return struct.unpack('>f',struct.pack('>f',x))[0]
    f=0.75
    for _ in range(n):f=f32(f32(f*f32(0.99991))+f32(0.00013));f=f32(f32(f*f32(0.99993))+f32(0.00011))
    call('limits_float',n,f=0.75)
    assert struct.unpack('>d',struct.pack('>Q',u.reg_read(UC_PPC_REG_FPR1)))[0]==f
for size in (8192,32768,262144,2097152):
    call('limits_fill',buffer,size);raw=bytes(u.mem_read(buffer,size))
    expected=b''.join(struct.pack('>I',(i*0x9e3779b9&0xffffffff)^0xa55a3cc3) for i in range(size//4));assert raw==expected
    assert call('limits_verify',buffer,size)==1
    u.mem_write(buffer+size-4,b'\0\0\0\0');assert call('limits_verify',buffer,size)==0
    call('limits_ring',buffer,size)
    assert call('limits_chase',buffer,0,size//32)==0
    assert call('limits_chase',buffer,0,size//32-1)!=0
# Real linked libc memcpy, both modeled memory banks and 32B/+4B alignment.
# These mappings verify copy semantics, not physical MEM1/MEM2 performance.
u.mem_map(0x90000000,0x800000)
for direction in (0,1):
    for offset in (0,4):
        for size in (32,8192,32768,262144,2097152):
            src=(0x90010000 if direction else 0x81010000)+offset
            dst=(0x81010000 if direction else 0x90010000)+offset
            guard=b'\xa5'*32;u.mem_write(dst-32,guard);u.mem_write(dst+size,guard)
            call('limits_fill',src,size);expected=bytes(u.mem_read(src,size))
            assert call('memcpy',dst,src,size)==dst
            assert bytes(u.mem_read(dst,size))==expected and call('limits_verify',dst,size)==1
            assert bytes(u.mem_read(dst-32,32))==guard and bytes(u.mem_read(dst+size,32))==guard
# Exercise the delivered polling fence with platform stubs: delayed completion,
# token wrap, and a permanently stalled GP. Only mftb is supplied a fake clock.
# This verifies control flow and timeout behavior, never GPU timing.
for initial,delay in ((12,0),(65535,2),(41,None)):
    clock=0;reads=0;pending=None;events=[]
    u.mem_write(syms['gx_failed'],bytes(4))
    funcs={syms[n]:n for n in ('GX_GetDrawSync','GX_SetDrawDone','GX_SetDrawSync','GX_Flush')}
    def hook(machine,address,size,user):
        global clock,reads,pending
        if address in funcs:
            name=funcs[address];events.append(name)
            if name=='GX_GetDrawSync':
                value=initial
                if pending is not None:
                    reads+=1;clock+=10000000
                    if delay is not None and reads>delay:value=pending
                machine.reg_write(UC_PPC_REG_3,value)
            elif name=='GX_SetDrawSync':pending=machine.reg_read(UC_PPC_REG_3)
            machine.reg_write(UC_PPC_REG_PC,machine.reg_read(UC_PPC_REG_LR));return
        insn=int.from_bytes(machine.mem_read(address,4),'big')
        if insn>>26==31 and ((insn>>1)&1023)==371:
            tbr=((insn>>16)&31)|((insn>>6)&0x3e0);reg=(insn>>21)&31
            assert tbr in (268,269)
            machine.reg_write(UC_PPC_REG_0+reg,(clock if tbr==268 else clock>>32)&0xffffffff)
            machine.reg_write(UC_PPC_REG_PC,address+4)
    h=u.hook_add(UC_HOOK_CODE,hook)
    result=call('fence');u.hook_del(h)
    assert pending==(initial+1)&65535
    assert events[:4]==['GX_GetDrawSync','GX_SetDrawDone','GX_SetDrawSync','GX_Flush']
    assert result==(0 if delay is None else 1)
    assert int.from_bytes(u.mem_read(syms['gx_failed'],4),'big')==(1 if delay is None else 0)
    assert reads<10
print(f'PASS {checks} actual-ELF PPC recurrence/ring/pattern/corruption/cross-bank-copy EABI and bounded GX-fence checks; no Wii boot or timing evidence')
