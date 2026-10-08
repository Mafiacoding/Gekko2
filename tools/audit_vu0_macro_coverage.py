#!/usr/bin/env python3
"""R1322: exact VU0 macro-mode interpreter/backend reachability audit.

The expected selector set is taken from source/core/ee/ee_core.c's current
COP2 decoder. The audit compiles the real PPC translator and checks that
ppc_dynarec_translate_one() accepts every interpreter-supported selector and
rejects reserved/unimplemented representatives. Generated PPC is not run on
the host.
"""
from pathlib import Path
import ctypes as C
import subprocess, sys

root = Path(__file__).resolve().parents[1]
out = root / 'outputs' / 'verification'
out.mkdir(parents=True, exist_ok=True)
so = out / 'libvu0_macro_coverage.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),
                str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(so)], check=True)

class Context(C.Structure):
    _fields_ = [('code', C.POINTER(C.c_uint32)),
                ('capacity_words', C.c_size_t),
                ('used_words', C.c_size_t)]

lib = C.CDLL(str(so))
lib.ppc_dynarec_translate_one.argtypes = [C.POINTER(Context), C.c_uint32]
lib.ppc_dynarec_translate_one.restype = C.c_int

def accepted(iw):
    code = (C.c_uint32 * 32768)()
    ctx = Context(code, 32768, 0)
    return lib.ppc_dynarec_translate_one(C.byref(ctx), C.c_uint32(iw)) == 0

errors=[]
checked=0
legal=0

def check(label, iw, expected):
    global checked, legal
    checked += 1
    legal += int(expected)
    got = accepted(iw)
    if got != expected:
        errors.append((label, hex(iw), expected, got))

scalar_legal = {0x00,0x01,0x02,0x04,0x05,0x06}
for rs in range(0x10):
    iw=(0x12<<26)|(rs<<21)|(3<<16)|(4<<11)
    check(f'scalar rs={rs:#04x}', iw, rs in scalar_legal)

co_legal = set(range(0x00,0x33)) | {0x34,0x35}
for mask in range(16):
    rs=0x10|mask
    for funct in range(0x3c):
        iw=(0x12<<26)|(rs<<21)|(3<<16)|(4<<11)|(5<<6)|funct
        check(f'CO mask={mask:x} funct={funct:#04x}', iw, funct in co_legal)

special2_legal = set(range(0,43)) | set(range(44,50)) | set(range(52,68))
for mask in range(16):
    rs=0x10|mask
    for idx in range(128):
        fd=(idx>>2)&31
        funct=0x3c|(idx&3)
        iw=(0x12<<26)|(rs<<21)|(3<<16)|(4<<11)|(fd<<6)|funct
        check(f'SPECIAL2 mask={mask:x} idx={idx}', iw, idx in special2_legal)

print('R1322 VU0 macro coverage audit')
print('  selector representatives checked:', checked)
print('  interpreter-supported representatives:', legal)
print('  mismatches:', len(errors))
for row in errors[:120]:
    print('  MISMATCH', row)
if errors:
    sys.exit(1)
print('R1322 VU0 macro coverage audit: PASS')
