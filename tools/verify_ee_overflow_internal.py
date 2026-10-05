"""Independent signed arithmetic / overflow oracles in linked PPC CPU code."""
from pathlib import Path
import struct
fixture=Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text()
fixture=fixture.replace(" if 'ee_jit_reset_stats_for_test' in syms:call('ee_jit_reset_stats_for_test')\n",'').replace(' heap=0x81600000\n','')
exec(compile(fixture,'fixture','exec'))
values=[0,1,0x7fffffff,0xffffffff80000000,0xffffffffffffffff,0x7fffffffffffffff,0x8000000000000000,0x80000000]
mask=(1<<64)-1
signed=lambda x,b:x-(1<<b) if x>>(b-1) else x
specs=[(0,32,32,False),(0,34,32,True),(0,44,64,False),(0,46,64,True),(8,0,32,False),(24,0,64,False)]
def native_count():
 total=0
 for name in ['ee_jit_get_executed_count','ee_jit_get_block_retired']:
  high=call(name);total+=(high<<32)|u.reg_read(UC_PPC_REG_4)
 return total
cases=0
for op,fn,bits,subtract in specs:
 print('Testing',op,fn,flush=True)
 for dest in [0,1,2,3]:
  for delayed in [False,True]:
   for exl in [0,2]:
    for bev in [0,0x400000]:
     for left in values:
      for right0 in values:
       setup();word(state+off['cop0']+44,0xffffffff)
       sr=exl|bev;word(state+off['cop0']+48,sr);word(state+off['cop0']+52,0x300);word(state+off['cop0']+56,0x12345678)
       u.mem_write(state+16,struct.pack('>Q',left)+struct.pack('>Q',0x9988776655443322))
       u.mem_write(state+32,struct.pack('>Q',right0)+struct.pack('>Q',0x1234432112344321))
       expected=bytearray(u.mem_read(state,544));right=right0 if op==0 else ((right0&65535)-(65536 if right0&32768 else 0))&mask
       lhs=signed(left&((1<<bits)-1),bits);rhs=signed(right&((1<<bits)-1),bits)
       result=lhs-rhs if subtract else lhs+rhs;overflow=not(-(1<<(bits-1))<=result<(1<<(bits-1)))
       if dest and not overflow:struct.pack_into('>Q',expected,dest*16,result&mask)
       instr=(1<<21)|(2<<16)|(dest<<11)|fn if op==0 else ((op<<26)|(1<<21)|(dest<<16)|(right0&65535))
       words=[instr] if not delayed else [(4<<26)|2,instr]
       u.mem_write(ram+base,b''.join(struct.pack('<I',w) for w in words))
       if enabled:before=native_count()
       assert call('ee_core_step_n',len(words))==len(words)
       pc=int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big');cop=struct.unpack('>32I',bytes(u.mem_read(state+off['cop0'],128)))
       assert bytes(u.mem_read(state,544))==bytes(expected),(op,fn,dest,left,right,overflow,'registers')
       if overflow:
        assert pc==(0xbfc00380 if bev else 0x80000180),(op,fn,left,right,hex(pc))
        assert cop[12]==sr|2 and cop[13]==(0x300|0x30|(0x80000000 if delayed and not exl else 0))
        assert cop[14]==(0x12345678 if exl else base)
       else:
        assert pc==(base+12 if delayed else base+4)
        assert cop[12]==sr and cop[13]==0x300 and cop[14]==0x12345678
       if enabled:assert native_count()>before,(op,fn,'native execution missing')
       cases+=1
print('PASS',cases,'linked signed arithmetic, overflow, zero/aliased destinations, upper lanes, EXL/BEV and delay EPC/BD; JIT',enabled)
