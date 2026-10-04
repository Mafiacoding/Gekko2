"""Actual EE CPU branch retirement and costs for inline dispatch."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_ee_frontend_r1276.py').read_text(),'measure_ee_frontend_r1276.py','exec'))
results={};cases=0
pc=0x80006000
for op in [2,3,4,5,6,7,0x14,0x15,0x16,0x17,0]:
 for variant in range(4):
  reset_frontend()
  aval=[0,1,0xffffffffffffffff,0x100000000][variant]
  bval=[0,0,0xffffffffffffffff,0][variant]
  rt=3 if op in [4,5,0x14,0x15] else 0
  iw=(op<<26)|(2<<21)|(rt<<16)|2
  taken={4:aval==bval,5:aval!=bval,6:bool(aval==0 or aval>>63),7:bool(aval!=0 and not aval>>63),0x14:aval==bval,0x15:aval!=bval,0x16:bool(aval==0 or aval>>63),0x17:bool(aval!=0 and not aval>>63)}.get(op,True)
  target=pc+12
  if op in [2,3]:target=pc+32;iw=(op<<26)|((target>>2)&0x3ffffff)
  if op==0:
   target=pc+32;aval=target;fn=8 if variant%2==0 else 9;iw=(2<<21)|(31<<11)|fn
  samples=[]
  for iteration in range(4):
   for n in range(16):guest(pc+n*4,0)
   guest(pc,iw);word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   word(state+off['cop0']+12*4,0);word(state+off['cop0']+9*4,100)
   u.mem_write(state+off['branch_pending'],b'\0');u.mem_write(state+off['halted'],b'\0');u.mem_write(state,bytes(512))
   u.mem_write(state+32,aval.to_bytes(8,'big')+bytes(8));u.mem_write(state+48,bval.to_bytes(8,'big')+bytes(8))
   total[0]=0;active[0]=True;assert call('ee_core_step_n',1)==1
   annul=op in [0x14,0x15,0x16,0x17] and not taken
   assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==pc+(8 if annul else 4),(op,variant,'firstPC')
   assert call('ee_core_step_n',1)==1;active[0]=False
   expected_pc=pc+12 if annul else target if taken else pc+8
   assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==expected_pc,(op,variant,'secondPC')
   assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==102
   assert bytes(u.mem_read(state,16))==bytes(16)
   if op==3 or (op==0 and variant%2):assert int.from_bytes(bytes(u.mem_read(state+31*16,8)),'big')==pc+8
   samples.append(total[0]);cases+=1
  results[hex(op)+'_'+str(variant)]={'cold':samples[0],'warm':samples[1:]}
print('PASS',cases,'actual Wii ELF short EE control transfers: full-width predicates, taken/not-taken, likely annul, delay slots, links, Count and GPR0')
print('EE_BRANCH_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Two actual EE retirements per sample, PPC counts; not Wii cycles/FPS.'}))
