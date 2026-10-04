"""Full IOP control retirement and costs in actual Wii ELFs."""
from pathlib import Path
exec(compile(Path(__file__).with_name('measure_iop_frontend_r1281.py').read_text(),'measure_iop_frontend_r1281.py','exec'))
results={};checks=0;pc=0x80006000
for op in [2,3,4,5,6,7,0]:
 for variant in range(4):
  aa=[0,1,0xffffffff,0x80000000][variant];bb=[0,0,0xffffffff,0][variant]
  rt=3 if op in [4,5] else 0;iw=(op<<26)|(2<<21)|(rt<<16)|2;target=pc+12
  taken={4:aa==bb,5:aa!=bb,6:bool(aa==0 or aa>>31),7:bool(aa!=0 and not aa>>31)}.get(op,True)
  if op in [2,3]:target=pc+32;iw=(op<<26)|((target>>2)&0x3ffffff)
  if op==0:target=pc+32;aa=target;iw=(2<<21)|(31<<11)|(8 if variant%2==0 else 9)
  samples=[]
  for iteration in range(4):
   call('iop_intc_init');call('iop_timers_init');u.mem_write(ios,bytes(128))
   word(ios+8,aa);word(ios+12,bb);word(ios+ioff['pc'],pc);word(ios+ioff['next_pc'],pc+4)
   word(ios+ioff['cop0']+12*4,0);u.mem_write(ios+ioff['halted'],b'\0');u.mem_write(ios+ioff['idle'],b'\0')
   for n in range(16):u.mem_write(io_ram+0x6000+4*n,bytes(4))
   u.mem_write(io_ram+0x6000,struct.pack('<I',iw))
   total[0]=0;active[0]=True;call('iop_core_step')
   assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==pc+4
   assert int.from_bytes(bytes(u.mem_read(ios+ioff['next_pc'],4)),'big')==(target if taken else pc+8)
   call('iop_core_step');active[0]=False
   assert int.from_bytes(bytes(u.mem_read(ios+ioff['pc'],4)),'big')==(target if taken else pc+8)
   if op==3 or (op==0 and variant%2):assert int.from_bytes(bytes(u.mem_read(ios+31*4,4)),'big')==pc+8
   assert bytes(u.mem_read(ios,4))==bytes(4) and bytes(u.mem_read(ios+ioff['halted'],1))==b'\0'
   samples.append(total[0]);checks+=1
  results[hex(op)+'_'+str(variant)]={'cold':samples[0],'warm':samples[1:]}
for rs in [0,4]:
 for rd in range(32):
  for rt in [0,2]:
   call('iop_intc_init');call('iop_timers_init');u.mem_write(ios,bytes(128))
   # Mask interrupts; keep this single-instruction COP0 test independent of unmasking.
   word(ios+ioff['cop0']+12*4,0)
   if rd!=12:word(ios+ioff['cop0']+rd*4,0x12345678)
   word(ios+8,0x10000 if rd==12 else 0xaabbccdd)
   word(ios+ioff['pc'],pc);word(ios+ioff['next_pc'],pc+4)
   u.mem_write(ios+ioff['halted'],b'\0');u.mem_write(ios+ioff['idle'],b'\0')
   iw=(0x10<<26)|(rs<<21)|(rt<<16)|(rd<<11)
   u.mem_write(io_ram+0x6000,struct.pack('<I',iw));call('iop_core_step')
   if rs==0 and rt:assert int.from_bytes(bytes(u.mem_read(ios+rt*4,4)),'big')==(0 if rd==12 else 0x12345678)
   if rs==4:
    expected=0 if not rt else 0x10000 if rd==12 else 0xaabbccdd
    # Retirement refreshes hardware Cause.IP2 even with CPU IRQs masked.
    if rd==13:expected &= ~0x400
    actual=int.from_bytes(bytes(u.mem_read(ios+ioff['cop0']+rd*4,4)),'big')
    assert actual==expected,(rs,rd,rt,hex(actual),hex(expected))
   assert bytes(u.mem_read(ios,4))==bytes(4);checks+=1
print('PASS',checks,'actual Wii ELF IOP control cases: signed predicates, branch/delay/links, all32 COP0 registers and rt0')
print('IOP_CONTROL_MEASURE',json.dumps({'elf':Path(a.elf).name,'results':results,'scope':'Two genuine IOP steps including retirement, PPC instruction counts not Wii cycles/FPS.'}))
