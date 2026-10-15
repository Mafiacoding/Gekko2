"""Execute production GIF Q handling in the linked Wii PPC ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/hw/gif.h"\nconst unsigned layout[]={offsetof(gif_state_t,cur_q),offsetof(gif_state_t,gif_q_bits)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 qoff,temp=struct.unpack('>2I',raw.read_bytes())
gif=call('gif_get_state');packet=0x817d0000;checks=0
def expect(gs,gifq):
 global checks
 assert struct.unpack('>f',u.mem_read(gif+qoff,4))[0]==gs
 assert int.from_bytes(u.mem_read(gif+temp,8),'big')==gifq
 checks+=1
def words(values):return struct.pack('<'+str(len(values))+'I',*values)
call('gif_init');expect(1,0x3f800000)
# Tag STQ, RGBA, XYZ3; split directly after STQ.
data=words([1|(1<<15),3<<28,0xd12,0,0x3f400000,0x3f000000,0x40000000,0,128,128,128,128,0,0,0,0])
u.mem_write(packet,data);call('gif_process_quadwords',3,packet,2);expect(1,0x40000000)
call('gif_process_quadwords',3,packet+32,2);expect(2,0x40000000)
# A+D RGBAQ changes GS Q independently of the GIF latch.
data=words([1|(1<<15),1<<28,14,0,0x80808080,0x40400000,1,0])
u.mem_write(packet,data);call('gif_process_quadwords',3,packet,2);expect(3,0x3f800000)
# A new STQ-only tag changes temporary Q without writing GS Q.
data=words([1|(1<<15),1<<28,2,0,0,0,0x40800000,0]);u.mem_write(packet,data)
call('gif_process_quadwords',3,packet,2);expect(3,0x40800000)
u.mem_write(packet,words([1<<15,1<<28,1,0]));call('gif_process_quadwords',3,packet,1);expect(3,0x40800000)
u.mem_write(packet,words([1|(1<<15),1<<28,1,0,128,128,128,128]));call('gif_process_quadwords',3,packet,2);expect(1,0x3f800000)
print(json.dumps({'linked_PPC_GIF_Q_cases':checks,'all_passed':True,'physical_wii_verified':False}))
