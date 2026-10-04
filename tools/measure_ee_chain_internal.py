"""Warm returned-block continuation costs from actual linked PPC code."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
results={}
for kind in ['jump_then_alu','jump_cycle','plain_alu']:
 samples=[]
 for trial in range(3):
  extension_setup()
  if kind!='plain_alu':
   u.mem_write(ram+base,struct.pack('<I',(2<<26)|((base+64)>>2)))
   for n in range(8):u.mem_write(ram+base+64+4*n,struct.pack('<I',(9<<26)|(4<<21)|(4<<16)|1))
   if kind=='jump_cycle':u.mem_write(ram+base+64,struct.pack('<I',(2<<26)|(base>>2)))
  assert call('ee_core_step_n',8)==8
  word(state+off['pc'],base);word(state+off['next_pc'],base+4)
  before=executed();total[0]=0;active[0]=True
  assert call('ee_core_step_n',8)==8 and executed()==before+8
  active[0]=False;samples.append(total[0])
 results[kind]=samples
print('EE_CHAIN_BENCH '+json.dumps({'elf':Path(a.elf).name,'PPC':results,'scope':'Eight warm guest retirements; mocked platform, not real Wii timing/FPS.'}))
