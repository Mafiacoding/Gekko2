#!/usr/bin/env python3
"""R1320: exhaust the selector-bearing EE precise-block admission surface.

Every representative encoding admitted as a candidate or terminal must be
accepted by the resident precise-block translator. Non-memory instructions
must also be accepted by the one-op translator. Proven RAM memory operations
may deliberately be resident-only because that path has specialized merge,
COP1/vector and direct-RAM emitters. No generated PPC executes on the host.
"""
from pathlib import Path
import ctypes as C
import json, subprocess

root=Path(__file__).resolve().parents[1]
out=root/'outputs/verification'; out.mkdir(parents=True,exist_ok=True)

class Context(C.Structure):
    _fields_=[('code',C.POINTER(C.c_uint32)),('capacity_words',C.c_size_t),('used_words',C.c_size_t)]

backend_so=out/'libr1320_block_backend.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(backend_so)],check=True)
lib=C.CDLL(str(backend_so))
lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
lib.ppc_dynarec_translate_one.restype=C.c_int
lib.ppc_dynarec_translate_ee_resident_delay_block.argtypes=[C.POINTER(Context),C.c_uint32,C.POINTER(C.c_uint32),C.c_uint,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
lib.ppc_dynarec_translate_ee_resident_delay_block.restype=C.c_int

shim=out/'r1320_block_policy_shim.c'
shim.write_text('#include <stdint.h>\n#include "core/recompiler/ee_block_policy.h"\nint cand(uint32_t w){return ee_jit_block_candidate(w);}\nint term(uint32_t w){return ee_jit_block_terminal(w);}\nunsigned width(uint32_t w){return ee_jit_block_memory_width(w);}\n')
shim_so=out/'libr1320_block_policy.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(shim),'-o',str(shim_so)],check=True)
pol=C.CDLL(str(shim_so))
pol.cand.argtypes=[C.c_uint32]; pol.cand.restype=C.c_int
pol.term.argtypes=[C.c_uint32]; pol.term.restype=C.c_int
pol.width.argtypes=[C.c_uint32]; pol.width.restype=C.c_uint

def enc(op,rs=1,rt=2,rd=3,sa=4,funct=0):
    return ((op&63)<<26)|((rs&31)<<21)|((rt&31)<<16)|((rd&31)<<11)|((sa&31)<<6)|(funct&63)

words=set()
for op in range(64): words.add(enc(op))
for f in range(64):
    for sa in (0,1,4,31): words.add(enc(0,sa=sa,funct=f))
for rt in range(32): words.add(enc(1,rt=rt))
for f in range(64):
    for sa in range(32): words.add(enc(0x1c,sa=sa,funct=f))
for rs in range(32):
    for rt in range(32): words.add(enc(0x11,rs=rs,rt=rt))
for rs in (0x10,0x14):
    for f in range(64): words.add(enc(0x11,rs=rs,funct=f))
for op in (0x1e,0x36):
    for rt in (0,1,31): words.add(enc(op,rt=rt))
for op in (2,3,4,5,6,7,0x14,0x15,0x16,0x17):
    for rs in (0,1,31):
        for rt in (0,2,31): words.add(enc(op,rs=rs,rt=rt))
for f in (8,9):
    for rs in (0,1,31): words.add(enc(0,rs=rs,rd=31,funct=f))

rows=[]; misses=[]; resident_only=[]
for iw in sorted(words):
    candidate=bool(pol.cand(iw)); terminal=bool(pol.term(iw))
    if not (candidate or terminal): continue
    memory_width=int(pol.width(iw))
    code=(C.c_uint32*8192)(); ctx=Context(code,8192,0)
    one_rc=lib.ppc_dynarec_translate_one(C.byref(ctx),iw)
    pair=(C.c_uint32*2)(iw,0)
    bcode=(C.c_uint32*16384)(); bctx=Context(bcode,16384,0)
    block_rc=lib.ppc_dynarec_translate_ee_resident_delay_block(C.byref(bctx),0x00200000,pair,2,0x1000,0x2000,0x3000,0x4000,4)
    one_required=memory_width==0
    row={'iw':f'{iw:08x}','candidate':candidate,'terminal':terminal,
         'memory_width':memory_width,'one_required':one_required,
         'one_rc':one_rc,'block_rc':block_rc,'one_words':ctx.used_words,'block_words':bctx.used_words}
    rows.append(row)
    if block_rc!=0 or (one_required and one_rc!=0): misses.append(row)
    elif one_rc!=0: resident_only.append(row)

(out/'ee_block_policy_reachability_r1320.json').write_text(json.dumps(rows,indent=2))
print('R1320 admitted representative encodings:',len(rows))
print('  candidates:',sum(r['candidate'] for r in rows),'terminals:',sum(r['terminal'] for r in rows))
print('  expected resident-only memory forms:',len(resident_only))
print('  real reachability mismatches:',len(misses))
if misses:
    for r in misses[:40]: print(' -',r)
    raise SystemExit(1)
print('R1320 EE precise-block reachability audit: PASS')
