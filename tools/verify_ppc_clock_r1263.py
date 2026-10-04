"""Run guest Count-write / display-clock regressions in both Wii ELFs.

Uses the shared emitted interpreter and parked hardware tick, not generated JIT.
The independent clock is initialized as a synthetic fixture through its ELF symbol;
checkpoint API functions are link-time discarded by the Wii frontend.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_intc_r1262.py').read_text(),'verify_ppc_intc_r1262.py','exec'))
call('ee_intc_init');call('gs_init')
word(state+off['cop0']+12*4,0)
word(state+off['pc'],0x80004000);word(state+off['next_pc'],0x80004004)
u.mem_write(state+off['branch_pending'],b'\0')
u.mem_write(state+off['gpr']+8*16,struct.pack('>Q',4921487))
u.mem_write(ram+0x4000,struct.pack('<2I',0x40884800,0))
def seed_clock(ticks):
 u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',ticks))
 if 'g_ee_display_phase' in syms:word(syms['g_ee_display_phase'],ticks%4921488)
seed_clock(100)
call('ee_core_step_n',1)
def clock():return int.from_bytes(bytes(u.mem_read(syms['g_ee_display_ticks'],8)),'big')
assert clock()==101,'guest Count write moved display phase'
assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==4921488,'MTC0 fixture did not execute'
assert not(int.from_bytes(bytes(u.mem_read(intc,4)),'big')&4),'Count write manufactured VBLANK'
gs=call('gs_get_state')
# Query CSR through the actual MMIO read helper, avoiding GS struct ABI guesses.
result=0x81741000
call('gs_mmio_read64',0x12001000,result)
assert not(int.from_bytes(bytes(u.mem_read(result,8)),'big')&8),'Count write manufactured GS VSYNC'
seed_clock(4921487)
word(state+off['cop0']+9*4,0xffffffff)
call('ee_core_park_tick',state)
assert clock()==4921488,'parked tick lost independent frame edge'
assert int.from_bytes(bytes(u.mem_read(state+off['cop0']+9*4,4)),'big')==0,'Count wrap failed'
assert int.from_bytes(bytes(u.mem_read(intc,4)),'big')&4,'parked frame missed VBLANK'
call('gs_mmio_read64',0x12001000,result)
assert int.from_bytes(bytes(u.mem_read(result,8)),'big')&8,'parked frame missed GS VSYNC'
call('ee_intc_init');seed_clock(410123)
call('ee_core_park_tick',state)
assert int.from_bytes(bytes(u.mem_read(intc,4)),'big')&8,'parked VBLANK-end missed'
seed_clock(0xffffffff)
call('ee_core_park_tick',state)
assert clock()==0x100000000,'clock failed to cross 32-bit width'
seed_clock(0x123456789abcdef0)
call('ee_core_park_tick',state)
assert clock()==0x123456789abcdef1,'PPC 64-bit tick arithmetic failed'
print('PASS 11 actual Wii ELF independent-clock checks: MTC0, parked edges, Count wrap, 64-bit width and carry')
# Real SifSetDma dispatch: both OPEN RPC ABIs and caller reply bounds.
call('sif_init');call('ee_intc_init')
def guest(address,v):u.mem_write(ram+(address&0xffff),struct.pack('<I',v))
def rpc_open(fno,recvsize):
 d,pkt,h,cd,reply=0x80001000,0x80002000,0x80003000,0x80005000,0x80006000
 u.mem_write(ram+0x1000,bytes(0x6000))
 for address,value in [(d,pkt),(d+4,0x10000),(d+8,128),(d+16,h),(d+20,0x11000),(d+24,64),
                       (h,64),(h+8,0x8000000a),(h+0x1c,cd),(h+0x20,fno),(h+0x28,reply),(h+0x2c,recvsize),
                       (reply,0xfeedface),(reply+4,0xcafef00d),(0x80007000,12)]:guest(address,value)
 call('sif_cmd_iop_track_bind_sid',cd,0x80000400)
 word(state+off['pc'],0x80007000);word(state+off['next_pc'],0x80007004)
 word(state+off['cop0']+12*4,0);u.mem_write(state+off['branch_pending'],b'\0')
 for reg,value in [(3,119),(4,d),(5,2)]:u.mem_write(state+off['gpr']+reg*16,struct.pack('>Q',value))
 call('ee_core_step_n',1)
 return int.from_bytes(bytes(u.mem_read(ram+0x6000,4)),'little'),int.from_bytes(bytes(u.mem_read(ram+0x6004,4)),'little')
for fno in [2,0x71]:
 result,guard=rpc_open(fno,4)
 assert result==0xfffffffc,'OPEN returned fabricated descriptor'
 assert guard==0xcafef00d,'OPEN reply overrun'
 result,guard=rpc_open(fno,0)
 assert result==0xfeedface,'zero-size OPEN reply overwritten'
result,guard=rpc_open(3,4)
assert result==0,'CLOSE was confused with OPEN'
print('PASS 7 actual Wii ELF MCSERV dispatch checks: modern/legacy OPEN, reply bounds and CLOSE')
