#!/usr/bin/env python3
"""Synthetic sceCdGetError RPCs through the actual Wii PPC syscall path.
Platform services use the shared fixture; this does not execute a Wii/IOS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
# The unused checkpoint helpers are GC-stripped from the Wii image. Derive
# the live byte address from the actual leaf accessor, rejecting any changed
# instruction shape instead of guessing among duplicate local g_regs symbols.
ins=struct.unpack('>3I',u.mem_read(syms['iop_cdvd_peek_last_ncmd_error'],12))
assert ins[0]>>26==15 and ((ins[0]>>16)&31)==0
assert ins[1]>>26==34 and ((ins[1]>>21)&31)==3
assert ((ins[1]>>16)&31)==((ins[0]>>21)&31) and ins[2]==0x4e800020
error_byte=(((ins[0]&65535)<<16)+struct.unpack('>h',struct.pack('>H',ins[1]&65535))[0])&0xffffffff
def little(at,value):u.mem_write(ram+at,struct.pack('<I',value))
def reg(n,value):u.mem_write(state+n*16,struct.pack('>QQ',value,0))
checks=0
for error in range(0,256,17):
 for length in range(9):
  setup();word(syms['gekko2_optimization_mask'],1<<20) # actual EE service HLE
  # Same persistent drive register as MMIO, queried without clearing.
  u.mem_write(error_byte,bytes([error]))
  assert call('iop_cdvd_peek_last_ncmd_error')==error
  word(syms['g_rpc_bind_pending'],0);word(syms['g_rpc_bind_qcount'],0)
  word(syms['g_rpc_bind_qhead'],0);word(syms['g_rpc_bind_qtail'],0)
  for at,value in [(0x2000,0x80003000),(0x2008,0),(0x2010,0x80004000),(0x2018,64),
   (0x4000,64),(0x4008,0x8000000a),(0x401c,0x80005000),(0x4020,4),
   (0x4024,0),(0x4028,0x80006000),(0x402c,length)]:little(at,value)
  u.mem_write(ram+0x6000,b'\xa5'*16)
  call('sif_cmd_iop_track_bind_sid',0x80005000,0x80000593)
  little(base,12);reg(3,119);reg(4,0x80002000);reg(5,2)
  # This HLE syscall can consume its scheduler slot via an early service
  # return; validate the actual dispatch/response rather than an ALU count.
  call('ee_core_step_n',1)
  assert int.from_bytes(u.mem_read(state+off['pc'],4),'big')==base+4
  assert not int.from_bytes(u.mem_read(state+off['halted'],4),'big')
  got=bytes(u.mem_read(ram+0x6000,16))
  expected=(struct.pack('<I',error) if length>=4 else b'\xa5'*4)+b'\xa5'*12
  assert got==expected,(error,length,got.hex(),expected.hex())
  assert call('iop_cdvd_peek_last_ncmd_error')==error
  checks+=1
print('PASS',checks,'linked PPC GetError RPC transactions: actual error, bounded reply, preserved MMIO state')
