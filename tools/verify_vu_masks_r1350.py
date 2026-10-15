"""Masked vector loads/stores preserve untouched lanes, in native and C paths."""
from pathlib import Path
code=Path(__file__).with_name('verify_vu_pipeline_internal.py').read_text().split('\nchecks=0\n')[0]
exec(compile(code,'vu_mask_fixture','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=get(mask_addr);checks=0
for native in [0,1]:
 word(mask_addr,(initial&~4)|(native<<2))
 for operation in range(6):
  for mask in range(16):
   reset();u.mem_write(v['vf']+2*16,struct.pack('>4I',11,22,33,44));u.mem_write(v['mem']+64,struct.pack('<4I',101,202,303,404))
   word(v['vi']+4,4)
   if operation==0:lower=(mask<<21)|(2<<16)|(1<<11)
   elif operation==1:lower=(1<<25)|(mask<<21)|(1<<16)|(2<<11)
   else:lower=lo(13,2,1,mask,operation-2) if operation in [3,5] else lo(13,1,2,mask,operation-2)
   # LQD/SQD decrement before access; point at qword5 initially.
   if operation>=4:word(v['vi']+4,5)
   if native:
    assert call('vu_jit_try_lower',v['vf'],v['vi'],v['mem'],16383,lower,0,v['branch_delay'],v['branch_target'])==1
   else:step(NOP,lower)
   store=operation in [1,3,5]
   got=list(struct.unpack('<4I',u.mem_read(v['mem']+64,16))) if store else list(struct.unpack('>4I',u.mem_read(v['vf']+32,16)))
   old=[101,202,303,404] if store else [11,22,33,44];new=[11,22,33,44] if store else [101,202,303,404]
   expected=[new[l] if mask&(8>>l) else old[l] for l in range(4)]
   assert got==expected,(native,operation,mask,got,expected)
   assert get(v['vi']+4)==(5 if operation in [2,3] else 4)
   checks+=1
print('PASS',checks,'LQ/SQ/LQI/SQI/LQD/SQD cases, all16 masks, preserved lanes and base updates, native and interpreter')
for native in [0,1]:
 word(mask_addr,(initial&~4)|(native<<2))
 for bc in [1,3]:
  reset();word(v['vi']+4,5 if bc==3 else 4)
  for lane in range(4):
   word(v['vf']+32+lane*4,fbits(11+lane));word(v['vf']+48+lane*4,fbits(100));word(v['vf']+64+lane*4,fbits(200))
  step(up(2,3,4,15,40),lo(13,2,1,15,bc))
  assert list(struct.unpack('<4I',u.mem_read(v['mem']+64,16)))==[fbits(11+l) for l in range(4)]
  assert list(struct.unpack('>4I',u.mem_read(v['vf']+32,16)))==[fbits(300)]*4
print('PASS 4 dual-issue SQI/SQD hazards: store old Fs while upper publishes new Fs')
