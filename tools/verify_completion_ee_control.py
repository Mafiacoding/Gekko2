#!/usr/bin/env python3
"""Independent executed PPC oracle for C.F.S and BC2, without a guest image."""
from pathlib import Path
import ctypes as C,random,struct,subprocess,tempfile
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
root=Path(__file__).resolve().parents[1]
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
with tempfile.TemporaryDirectory(prefix='gekko2-ee-control-') as d:
 so=Path(d)/'emitter.so';subprocess.run(['gcc','-shared','-fPIC','-O2','-Iinclude','source/core/recompiler/ppc_dynarec.c','-o',str(so)],cwd=root,check=True)
 lib=C.CDLL(str(so));lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
 def compile(word):
  c=Context();assert not lib.ppc_dynarec_init(C.byref(c),4)
  result=lib.ppc_dynarec_translate_one(C.byref(c),word)
  data=struct.pack('>'+str(c.used+1)+'I',*c.code[:c.used],0x4e800020)
  lib.ppc_dynarec_free(C.byref(c));return result,data
 u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x10000,0x30000)
 rng=random.Random(20261018);checks=0
 def word(data,off,v):struct.pack_into('>I',data,off,v&0xffffffff)
 def run(iw,state,expected):
  global checks
  r,code=compile(iw);assert not r
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x18000);u.mem_write(0x20000,bytes(state))
  canaries=[0xadef0000+n for n in range(14,32)]
  for n,v in enumerate(canaries,14):u.reg_write(UC_PPC_REG_0+n,v)
  u.reg_write(UC_PPC_REG_1,0x3f000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x18000)
  u.emu_start(0x10000,0x18000,count=1000)
  assert bytes(u.mem_read(0x20000,len(state)))==bytes(expected)
  assert [u.reg_read(UC_PPC_REG_0+n) for n in range(14,32)]==canaries
  assert u.reg_read(UC_PPC_REG_1)==0x3f000;checks+=1
 for _ in range(256):
  state=bytearray(rng.randbytes(4096));expected=state[:]
  value=struct.unpack_from('>I',state,1592)[0];word(expected,1592,value&~0x00800000)
  run((17<<26)|(16<<21)|(2<<16)|(3<<11)|0x30,state,expected)
 for pc in (0x200000,0x7ffffffc,0xfffffffc):
  for imm in (-32768,-17,0,32767):
   for condition in (0,1):
    for rt in range(4):
     state=bytearray(rng.randbytes(4096));word(state,544,pc+4);word(state,548,pc+8);state[684]=0
     word(state,1456,pc);word(state,1600+29*4,0xcafe00a5|(condition<<8));expected=state[:]
     taken=condition==(rt&1);annul=not taken and rt&2
     if taken:word(expected,548,pc+4+imm*4)
     if annul:word(expected,544,pc+8);word(expected,548,pc+12)
     else:expected[684]=1
     run((18<<26)|(8<<21)|(rt<<16)|(imm&65535),state,expected)
 for rt in range(4,32):assert compile((18<<26)|(8<<21)|(rt<<16))[0]!=0
 print(f'PASS {checks} executed PPC C.F.S/BC2 full-state, address-wrap, sign-offset and EABI oracles; 28 reserved BC2 selectors declined')
