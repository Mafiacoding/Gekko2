"""Real linked PPC COP1 bit-transfer/sign operations in precise EE blocks.
Platform allocation/cache services are mocked; this is not a Wii FPS test.
"""
from pathlib import Path
try:
 exec(compile(Path(__file__).with_name('verify_ee_blocks_r1295.py').read_text(),'verify_ee_blocks_r1295.py','exec'))
except SystemExit as stop:
 if stop.code not in (None,0):raise
import hashlib
words=[(17<<26)|(4<<21)|(3<<16)|(4<<11),
 (17<<26)|(16<<21)|(4<<11)|(5<<6)|6,
 (17<<26)|(16<<21)|(5<<11)|(6<<6)|5,
 (17<<26)|(16<<21)|(6<<11)|(7<<6)|7,
 (17<<26)|(8<<16)|(7<<11),
 (17<<26)|(6<<21)|(9<<16)|(31<<11),
 (17<<26)|(2<<21)|(10<<16)|(31<<11),
 (9<<26)|(2<<21)|(2<<16)|1]
def program(bits):
 setup();u.mem_write(state+3*16,struct.pack('>QQ',bits,0));u.mem_write(state+9*16,struct.pack('>QQ',0x01800001,0))
 u.mem_write(state+1464,bytes(32*4));word(state+1592,0)
 for n,w in enumerate(words):u.mem_write(ram+base+4*n,struct.pack('<I',w))
hash=hashlib.sha256()
for bits in [0,0x80000000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc12345,0xffffffff]:
 program(bits);before=executed();assert call('ee_core_step_n',8)==8
 expected=bits|0x80000000
 assert bytes(u.mem_read(state+1464+4*4,16))==struct.pack('>4I',bits,bits,bits&0x7fffffff,expected)
 assert int.from_bytes(bytes(u.mem_read(state+8*16,8)),'big')==(expected|0xffffffff00000000)
 assert int.from_bytes(bytes(u.mem_read(state+10*16,8)),'big')==0x01800001
 assert executed()==before+8
 if enabled:
  high=call('ee_jit_get_block_retired');assert ((high<<32)|u.reg_read(UC_PPC_REG_4))==8
 hash.update(bytes(u.mem_read(state,544))+bytes(u.mem_read(state+1464,132)))
program(0xbf800000);assert call('ee_core_step_n',8)==8
samples=[]
for iteration in range(4):
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 total[0]=0;active[0]=True;assert call('ee_core_step_n',8)==8;active[0]=False;samples.append(total[0])
print('COP1_BLOCK_BENCH '+json.dumps({'samples':samples,'scope':'eight real retired COP1/ALU instructions; mocked platform services; no Wii FPS claim'}))
program(0xbf800000);word(state+off['cop0']+9*4,0);word(state+off['cop0']+11*4,3);word(state+off['cop0']+12*4,0x18001)
assert call('ee_core_step_n',3)==3
assert int.from_bytes(bytes(u.mem_read(state+1464+7*4,4)),'big')==0
assert int.from_bytes(bytes(u.mem_read(state+off['pc'],4)),'big')==0x80000200
assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+14*4,4)),'big')==base+12
print('COP1_BLOCK_SIGNATURE '+hash.hexdigest())
print('PASS 8 linked PPC COP1 mixed blocks, bit-preserving NaN/sign/zero transfers and exact interrupt boundary')
