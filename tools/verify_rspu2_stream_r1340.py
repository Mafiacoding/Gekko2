"""Actual linked PPC retail streaming transport. Disc reads/libc are mocked;
the protocol/state/RAM transfer code executes from the Wii ELF.
No copyrighted data, synthetic frames or physical performance claims.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0],'verify_ppc_gs_memory.py','exec'))
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_4,UC_PPC_REG_5
reads=[];invalidations=[];fault=[-1]
hooks={syms[n]:n for n in ['memcpy','memset','iop_cdvd_disc_read_sector','ee_jit_notify_physical_write']}
def host(uc,address,size,user):
    name=hooks.get(address)
    if not name:return
    x,y,z=[uc.reg_read(r) for r in [UC_PPC_REG_3,UC_PPC_REG_4,UC_PPC_REG_5]]
    ret=x
    if name=='memcpy':uc.mem_write(x,bytes(uc.mem_read(y,z)))
    elif name=='memset':uc.mem_write(x,bytes([y&255])*z)
    elif name=='iop_cdvd_disc_read_sector':
        reads.append(x);ret=0xffffffff if x==fault[0] else 0
        if not ret:uc.mem_write(y,bytes((x+k)&255 for k in range(2048)))
    else:invalidations.append((x,y))
    uc.reg_write(UC_PPC_REG_3,ret);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,host)
ram=0x91000000;u.mem_map(ram,0x4000)
words=0x81740000;answer=words+128;films=words+256;audio=words+272;saved=words+512
u.mem_write(films,struct.pack('>3I',8192,4096,2048));u.mem_write(audio,struct.pack('>3I',2048,1024,512))
def rpc(cmd,x=0,y=0,z=0,count=7):
    u.mem_write(words,struct.pack('>7I',0,x,y,z,4,0,0))
    assert call('rspu2_stream_rpc',cmd,words,count,ram,0x4000,answer)==1
    return int.from_bytes(u.mem_read(answer,4),'big',signed=True)
call('rspu2_stream_reset');call('rspu2_stream_configure',100,65536,films,audio)
assert rpc(0x2058,0x4820,0)==-1
assert rpc(0x2000,0x2237,0x20008000)==0 and rpc(0x2030)==0x44
call('rspu2_stream_resource_result',0);assert rpc(0x2030)==0x40
assert rpc(0x2058,0x4820,0)==0 and rpc(0x2030)==0
assert rpc(0x2059,0x20003c00,2)==-1 and not invalidations
assert rpc(0x2059,0x20000080,2)==0 and rpc(0x2030)==0x20
assert bytes(u.mem_read(ram+128,4096))==bytes((100+k//2048+k%2048)&255 for k in range(4096))
assert invalidations==[(128,2048),(2176,2048)]
saved=call('rspu2_stream_get_state');assert struct.unpack('>4I',u.mem_read(saved,16))==(1,1,100,65536)
fault[0]=103;assert rpc(0x2059,0x20002000,2)==-1 and rpc(0x2030)==4
assert reads==[100,101,102,103] and len(invalidations)==3
fault[0]=-1;assert rpc(0x2059,0x20002800,1)==0 and len(invalidations)==4
assert rpc(0x2059,0x20002800,1)==-1 and len(invalidations)==4
# Restore/validation is covered natively; those unused Wii UI symbols are
# garbage-collected from this production ELF.
assert rpc(0x205a,0)==0 and rpc(0x2030)==0x44
assert rpc(0x205a,1)==1
assert rpc(0x205e,47,1,2)==0 and rpc(0x205e,48,1,2)==-1
# One audio sector plus four video sectors per physical group. The fifth
# video sector comes from physical sector 106, never audio sector 105.
u.mem_write(films,struct.pack('>3I',8*2048,4096,2048))
call('rspu2_stream_configure',100,65536,films,audio)
assert rpc(0x2000,0x2237)==0
call('rspu2_stream_resource_result',0)
assert rpc(0x2058,0x4820,0,1)==0
before=len(reads);assert rpc(0x2059,0x20000080,5)==0
assert reads[before:]==list(range(100,107))
for k,sector in enumerate([101,102,103,104,106]):
    assert bytes(u.mem_read(ram+128+k*2048,2048))==bytes((sector+n)&255 for n in range(2048))
# Exact private-boot 7/128 geometry, including crossing an audio prefix.
u.mem_write(films,struct.pack('>3I',270*2048,4096,2048))
call('rspu2_stream_configure',100,1048576,films,audio)
assert rpc(0x2000,0x2237)==0
u.mem_write(words,struct.pack('>7I',0,0x4820,0,7,128,0,0))
assert call('rspu2_stream_rpc',0x2058,words,7,ram,0x4000,answer)==1
assert int.from_bytes(u.mem_read(answer,4),'big',signed=True)==0
for _ in range(18):assert rpc(0x2059,0x20000080,7)==0
assert rpc(0x2059,0x20000080,4)==0
for k,sector in enumerate([233,234,242,243]):
    assert bytes(u.mem_read(ram+128+k*2048,2048))==bytes((sector+n)&255 for n in range(2048))
print('PASS linked PPC R1340 real-byte streaming, status, errors, EOF, JIT invalidation and state inspection')
