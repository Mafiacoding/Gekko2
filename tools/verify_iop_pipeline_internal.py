"""Independent linked-PPC load/exception oracle across bounded IOP blocks.
Synthetic RAM programs only; platform allocation/cache services are mocked.
"""
from pathlib import Path
prefix=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text().split('# Mixed instruction sequences')[0]
exec(compile(prefix,'verify_iop_blocks_internal.py','exec'))
def get(offset):return int.from_bytes(u.mem_read(ist+offset,4),'big')
checks=0
for native in [False,True]:
 def ticks(n):
  if native:assert call('iop_core_step_n',n)==n
  else:
   for _ in range(n):call('iop_core_step')
 for split in range(1,9):
  regs=[0]*32;regs[1]=0x80180000;regs[2]=9
  words=[0]*(split-1)+[(0x23<<26)|(1<<21)|(2<<16),(9<<26)|(2<<21)|(3<<16)|1,(9<<26)|(2<<21)|(4<<16)|2]
  setup(words,regs);u.mem_write(iram+0x180000,b'\x78\x56\x34\x12')
  ticks(split);assert get(8)==9
  ticks(1);assert get(12)==10 and get(8)==0x12345678
  ticks(1);assert get(16)==0x1234567a;checks+=1
 for take in [False,True]:
  for bev in [False,True]:
   for fault,code in [(12,8),(0xfc000000,10),((0x23<<26)|(1<<21)|(4<<16)|1,4),((8<<26)|(5<<21)|1,12)]:
    regs=[0]*32;regs[1]=0x80180000;regs[3]=0 if take else 1;regs[5]=0x7fffffff
    setup([(4<<26)|(3<<21)|7,fault],regs)
    word(ist+io['cop0']+12*4,0x400000 if bev else 0)
    ticks(1);ticks(1)
    assert get(io['pc'])==(0xbfc00180 if bev else 0x80000080)
    assert get(io['cop0']+14*4)==ibase
    assert get(io['cop0']+13*4)&0x8000007c==0x80000000|(code<<2)
    assert get(io['cop0']+6*4)==(ibase+32 if take else ibase+8)
    if code==4:assert get(io['cop0']+8*4)==0x80180001
    if not bev and code in [8,10]:
     ticks(1);assert get(io['pc'])==(ibase+32 if take else ibase+8)
    checks+=1
 # Consecutive merge loads forward pending data, but not the address base.
 regs=[0]*32;regs[2]=0x80180001
 setup([(0x22<<26)|(2<<21)|(2<<16)|3,(0x26<<26)|(2<<21)|(2<<16),0],regs)
 u.mem_write(iram+0x180000,b'\x78\x56\x34\x12\xef\xcd\xab\x90')
 ticks(1);ticks(1);assert get(8)==0x80180001
 ticks(1);assert get(8)==0xef123456;checks+=1
print('PASS',checks,'independent linked-PPC load visibility/block exits, merge aliasing, EPC/BD/TAR/BEV, alignment/overflow and HLE delay-slot resumption oracles')
