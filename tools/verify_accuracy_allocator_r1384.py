#!/usr/bin/env python3
"""Execute the real host-generated PPC allocator output with Unicorn.
No instruction-count result is presented as Wii runtime performance.
"""
import ctypes as C,random,struct,subprocess,tempfile
from pathlib import Path
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
root=Path(__file__).resolve().parents[1]
wrapper=r'''
#include "core/recompiler/ppc_dynarec.c"
unsigned compile_allocated(uint32_t *output,const uint32_t *words,unsigned count,int resident) {
 ppc_codegen_ctx_t ctx;ppc_dynarec_init(&ctx,count+4u);
 ppc_residency plan={0};if(resident)residency_plan(&plan,words,count,1,0);
 for(unsigned n=0;n<count;n++) {
  size_t start=ctx.used_words;
  if(ppc_dynarec_translate_one(&ctx,words[n])){ppc_dynarec_free(&ctx);return 0;}
  ppc_allocate_emitted_body(&ctx,start,128u);
  if(resident)residency_body(&ctx,start,&plan);
 }
 memcpy(output,ctx.code,ctx.used_words*4);unsigned used=ctx.used_words;ppc_dynarec_free(&ctx);return used;
}
unsigned plan_lane_mask(const uint32_t *words,unsigned count,unsigned guest) {
 ppc_residency p;residency_plan(&p,words,count,1,0);unsigned mask=0;
 for(unsigned l=0;l<4;l++)if(p.slot[guest*4u+l]>=0)mask|=1u<<l;return mask;
}
'''
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'emitter.c';libfile=Path(d)/'emitter.so';src.write_text(wrapper)
 subprocess.run(['gcc','-shared','-fPIC','-O1','-Iinclude','-Isource',str(src),'source/core/recompiler/ppc_code_cache.c','source/core/recompiler/optimization.c','-o',str(libfile),'-lm'],cwd=root,check=True)
 lib=C.CDLL(str(libfile));lib.compile_allocated.argtypes=[C.POINTER(C.c_uint32),C.POINTER(C.c_uint32),C.c_uint,C.c_int]
 lib.plan_lane_mask.argtypes=[C.POINTER(C.c_uint32),C.c_uint,C.c_uint]
 mask=C.c_uint32.in_dll(lib,'gekko2_optimization_mask');rng=random.Random(1384);checks=0
 u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x10000,0x10000);u.mem_map(0x20000,0x10000)
 for kind in range(4):
  fn=9 if kind<2 else 41;sa=18+(kind&1)
  for dest in [0,1,2,3,31]:
   for trial in range(40):
    regs=[[rng.getrandbits(32) for _ in range(4)] for _ in range(32)];regs[0]=[0]*4
    expected=[r[:] for r in regs]
    instr=(28<<26)|(1<<21)|(2<<16)|(dest<<11)|(sa<<6)|fn
    words=(C.c_uint32*8)(*([instr]*8));code=(C.c_uint32*2048)()
    for enabled in range(3):
     mask.value=0xffffffff if enabled else 0
     used=lib.compile_allocated(code,words,8,enabled==2);assert used
     if enabled==2:
      assert lib.plan_lane_mask(words,8,1)==15 and lib.plan_lane_mask(words,8,2)==15
     oracle=[r[:] for r in regs]
     for _ in range(8):
      a=oracle[1][:];b=oracle[2][:]
      val=[(x&y if kind==0 else x^y if kind==1 else x|y if kind==2 else ~(x|y)&0xffffffff) for x,y in zip(a,b)]
      if dest:oracle[dest]=val
     machine=struct.pack('>'+str(used+1)+'I',*code[:used],0x4e800020)
     u.mem_write(0x10000,machine);u.ctl_remove_cache(0x10000,0x12000)
     raw=struct.pack('>128I',*(v for r in regs for v in r));u.mem_write(0x20000,raw)
     for n in range(32):u.reg_write(UC_PPC_REG_0+n,0xdead0000+n)
     u.reg_write(UC_PPC_REG_1,0x2f000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x12000)
     u.emu_start(0x10000,0x12000,count=5000)
     got=bytes(u.mem_read(0x20000,512));want=struct.pack('>128I',*(v for r in oracle for v in r))
     assert got==want,(kind,dest,trial,enabled,[(n,got[n:n+4].hex(),want[n:n+4].hex()) for n in range(0,512,4) if got[n:n+4]!=want[n:n+4]])
     checks+=1
 print('PASS',checks,'executed PPC full-128-bit allocation, residency, alias and VF/GPR zero oracles')
