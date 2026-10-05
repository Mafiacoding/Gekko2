#!/usr/bin/env python3
"""R1319 verification: exact legal MMI selectors must be admitted by the
precise-block policy and accepted by both the single-op and resident-block PPC
translators. Unknown subgroup selectors and invalid PMFHL modes stay scalar.
No generated PPC is executed on the host.
"""
from pathlib import Path
import ctypes as C
import json, re, subprocess

root=Path(__file__).resolve().parents[1]
out=root/'outputs/verification'; out.mkdir(parents=True,exist_ok=True)
ref=(root/'docs/reference/pcsx2/pcsx2/R5900OpcodeTables.cpp').read_text()

class Context(C.Structure):
    _fields_=[('code',C.POINTER(C.c_uint32)),('capacity_words',C.c_size_t),('used_words',C.c_size_t)]

def table(name):
    m=re.search(r'tbl_'+name+r'\[\d+\]\s*=\s*\{(.*?)\};',ref,re.S)
    if not m: raise SystemExit('missing table '+name)
    return [x.strip() for x in m.group(1).split(',') if x.strip()]

def load_backend(path):
    subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),
                    str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(path)],check=True)
    lib=C.CDLL(str(path))
    lib.ppc_dynarec_translate_one.argtypes=[C.POINTER(Context),C.c_uint32]
    lib.ppc_dynarec_translate_one.restype=C.c_int
    lib.ppc_dynarec_translate_ee_resident_delay_block.argtypes=[C.POINTER(Context),C.c_uint32,C.POINTER(C.c_uint32),C.c_uint,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
    lib.ppc_dynarec_translate_ee_resident_delay_block.restype=C.c_int
    return lib

lib=load_backend(out/'libmmi_block_audit.so')
shim=out/'mmi_policy_shim.c'
shim.write_text('#include <stdint.h>\n#include "core/recompiler/ee_block_policy.h"\nint candidate(uint32_t w){return ee_jit_block_candidate(w);}\nint mmi(uint32_t w){return ee_jit_block_mmi(w);}\n')
shimso=out/'libmmi_policy.so'
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(shim),'-o',str(shimso)],check=True)
policy=C.CDLL(str(shimso))
policy.candidate.argtypes=[C.c_uint32]; policy.candidate.restype=C.c_int
policy.mmi.argtypes=[C.c_uint32]; policy.mmi.restype=C.c_int

tables={name:table(name) for name in ('MMI','MMI0','MMI1','MMI2','MMI3')}
subdispatch={'MMI0':'MMI0','MMI1':'MMI1','MMI2':'MMI2','MMI3':'MMI3'}

def selector_legal(funct,sa):
    name=tables['MMI'][funct]
    if name=='MMI_Unknown': return False
    if name in subdispatch:
        return tables[subdispatch[name]][sa]!='MMI_Unknown'
    if name=='PMFHL': return sa<=4
    return True

# Exhaust every primary-funct/sa selector pair. Direct MMI operations ignore
# sa, while subgroup dispatch and PMFHL use it architecturally.
selector_rows=[]
for funct in range(64):
    for sa in range(32):
        iw=(28<<26)|(1<<21)|(2<<16)|(3<<11)|(sa<<6)|funct
        expected=selector_legal(funct,sa)
        got=bool(policy.mmi(iw))
        candidate=bool(policy.candidate(iw))
        selector_rows.append({'funct':funct,'sa':sa,'expected':expected,'mmi':got,'candidate':candidate})

# Keep the historical 103 canonical legal encodings as the backend/block
# reachability set so the result remains directly comparable with R1315.
rows=[]
for tab,fn in [('MMI',None),('MMI0',8),('MMI1',40),('MMI2',9),('MMI3',41)]:
    names=tables[tab]
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
            rows.append({'table':tab,'name':name,'mode':mode,'iw':f'{iw:08x}',
                         'single':single,'precise_block':block,
                         'policy_candidate':bool(policy.candidate(iw)),
                         'single_words':ctx.used_words,'block_words':bctx.used_words})

mismatch=[r for r in selector_rows if r['mmi']!=r['expected'] or r['candidate']!=r['expected']]
(out/'mmi_block_admission.json').write_text(json.dumps({'canonical':rows,'selectors':selector_rows},indent=2))
print('Legal canonical encodings:',len(rows))
print('Single backend accepted:',sum(r['single'] for r in rows))
print('Precise resident block accepted:',sum(r['precise_block'] for r in rows))
print('Policy admitted:',sum(r['policy_candidate'] for r in rows))
print('Selector pairs checked:',len(selector_rows),'mismatches:',len(mismatch))
if mismatch:
    print('First selector mismatches:',mismatch[:16])
assert len(rows)==103
assert all(r['single'] and r['precise_block'] and r['policy_candidate'] for r in rows)
assert not mismatch
print('R1319 MMI precise-block admission audit: PASS')
