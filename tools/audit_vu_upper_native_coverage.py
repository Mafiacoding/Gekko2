#!/usr/bin/env python3
"""R1324: prove native coverage for every VU upper opcode implemented by vu_exec_upper().

The audit compiles the real PPC translator and asks ppc_dynarec_translate_vu_upper()
to translate canonical encodings for every interpreter-backed Class A, Class B
broadcast, and SPECIAL upper form. It deliberately excludes the one reserved
SPECIAL slot (sub=0x0a, bc=3) that vu_exec_upper() does not implement either.
No generated PPC is executed on the host.
"""
from pathlib import Path
import ctypes as C
import subprocess

root=Path(__file__).resolve().parents[1]
out=root/'outputs'/'verification'
out.mkdir(parents=True,exist_ok=True)
so=out/'libvu_upper_coverage.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),
                str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(so)],check=True)

class Context(C.Structure):
    _fields_=[('code',C.POINTER(C.c_uint32)),('capacity_words',C.c_size_t),('used_words',C.c_size_t)]
lib=C.CDLL(str(so))
lib.ppc_dynarec_translate_vu_upper.argtypes=[C.POINTER(Context),C.c_uint32]
lib.ppc_dynarec_translate_vu_upper.restype=C.c_int

def accepted(w):
    code=(C.c_uint32*65536)(); c=Context(code,65536,0)
    return lib.ppc_dynarec_translate_vu_upper(C.byref(c),C.c_uint32(w))==0

def upper(funct, dest=0xf, ft=4, fs=3, fd=5):
    return (dest<<21)|(ft<<16)|(fs<<11)|(fd<<6)|(funct&0x3f)

def special(sub,bc,dest=0xf,ft=4,fs=3):
    # SPECIAL iff bits5:2 are 0xf; fd slot carries the 5-bit sub-op.
    return (dest<<21)|(ft<<16)|(fs<<11)|((sub&31)<<6)|0x3c|(bc&3)

rows=[]
def add(name,w): rows.append((name,accepted(w),w))

# Class B: ADD/SUB/MADD/MSUB/MAX/MINI/MUL broadcast x/y/z/w.
for name,sub in [('ADDbc',0),('SUBbc',1),('MADDbc',2),('MSUBbc',3),
                 ('MAXbc',4),('MINIbc',5),('MULbc',6)]:
    for bc,lane in enumerate('xyzw'):
        add(f'{name[:-2]}{lane}',upper((sub<<2)|bc))

# Class A / Q-I row implemented by vu_exec_upper().
class_a={
 'MULq':0x1c,'MAXi':0x1d,'MULi':0x1e,'MINIi':0x1f,
 'ADDq':0x20,'MADDq':0x21,'ADDi':0x22,'MADDi':0x23,
 'SUBq':0x24,'MSUBq':0x25,'SUBi':0x26,'MSUBi':0x27,
 'ADD':0x28,'MADD':0x29,'MUL':0x2a,'MAX':0x2b,
 'SUB':0x2c,'MSUB':0x2d,'OPMSUB':0x2e,'MINI':0x2f,
}
for name,fn in class_a.items(): add(name,upper(fn))

# SPECIAL accumulator/unary/conversion forms implemented in vu_exec_upper().
for sub,prefix in [(0,'ADDA'),(1,'SUBA'),(2,'MADDA'),(3,'MSUBA'),(6,'MULA')]:
    for bc,lane in enumerate('xyzw'): add(prefix+lane,special(sub,bc))
for bc,name in enumerate(['ITOF0','ITOF4','ITOF12','ITOF15']): add(name,special(4,bc))
for bc,name in enumerate(['FTOI0','FTOI4','FTOI12','FTOI15']): add(name,special(5,bc))
for bc,name in enumerate(['MULAq','ABS','MULAi','CLIPw']): add(name,special(7,bc))
for bc,name in enumerate(['ADDAq','MADDAq','ADDAi','MADDAi']): add(name,special(8,bc))
for bc,name in enumerate(['SUBAq','MSUBAq','SUBAi','MSUBAi']): add(name,special(9,bc))
for bc,name in enumerate(['ADDA','MADDA','MULA']): add(name,special(10,bc))
for bc,name in enumerate(['SUBA','MSUBA','OPMULA','NOP']): add(name,special(11,bc))

gaps=[r for r in rows if not r[1]]
print('R1324 VU upper native coverage')
print('  interpreter-backed canonical forms:',len(rows))
print('  native:',len(rows)-len(gaps))
print('  gaps:',len(gaps))
for name,ok,w in rows:
    print(f'  {"NATIVE" if ok else "FALLBACK":8} {name:12} {w:#010x}')
if gaps:
    raise SystemExit('native upper gaps remain: '+', '.join(r[0] for r in gaps))
