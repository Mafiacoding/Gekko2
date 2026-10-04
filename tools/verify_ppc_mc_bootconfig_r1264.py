"""Execute actual Wii reset-packet / MCSERV INIT code against synthetic ROMs.
This verifies the shared RPC HLE, not IRX execution or generated JIT blocks.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_clock_r1263.py').read_text(),'verify_ppc_clock_r1263.py','exec'))
with tempfile.TemporaryDirectory() as d:
    src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
    src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst uint32_t fields[]={offsetof(ee_state_t,bios),offsetof(ee_state_t,mcserv_module_version),offsetof(ee_state_t,mcman_module_version),sizeof(bios_image_t),offsetof(bios_image_t,data),offsetof(bios_image_t,size)};\n')
    subprocess.run([cc,'-O2','-msdata=none','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
    subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
    bio,sv,mv,bs,bd,bz=struct.unpack('>6I',raw.read_bytes())
rom=bytearray(816)
def put32(buf,pos,v):struct.pack_into('<I',buf,pos,v)
def entry(buf,pos,name,size):buf[pos:pos+len(name)]=name.encode();put32(buf,pos+12,size)
def irx(pos,name,version):
 rom[pos:pos+4]=b'\x7fELF';rom[pos+4]=rom[pos+5]=1;put32(rom,pos+32,52);rom[pos+46]=40;rom[pos+48]=1;put32(rom,pos+56,0x70000080);put32(rom,pos+68,100);put32(rom,pos+72,60);struct.pack_into('<H',rom,pos+124,version);rom[pos+126:pos+126+len(name)]=name.encode()
for pos,name,size in [(128,'RESET',128),(144,'ROMDIR',112),(160,'OSDCNF',96),(176,'XMCSERV',160),(192,'XMCMAN',160),(208,'LEGACY',160),(240,'RESET',0),(256,'ROMDIR',64),(272,'IOPBTCONF',32)]:entry(rom,pos,name,size)
text=b'@800\nXMCMAN\nXMCSERV\n';rom[304:304+len(text)]=text
irx(336,'mcserv',0x208);irx(496,'mcman_cex',0x209);irx(656,'mcman',0xfff)
bios=0x90100000;data=bios+0x1000;u.mem_map(bios,0x10000);u.mem_write(data,bytes(rom));u.mem_write(bios,bytes(bs));word(bios+bd,data);word(bios+bz,len(rom));word(state+bio,bios)
def versions():return tuple(int.from_bytes(bytes(u.mem_read(state+offset,2)),'big') for offset in (sv,mv))
def reboot(args='rom0:UDNL rom0:OSDCNF',length=None,size=104,mode=0,header=104):
 encoded=args.encode();length=len(encoded) if length is None else length
 u.mem_write(ram+0x1000,bytes(0x6000))
 for address,value in [(0x80001000,0x80002000),(0x80001004,0x10000),(0x80001008,size),(0x80002000,header),(0x80002008,0x80000003),(0x80002010,length),(0x80002014,mode),(0x80007000,12)]:guest(address,value)
 u.mem_write(ram+0x2018,encoded+b'\0')
 word(state+off['pc'],0x80007000);word(state+off['next_pc'],0x80007004);word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0')
 for reg,value in [(3,119),(4,0x80001000),(5,1)]:u.mem_write(state+off['gpr']+reg*16,struct.pack('>Q',value))
 call('ee_core_step_n',1)
reboot();assert versions()==(0x208,0x209),'reset lost selected provider versions or used unselected legacy module'
result,serv=rpc_open(0xfe,12)
assert result==0 and serv==0x208,'INIT success / MCSERV version mismatch'
assert int.from_bytes(bytes(u.mem_read(ram+0x6008,4)),'little')==0x209,'INIT mcman_cex version missing'
assert bytes(u.mem_read(ram+0x600c,4))==bytes(4),'INIT overran 12-byte reply'
reboot(header=64);assert versions()==(0x208,0x209),"non-reset payload invalidated provider metadata"
reboot(length=81);assert versions()==(0x208,0x209),'malformed oversized arg cleared providers'
reboot(size=40);assert versions()==(0x208,0x209),'truncated args read outside packet'
reboot(args='rom0:UDNL rom0:MISSING');assert versions()==(0,0),'missing config inherited stale provider'
reboot();reboot(mode=1);assert versions()==(0,0),'unsupported mode invented provider versions'
reboot();u.mem_write(data+284,struct.pack('<I',0xffffffff));reboot();assert versions()==(0,0),'nested ROM extent overflow accepted'
print('PASS 18 actual Wii ELF boot-config checks: selected versions, INIT ABI, bounds and provider invalidation')
