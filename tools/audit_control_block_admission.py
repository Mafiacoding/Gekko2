#!/usr/bin/env python3
"""R1320 discovery audit for EE precise-block control instructions.

Derive the branch/jump selector sets from the checked-in PCSX2 R5900 opcode
reference, then require Gekko2's precise-block terminal policy and PPC backend
to agree for every relevant primary/SPECIAL/REGIMM/COP1-BC1 selector. Generated
PPC is inspected through translation return values only; it is never executed
on the host.
"""
from pathlib import Path
import ctypes as C
import re, subprocess, sys

root = Path(__file__).resolve().parents[1]
out = root / 'outputs' / 'verification'
out.mkdir(parents=True, exist_ok=True)
ref = (root / 'docs/reference/pcsx2/pcsx2/R5900OpcodeTables.cpp').read_text()

class Context(C.Structure):
    _fields_ = [('code', C.POINTER(C.c_uint32)),
                ('capacity_words', C.c_size_t),
                ('used_words', C.c_size_t)]

def table(name, n):
    m = re.search(r'static const OPCODE\s+' + re.escape(name) +
                  r'\[' + str(n) + r'\]\s*=\s*\{(.*?)\};', ref, re.S)
    if not m:
        raise SystemExit('missing reference table ' + name)
    vals = [x.strip() for x in m.group(1).split(',') if x.strip()]
    if len(vals) != n:
        raise SystemExit(f'{name}: expected {n} entries, got {len(vals)}')
    return vals

# Locate the primary 64-entry table by contents instead of depending on its
# historical variable name.
primary = None
for m in re.finditer(r'static const OPCODE\s+(tbl_\w+)\[64\]\s*=\s*\{(.*?)\};', ref, re.S):
    vals = [x.strip() for x in m.group(2).split(',') if x.strip()]
    if len(vals) == 64 and all(x in vals for x in ('SPECIAL','REGIMM','J','JAL','BEQ','BNE')):
        primary = vals
        break
if primary is None:
    raise SystemExit('could not locate primary R5900 opcode table')

special = table('tbl_Special', 64)
regimm = table('tbl_RegImm', 32)
bc1 = table('tbl_COP1_BC1', 32)

primary_controls = {'J','JAL','BEQ','BNE','BLEZ','BGTZ','BEQL','BNEL','BLEZL','BGTZL'}
special_controls = {'JR','JALR'}
regimm_controls = {'BLTZ','BGEZ','BLTZL','BGEZL','BLTZAL','BGEZAL','BLTZALL','BGEZALL'}
bc1_controls = {'BC1F','BC1T','BC1FL','BC1TL'}

# Build real backend plus a tiny policy shim.
so = out / 'libcontrol_admission_backend.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),
                str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(so)], check=True)
lib = C.CDLL(str(so))
lib.ppc_dynarec_translate_one.argtypes = [C.POINTER(Context), C.c_uint32]
lib.ppc_dynarec_translate_one.restype = C.c_int
lib.ppc_dynarec_translate_ee_resident_delay_block.argtypes = [C.POINTER(Context),C.c_uint32,
    C.POINTER(C.c_uint32),C.c_uint,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
lib.ppc_dynarec_translate_ee_resident_delay_block.restype = C.c_int

shim = out / 'control_policy_shim.c'
shim.write_text('#include <stdint.h>\n#include "core/recompiler/ee_block_policy.h"\n'
                'int terminal(uint32_t w){return ee_jit_block_terminal(w);}\n'
                'int candidate(uint32_t w){return ee_jit_block_candidate(w);}\n')
shimso = out / 'libcontrol_policy.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(shim),'-o',str(shimso)], check=True)
policy = C.CDLL(str(shimso))
policy.terminal.argtypes=[C.c_uint32]; policy.terminal.restype=C.c_int
policy.candidate.argtypes=[C.c_uint32]; policy.candidate.restype=C.c_int

def single_ok(iw):
    code=(C.c_uint32*4096)(); ctx=Context(code,4096,0)
    return lib.ppc_dynarec_translate_one(C.byref(ctx), C.c_uint32(iw)) == 0

def block_ok(iw):
    # ADDIU $zero,$zero,0 is a safe legal delay-slot candidate.
    delay=(0x09<<26)
    words=(C.c_uint32*2)(iw,delay)
    code=(C.c_uint32*8192)(); ctx=Context(code,8192,0)
    return lib.ppc_dynarec_translate_ee_resident_delay_block(
        C.byref(ctx),0x00200000,words,2,0x1000,0x2000,0x3000,0x4000,4) == 0

mismatches=[]
canonical=[]

def check(kind, selector, name, iw, expected):
    got=bool(policy.terminal(iw))
    if got != expected:
        mismatches.append((kind,selector,name,'terminal-policy',expected,got,hex(iw)))
    if expected:
        s=single_ok(iw); b=block_ok(iw)
        canonical.append((kind,selector,name,s,b,hex(iw)))
        if not s: mismatches.append((kind,selector,name,'single-backend',True,s,hex(iw)))
        if not b: mismatches.append((kind,selector,name,'resident-block',True,b,hex(iw)))

# Direct primary controls. Subclasses are audited below at their real selector.
for op,name in enumerate(primary):
    if name in ('SPECIAL','REGIMM','COP1'):
        continue
    iw=(op<<26)|(1<<21)|(2<<16)|1
    check('primary',op,name,iw,name in primary_controls)

for f,name in enumerate(special):
    iw=(1<<21)|(31<<11)|f
    check('SPECIAL',f,name,iw,name in special_controls)

for rt,name in enumerate(regimm):
    iw=(1<<26)|(1<<21)|(rt<<16)|1
    check('REGIMM',rt,name,iw,name in regimm_controls)

# Exhaust the COP1 rs/rt selector plane. Only rs==8 dispatches BC1, and the
# checked-in BC1 table defines exactly four branch variants.
for rs in range(32):
    for rt in range(32):
        name = bc1[rt] if rs == 8 else 'non-BC1'
        iw=(0x11<<26)|(rs<<21)|(rt<<16)|1
        check('COP1',rs*32+rt,name,iw,rs==8 and name in bc1_controls)

print('R1320 control selector audit')
print('  canonical terminal encodings:', len(canonical))
print('  single backend accepted:', sum(1 for x in canonical if x[3]))
print('  resident block accepted:', sum(1 for x in canonical if x[4]))
print('  selector mismatches:', len(mismatches))
if mismatches:
    for row in mismatches[:40]: print('  MISMATCH', row)
    sys.exit(1)
print('R1320 control-path admission audit: PASS')
