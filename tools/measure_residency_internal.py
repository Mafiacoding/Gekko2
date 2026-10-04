"""Warm eight-retirement PPC instruction counts, not Wii cycles/FPS."""
from pathlib import Path
prefix=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction sequences')[0]
exec(compile(prefix,'verify_iop_blocks_internal.py','exec'))
# Derive exact old/new object sizes from each ELF; appended fields must not
# make the old-baseline fixture clear bytes belonging to another object.
nmtext=subprocess.check_output([a.nm,'-S',a.elf],text=True)
import re
sizes={m[2]:int(m[1],16) for m in re.findall(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\w\s+(\S+)$',nmtext,re.M)}
size=sizes['g_state'];isize=sizes['g_iop']
results={}
for cpu in ['EE','IOP']:
 for name,iw in [('ADDIU_self',(9<<26)|(2<<21)|(2<<16)|1),('OR_sources',(1<<21)|(2<<16)|(3<<11)|0x25),('XOR_accumulator',(2<<21)|(1<<16)|(2<<11)|0x26),('LW_base',(0x23<<26)|(1<<21)|(3<<16))]:
  samples=[]
  for trial in range(3):
   if cpu=='EE':
    extension_setup();u.mem_write(state+16,struct.pack('>QQ',0x80300000,0));u.mem_write(state+32,struct.pack('>QQ',7,0));u.mem_write(ram+0x300000,b'\x78\x56\x34\x12')
    for n in range(8):u.mem_write(ram+base+n*4,struct.pack('<I',iw))
    assert call('ee_core_step_n',8)==8;word(state+off['pc'],base);word(state+off['next_pc'],base+4)
    total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False
   else:
    regs=[0]*32;regs[1]=0x80180000;regs[2]=7;setup([iw]*8,regs)
    assert call('iop_core_step_n',8)==8;word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
    total[0]=0;active[0]=True;assert call('iop_core_step_n',8)==8;active[0]=False
   samples.append(total[0])
  assert len(set(samples))==1,(cpu,name,samples)
  results[cpu+'_'+name]=samples[0]
print(json.dumps({'elf':Path(a.elf).name,'retirements':8,'warm_PPC_instructions':results,'scope':'Mocked platform, synthetic programs; includes full CPU retirement. Not Wii cycles, BIOS timing or FPS.'}))
