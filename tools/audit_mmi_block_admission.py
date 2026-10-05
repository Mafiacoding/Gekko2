#!/usr/bin/env python3
"""R1319 discovery: compare legal MMI encodings with single-op backend,
precise-block backend, and the public EE block-candidate policy.
No generated PPC is executed on the host.
"""
from pathlib import Path
import ctypes as C
import json, re, subprocess, tempfile

root=Path(__file__).resolve().parents[1]
out=root/'outputs/verification'; out.mkdir(parents=True,exist_ok=True)
ref=(root/'docs/reference/pcsx2/pcsx2/R5900OpcodeTables.cpp').read_text()
so=out/'libmmi_block_audit.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),
                str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(so)],check=True)
class Context(C.Structure):
    _fields_=[('code',C.POINTER(C.c_uint32)),('capacity_words',C.c_size_t),('used_words',C.c_size_t)]
lib=C.CDLL(str(so))
lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
lib.ppc_dynarec_translate_one.restype=C.c_int
lib.ppc_dynarec_translate_ee_resident_delay_block.argtypes=[C.POINTER(Context),C.c_uint32,C.POINTER(C.c_uint32),C.c_uint,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
lib.ppc_dynarec_translate_ee_resident_delay_block.restype=C.c_int

# Compile the actual inline block policy into a callable host shim.
shim=out/'mmi_policy_shim.c'
shim.write_text('#include <stdint.h>\n#include "core/recompiler/ee_block_policy.h"\nint candidate(uint32_t w){return ee_jit_block_candidate(w);}\n')
shimso=out/'libmmi_policy.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(shim),'-o',str(shimso)],check=True)
policy=C.CDLL(str(shimso)); policy.candidate.argtypes=[C.c_uint32]; policy.candidate.restype=C.c_int

rows=[]
for table,fn in [('MMI',None),('MMI0',8),('MMI1',40),('MMI2',9),('MMI3',41)]:
    m=re.search(r'tbl_'+table+r'\[\d+\]\s*=\s*\{(.*?)\};',ref,re.S)
    if not m: raise SystemExit('missing table '+table)
    names=[x.strip() for x in m.group(1).split(',') if x.strip()]
    for pos,name in enumerate(names):
        if name in ['MMI_Unknown','MMI0','MMI1','MMI2','MMI3']: continue
        modes=range(5) if name=='PMFHL' else [pos if fn is not None else 0]
        for mode in modes:
            iw=(28<<26)|(1<<21)|(2<<16)|(3<<11)|(mode<<6)|(pos if fn is None else fn)
            code=(C.c_uint32*4096)(); ctx=Context(code,4096,0)
            single=lib.ppc_dynarec_translate_one(C.byref(ctx),iw)==0
            words=(C.c_uint32*2)(iw,0)
            blockcode=(C.c_uint32*4096)(); bctx=Context(blockcode,4096,0)
            block=lib.ppc_dynarec_translate_ee_resident_delay_block(
                C.byref(bctx),0x00200000,words,2,0x1000,0x2000,0x3000,0x4000,4)==0
            rows.append({'table':table,'name':name,'mode':mode,'iw':f'{iw:08x}',
                         'single':single,'precise_block_backend':block,
                         'policy_candidate':bool(policy.candidate(iw)),
                         'single_words':ctx.used_words,'block_words':bctx.used_words})

(out/'mmi_block_admission.json').write_text(json.dumps(rows,indent=2))
print('legal',len(rows))
print('single backend',sum(r['single'] for r in rows))
print('precise block backend',sum(r['precise_block_backend'] for r in rows))
print('policy admitted',sum(r['policy_candidate'] for r in rows))
print('block backend declines',[r['name'] for r in rows if not r['precise_block_backend']])
print('policy declines',[r['name'] for r in rows if not r['policy_candidate']][:120])
assert all(r['single'] for r in rows)
