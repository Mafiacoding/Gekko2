#!/usr/bin/env python3
"""Independent oracle for emitted PPC VU integer memory lane masks (no BIOS)."""
import argparse, ctypes as C, struct, subprocess, tempfile
from unicorn import UC_HOOK_CODE
from pathlib import Path
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_PPC32, UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import *
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--source',default='source/core/recompiler/ppc_dynarec.c');args=parser.parse_args()
class Context(C.Structure):
 _fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
with tempfile.TemporaryDirectory() as directory:
 libpath=Path(directory)/'emit.so'
 subprocess.run(['gcc','-O2','-shared','-fPIC','-Iinclude','-Isource/core/recompiler',args.source,'-o',str(libpath)],cwd=root,check=True)
 lib=C.CDLL(str(libpath));lib.ppc_dynarec_translate_vu_lower.argtypes=[C.POINTER(Context),C.c_uint32]
 u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN)
 for address,size in [(0x10000,0x4000),(0x20000,0x4000),(0x30000,0x4000),(0x40000,0x4000)]:u.mem_map(address,size)
 counts=[0]
 def count(uc,address,size,user):counts[0]+=1
 u.hook_add(UC_HOOK_CODE,count)
 checks=0
 for relative in (0,1):
  for store in (0,1):
   for mask in range(16):
    for alias in (0,1):
     for mm in (4095,16383):
      for base in (0,mm//16):
       rs=1;rt=1 if alias else 2;vi=[0]*32;vi[rs]=base
       if not alias:vi[rt]=0xabcd
       value=vi[rt];offset=((base+(0 if relative else 1))*16)&mm
       mem=bytearray([0x73])*16384
       for lane in range(4):struct.pack_into('<I',mem,offset+lane*4,0xace00000+0x1111*(lane+1))
       expected=bytearray(mem);expectedvi=list(vi)
       for lane in range(4):
        if mask&(8>>lane):
         if store:struct.pack_into('<I',expected,offset+lane*4,value)
         else:expectedvi[rt]=struct.unpack_from('<I',mem,offset+lane*4)[0]&65535
       w=((0x800003fe|store) if relative else ((4+store)<<25)|1)|rt<<16|rs<<11|mask<<21
       ctx=Context();assert lib.ppc_dynarec_init(C.byref(ctx),2)==0
       assert lib.ppc_dynarec_translate_vu_lower(C.byref(ctx),w)==0
       lib.ppc_dynarec_finalize(C.byref(ctx));code=struct.pack('>'+str(ctx.used)+'I',*ctx.code[:ctx.used]);lib.ppc_dynarec_free(C.byref(ctx))
       u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x14000);u.mem_write(0x20000,struct.pack('>32I',*vi));u.mem_write(0x40000,bytes(mem))
       for k in range(32):u.reg_write(UC_PPC_REG_0+k,0xdead0000+k)
       for reg,val in [(1,0x32000),(3,0x21000),(4,0x20000),(5,0x40000),(6,mm),(7,0),(8,0x22000),(9,0x22004)]:u.reg_write(UC_PPC_REG_0+reg,val)
       u.reg_write(UC_PPC_REG_LR,0x11000);u.emu_start(0x10000,0x11000,count=1000)
       assert bytes(u.mem_read(0x40000,16384))==expected,(relative,store,mask,alias,mm,base,'memory')
       assert struct.unpack('>32I',u.mem_read(0x20000,128))==tuple(expectedvi),(relative,store,mask,alias,mm,base,'VI')
       assert u.reg_read(UC_PPC_REG_1)==0x32000
       for k in range(14,32):assert u.reg_read(UC_PPC_REG_0+k)==0xdead0000+k
       checks+=1
 print(f'PPC_INSTRUCTIONS total={counts[0]} synthetic-VU-only-not-Wii-FPS')
 print(f'PASS {checks} independent emitted PPC VU ILW/ISW/ILWR/ISWR cases, both memory sizes, masks/alias/wrap and ABI')
