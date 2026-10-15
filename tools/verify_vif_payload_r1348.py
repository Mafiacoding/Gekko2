"""Actual VIF handlers: V3 lookahead cannot consume the next command."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\n#include "core/hw/vif.h"\n#include "core/hw/vu.h"\nconst unsigned layout[]={offsetof(ee_state_t,vu0_mem),offsetof(vu1_state_t,mem),offsetof(vif_state_t,row),offsetof(vif_state_t,unpack_pending)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 m0,m1,row,pending=struct.unpack('>4I',raw.read_bytes())
vu1=call('vu1_get_state');source=ram+0x10000;checks=0
for unit in [0,1]:
 for vl in [0,1,2]:
  width=[4,2,1][vl]
  for num in [1,2,3,5]:
   values=list(range(1,num*3+1));payload=b''.join(v.to_bytes(width,'little') for v in values);payload+=bytes((-len(payload))%4)
   stream=struct.pack('<2I',0x01000101,((0x68+vl)<<24)|(num<<16)|0x4000)+payload+struct.pack('<5I',0x30000000,0x11223344,0x55667788,0xabcdef01,0x23456789)
   for split in [None,8,12,len(stream)-4]:
    call('vif_init');call('vu1_init');u.mem_write(state+m0,bytes(4096));u.mem_write(source,stream)
    fn='vif%d_process_tag_words'%unit
    if split is None:call(fn,unit,source,len(stream)//4)
    else:
     call(fn,unit,source,split//4);call(fn,unit,source+split,(len(stream)-split)//4)
    vif=call('vif%d_get_state'%unit);assert bytes(u.mem_read(vif+row,16))==struct.pack('>4I',0x11223344,0x55667788,0xabcdef01,0x23456789),(unit,vl,num,split)
    assert int.from_bytes(u.mem_read(vif+pending,4),'big')==0
    mem=state+m0 if unit==0 else vu1+m1
    for vector in range(num):
     assert struct.unpack('<3I',u.mem_read(mem+vector*16,12))==tuple(values[vector*3:vector*3+3]),(unit,vl,num,split,vector)
    checks+=1
print('PASS',checks,'VIF0/1 V3-32/16/8 payload boundaries, split transfers, XYZ data and following STROW')
