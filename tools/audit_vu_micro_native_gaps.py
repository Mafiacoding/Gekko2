#!/usr/bin/env python3
"""R1323 discovery: enumerate interpreter-backed VU micro lower ops that still fall back.

This compiles the real PPC lower-word translator and probes canonical encodings
for every lower-word family currently implemented by source/hw/vu.c. It does
not execute generated PPC. The result is a gap inventory, not a pass/fail gate:
rejected entries are candidates for later native implementation or deliberate
scheduler/side-effect boundaries.
"""
from pathlib import Path
import ctypes as C
import subprocess

root=Path(__file__).resolve().parents[1]
out=root/'outputs'/'verification'
out.mkdir(parents=True,exist_ok=True)
so=out/'libvu_micro_gap_audit.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),
                str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(so)],check=True)

class Context(C.Structure):
    _fields_=[('code',C.POINTER(C.c_uint32)),('capacity_words',C.c_size_t),('used_words',C.c_size_t)]
lib=C.CDLL(str(so))
lib.ppc_dynarec_translate_vu_lower.argtypes=[C.POINTER(Context),C.c_uint32]
lib.ppc_dynarec_translate_vu_lower.restype=C.c_int

def accepted(w):
    code=(C.c_uint32*32768)(); c=Context(code,32768,0)
    return lib.ppc_dynarec_translate_vu_lower(C.byref(c),C.c_uint32(w))==0

def direct(op,rs=3,rt=4,rd=5,dest=0xf,imm=1):
    return (op<<25)|(dest<<21)|(rt<<16)|(rs<<11)|(rd<<6)|(imm&0x7ff)

def special(rd,bc,rs=3,rt=4,dest=0xf,fnbase=0x3c):
    return (0x40<<25)|(dest<<21)|(rt<<16)|(rs<<11)|(rd<<6)|(fnbase|bc)

rows=[]
def add(name,w,kind='ordinary'):
    rows.append((name,kind,accepted(w),w))

for name,op in [
 ('LQ',0x00),('SQ',0x01),('ILW',0x04),('ISW',0x05),
 ('IADDIU',0x08),('ISUBIU',0x09),
 ('FCEQ',0x10),('FCSET',0x11),('FCAND',0x12),('FCOR',0x13),
 ('FSEQ',0x14),('FSSET',0x15),('FSAND',0x16),('FSOR',0x17),
 ('FMEQ',0x18),('FMAND',0x1a),('FMOR',0x1b),('FCGET',0x1c),
 ('B',0x20),('BAL',0x21),('JR',0x24),('JALR',0x25),
 ('IBEQ',0x28),('IBNE',0x29),('IBLTZ',0x2c),('IBGTZ',0x2d),
 ('IBLEZ',0x2e),('IBGEZ',0x2f)]:
    add(name,direct(op))

# Plain SPECIAL integer ALU uses funct6 directly, not the fd/bc subgroup form.
for name,fn in [('IADD',0x30),('ISUB',0x31),('IADDI',0x32),('IOR',0x33),('IAND',0x34)]:
    w=(0x40<<25)|(0xf<<21)|(4<<16)|(3<<11)|(5<<6)|fn
    add(name,w)

for name,rd,bc in [
 ('MOVE',0x0c,0),('MR32',0x0c,1),
 ('LQI',0x0d,0),('SQI',0x0d,1),('LQD',0x0d,2),('SQD',0x0d,3),
 ('DIV',0x0e,0),('SQRT',0x0e,1),('RSQRT',0x0e,2),('WAITQ',0x0e,3),
 ('MTIR',0x0f,0),('MFIR',0x0f,1),('ILWR',0x0f,2),('ISWR',0x0f,3),
 ('XTOP',0x1a,0),('XITOP',0x1a,1),('XGKICK',0x1b,0),('WAITP',0x1e,3)]:
    boundary = 'scheduler/side-effect' if name in {'DIV','SQRT','RSQRT','WAITQ','XTOP','XITOP','XGKICK','WAITP'} else 'ordinary'
    add(name,special(rd,bc),boundary)

# VU1 EFU/P producers implemented by vu_efu_compute().
latency={
 28:[11,18,18,24],29:[54,54,12,0],30:[12,18,12,0],31:[29,54,44,0]
}
for rd,cycles in latency.items():
    for bc,cy in enumerate(cycles):
        if cy:
            add(f'EFU rd={rd} bc={bc}',special(rd,bc),'scheduler/side-effect')

native=[r for r in rows if r[2]]
rejected=[r for r in rows if not r[2]]
ordinary_gap=[r for r in rejected if r[1]=='ordinary']
boundary=[r for r in rejected if r[1]!='ordinary']
print('R1323 VU micro lower native-gap discovery')
print('  interpreter-backed canonical forms:',len(rows))
print('  accepted by PPC lower translator:',len(native))
print('  rejected/fallback:',len(rejected))
print('  ordinary native gaps:',len(ordinary_gap))
print('  scheduler/side-effect boundaries:',len(boundary))
for name,kind,ok,w in rows:
    print(f'  {"NATIVE" if ok else "FALLBACK":8} {kind:22} {name:18} {w:#010x}')
