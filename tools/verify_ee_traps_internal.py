"""Independent architectural TR/EPC/BD oracles in actual linked PPC code."""
from pathlib import Path
import struct
fixture=Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text()
fixture=fixture.replace(" if 'ee_jit_reset_stats_for_test' in syms:call('ee_jit_reset_stats_for_test')\n",'').replace(' heap=0x81600000\n','')
exec(compile(fixture,'fixture','exec'))
values=[0,1,0x7fffffff,0x80000000,0xffffffffffffffff,0x7fffffffffffffff,0x8000000000000000,0xffffffff00000000]
mask=(1<<64)-1
signed=lambda x:x-(1<<64) if x>>63 else x
specs=[(0,x) for x in [48,49,50,51,52,54]]+[(1,x) for x in [8,9,10,11,12,14]]
def count_native():
 total=0
 for name in ['ee_jit_get_executed_count','ee_jit_get_block_retired']:
  high=call(name);total+=(high<<32)|u.reg_read(UC_PPC_REG_4)
 return total
cases=0
for op,kind in specs:
 print('Testing',op,kind,flush=True)
 for delayed in [False,True]:
  for exl in [0,2]:
   for bev in [0,0x400000]:
    for n,left in enumerate(values):
     for right0 in values:
      setup();right=right0 if op==0 else ((right0&65535)-(65536 if right0&32768 else 0))&mask
      family=kind if op==0 else kind+40
      take={48:signed(left)>=signed(right),49:left>=right,50:signed(left)<signed(right),51:left<right,52:left==right,54:left!=right}[family]
      word(state+off['cop0']+44,0xffffffff)
      sr=exl|bev;word(state+off['cop0']+48,sr);word(state+off['cop0']+52,0x300);word(state+off['cop0']+56,0x12345678)
      u.mem_write(state+16,struct.pack('>Q',left)+struct.pack('>Q',0x9988776655443322))
      u.mem_write(state+32,struct.pack('>Q',right0)+struct.pack('>Q',0x1234432112344321))
      beforegpr=bytes(u.mem_read(state,544))
      instr=(1<<21)|(2<<16)|kind if op==0 else ((1<<26)|(1<<21)|(kind<<16)|(right0&65535))
      words=[instr] if not delayed else [(4<<26)|2,instr]
      u.mem_write(ram+base,b''.join(struct.pack('<I',w) for w in words))
      if enabled:before=count_native()
      assert call('ee_core_step_n',len(words))==len(words)
      pc=int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big');cop=struct.unpack('>32I',bytes(u.mem_read(state+off['cop0'],128)))
      assert bytes(u.mem_read(state,544))==beforegpr,(op,kind,delayed,'gpr')
      if take:
       assert pc==(0xbfc00380 if bev else 0x80000180),(op,kind,left,right,hex(pc))
       assert cop[12]==sr|2 and cop[13]==(0x300|0x34|(0x80000000 if delayed and not exl else 0)),(op,kind,delayed,exl,bev,left,right,hex(cop[12]),hex(cop[13]))
       assert cop[14]==(0x12345678 if exl else base)
      else:
       assert pc==(base+12 if delayed else base+4),(op,kind,left,right,'false pc',hex(pc))
       assert cop[12]==sr and cop[13]==0x300 and cop[14]==0x12345678
      if enabled:
       after=count_native()
       assert after>before,(op,kind,'native execution missing')
      cases+=1
print('PASS',cases,'linked EE register/immediate traps, EXL/BEV, delay slots, EPC/BD, unchanged 128-bit registers; JIT',enabled)
