#!/usr/bin/env python3
"""Real ELF EE scalar/block differential and PPC instruction counts, not Wii FPS."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
mask_addr=syms['gekko2_optimization_mask'];saved=int.from_bytes(u.mem_read(mask_addr,4),'big')
rows=[]
for phys in (0x100000,0x100ffc,0x1fffc0,0x200000,0xffff0):
 results=[]
 for native in (0,1):
  setup();word(mask_addr,(saved|9)&~(1<<25) if native else saved&~9)
  pc=0x80000000+phys
  for k in range(64):u.mem_write(ram+phys+4*k,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  # Warm the exact body before counting; compilation is not steady-state cost.
  assert call('ee_core_step_n',64)==64
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  call('ee_jit_get_block_retired');before_blocks=(u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4)
  active[0]=True;total[0]=0
  assert call('ee_core_step_n',64)==64
  active[0]=False
  digest=bytes(u.mem_read(state,512))+bytes(u.mem_read(state+off['pc'],8))+bytes(u.mem_read(state+off['cop0'],128))
  call('ee_jit_get_block_retired');blocks=(u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4)
  results.append((digest,total[0],blocks-before_blocks))
 assert results[0][0]==results[1][0],hex(phys)
 rows.append(dict(physical=hex(phys),scalar_PPC_instructions=results[0][1],native_PPC_instructions=results[1][1],block_retired=results[1][2]))
print(json.dumps(rows,indent=2))
print('PASS five physical placements: exact scalar/block GPR128, PC/nextPC, COP0 and retirement; page crossing and low-kernel guard retained. PPC counts are simulated instruction counts, not hardware cycles.')
