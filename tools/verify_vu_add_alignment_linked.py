#!/usr/bin/env python3
"""Exact synthetic VU ADD/SUB vectors through the linked Wii PPC VU1 runner.
Platform allocation/cache services are modeled by the shared fixture.
No physical Wii timing, GS output or game-compatibility claim.
"""
from pathlib import Path
source=Path(__file__).with_name('verify_beta12_22_linked.py').read_text()
exec(compile(source.split('_,vo=')[0],'fixture','exec'))
_,vo=layout('core/hw/vu.h','vu1_state_t',[
 'vf','vi','acc','micro','running','instructions_executed','unimplemented_opcodes_seen'])
vu=call('vu1_get_state')
# encoding, accumulator destination, subtraction
ops=[(232,0,0),(192,0,0),(224,0,0),(226,0,0),
     (236,0,1),(196,0,1),(228,0,1),(230,0,1),
     (700,1,0),(60,1,0),(572,1,0),(574,1,0),
     (764,1,1),(124,1,1),(636,1,1),(638,1,1)]
vectors=[(0x3f800000,0xb37fffff,0x3f800000,0x3f800000),
 (0xb37fffff,0x3f800000,0x3f800000,0xbf800000),
 (0xbf800000,0x337fffff,0xbf800000,0xbf800000),
 (0x3f800000,0xb3ffffff,0x3f7ffffe,0x3f800001)]
checks=runs=0
for mask_options in (0,(1<<2)|(1<<20)):
 word(syms['gekko2_optimization_mask'],mask_options)
 for encoding,accumulator,subtract in ops:
  for lanes in (1,5,10,15):
   for ab,bb,add,sub in vectors:
    call('vu1_init')
    for l in range(4):
     for reg,bits in ((1,ab),(2,bb),(3,0x3e800000)):
      word(vu+vo['vf']+reg*16+l*4,bits)
     word(vu+vo['acc']+l*4,0x3e800000)
    for reg in (21,22):word(vu+vo['vi']+reg*4,bb)
    upper=encoding|(lanes<<21)|(2<<16)|(1<<11)
    # Four NOPs mature arithmetic flags, then E and its delay pair.
    pairs=[(0x8000033c,upper)]+[(0x8000033c,0x2ff)]*4+[(0x8000033c,0x400002ff),(0x8000033c,0x2ff)]
    u.mem_write(vu+vo['micro'],b''.join(struct.pack('<II',*p) for p in pairs))
    call('vu1_exec_micro',0)
    assert u.mem_read(vu+vo['running'],1)==b'\0'
    assert int.from_bytes(u.mem_read(vu+vo['instructions_executed'],8),'big')==7
    assert int.from_bytes(u.mem_read(vu+vo['unimplemented_opcodes_seen'],8),'big')==0
    expected=sub if subtract else add
    dest=vu+vo['acc'] if accumulator else vu+vo['vf']+3*16
    for l in range(4):
     want=expected if lanes&(8>>l) else 0x3e800000
     got=int.from_bytes(u.mem_read(dest+l*4,4),'big')
     assert got==want,(mask_options,encoding,lanes,hex(ab),hex(bb),l,hex(got),hex(want))
     checks+=1
    mac=int.from_bytes(u.mem_read(vu+vo['vi']+17*4,4),'big')
    status=int.from_bytes(u.mem_read(vu+vo['vi']+16*4,4),'big')
    assert mac==(lanes<<4 if expected>>31 else 0),(hex(mac),hex(expected))
    assert status==(0x82 if expected>>31 else 0),hex(status)
    checks+=2;runs+=1
print('PASS',runs,'linked PPC VU1 programs and',checks,'lane/flag checks, scalar and block-option runners')
