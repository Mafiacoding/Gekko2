#!/usr/bin/env python3
"""Rebuild the actual EE core with synthetic generated blocks; no private inputs."""
import concurrent.futures
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import re
import sys

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('aot',root/'tools/ee_aot_generate.py')
aot=importlib.util.module_from_spec(spec);spec.loader.exec_module(aot)
def module(name):
    spec=importlib.util.spec_from_file_location(name,root/f'tools/{name}.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
vu_aot=module('vu_aot_generate');iop_aot=module('iop_aot_generate')
blocks=[]
memory_ops=tuple(sorted(aot.MEMORY_OPS))
words=[(op<<26)|(1<<21)|(2<<16)|0xffff for op in (9,10,11,12,13,14,25)]
words += [(15<<26)|(2<<16)|0x8000]
words += [(1<<21)|(2<<16)|(3<<11)|(31<<6)|fn for fn in (0,2,3,4,6,7,10,11,16,17,18,19,20,22,23,33,35,36,37,38,39,42,43,45,47,56,58,59,60,62,63)]
words += [(op<<26)|(1<<21)|(2<<16)|8 for op in memory_ops]
words += [(0x36<<26)|(1<<21)|8]
words += [(op<<26)|(1<<21)|(2<<16)|3 for op in (4,5,6,7,20,21,22,23)]
words += [(1<<26)|(31<<21)|(rt<<16)|0xfffd for rt in (0,1,2,3,16,17,18,19)]
words += [(op<<26)|0x80001 for op in (2,3)]
words += [(3<<21)|(3<<11)|fn for fn in (8,9)]
words += [(9<<26)|(1<<21)|0xffff, 0, 15]
words += [(op<<26)|(1<<21)|(2<<16)|0x7fff for op in (8,24)]
words += [(1<<21)|(2<<16)|(3<<11)|fn for fn in (24,25,26,27,32,34,40,41,44,46,48,49,50,51,52,54)]
words += [(1<<26)|(1<<21)|(rt<<16)|0xffff for rt in (8,9,10,11,12,14,24,25)]
for fn in range(64):
    for sa in (range(32) if fn in (8,9,40,41) else [0]):
        word=(28<<26)|(1<<21)|(2<<16)|(3<<11)|(sa<<6)|fn
        if aot.complex_family(word):words.append(word)
for rs in (0,2,4,6,8,16,20):
    for fn in (range(64) if rs in (16,20) else [0]):
        word=(17<<26)|(rs<<21)|(2<<16)|(1<<11)|(3<<6)|fn
        if aot.complex_family(word):words.append(word)
for rt in range(4):words.append((18<<26)|(8<<21)|(rt<<16)|3)
for rs in (0,1,2,4,5,6,31):
    for fn in (range(64) if rs==31 else [0]):
        word=(18<<26)|(rs<<21)|(2<<16)|(1<<11)|(3<<6)|fn
        if aot.complex_family(word) and fn not in (56,57):words.append(word)
for index in range(128):
    word=(18<<26)|(31<<21)|(2<<16)|(1<<11)|((index>>2)<<6)|60|(index&3)
    if aot.complex_family(word):words.append(word)
for rs,rd in ((0,9),(0,12),(4,9),(4,10),(4,11),(4,16)):
    words.append((16<<26)|(rs<<21)|(2<<16)|(rd<<11))
for index, word in enumerate(words):
    pc=0x80200000+index*0x100
    translated=aot.translate(word,pc)
    assert translated is not None
    blocks.append((pc,[word] if translated[1] else [word,0]))
# Fused terminal branch and legal slot fixtures, including faults and live memory.
for index, (branch, slot) in enumerate((
    (0x10220003,0x24630001), (0x50220003,0x24630001),
    (0x04210003,0x24630001), (0x04310003,0x24630001),
    (0x00201809,0x24630001), (0x10220003,0x00221820),
    (0x10220003,0x00221834), (0x10220003,0x8c830008),
    (0x10220003,0x48830800), (0x41010003,0x24630001),
    (0x45010003,0x24630001), (0x49010003,0x24630001))):
    blocks.append((0x80310000+index*256,[branch,slot]))
blocks.append((0x00200000,[0x2422ffff,0]))
for invalid in ([(3,[0])],[(0x80200000,[0x0000000c])],[(0x80200000,[0,0])]*2,[(0x80200000,[0x10000000,0,0])]):
    try:aot.generate(invalid)
    except ValueError:pass
    else:raise AssertionError('invalid block accepted')
with tempfile.TemporaryDirectory(prefix='gekko2-aot-') as temporary:
    out=Path(temporary)
    generated=aot.generate(blocks)
    (out/'ee_aot_generated.inc').write_text(generated)
    (out/'core/ee').mkdir(parents=True)
    (out/'core/ee/ee_aot_complex_generated.inc').write_text(aot.generate_complex(blocks))
    vu_upper=[(15<<21)|(2<<16)|(1<<11)|(3<<6)|fn for fn in range(64)]
    vu_upper += [(15<<21)|(2<<16)|(1<<11)|(sub<<6)|60|bc for sub in range(16) for bc in range(4)]
    vu_lower=[(op<<25)|(15<<21)|(2<<16)|(1<<11) for op in (0,1,4,5,8,9,16,17,18,19,20,21,22,23,24,26,27,28,32,33,36,37,40,41,44,45,46,47)]
    vu_lower += [(64<<25)|(15<<21)|(2<<16)|(1<<11)|(3<<6)|fn for fn in (48,49,50,52,53)]
    vu_lower += [(64<<25)|(15<<21)|(2<<16)|(1<<11)|(sub<<6)|60|bc for sub in range(12,32) for bc in range(4)]
    (out/'hw').mkdir();(out/'hw/vu_aot_generated.inc').write_text(vu_aot.generate(vu_upper+[0x2ff],vu_lower+[0x800003bf]))
    vu_pairs=[(w,0x800003bf) for w in vu_upper]+[(0x01e208e8,w) for w in vu_lower]
    (out/'vu_aot_test_fixture.h').write_text('static const uint32_t vu_aot_fixtures[][2]={'+','.join('{0x%08xu,0x%08xu}'%pair for pair in vu_pairs)+'};\nstatic const unsigned vu_aot_fixture_count=%du;\n'%len(vu_pairs))
    iop_words=[(op<<26)|(1<<21)|(2<<16) for op in range(64) if iop_aot.supported((op<<26)|(1<<21)|(2<<16))]
    iop_words += [(1<<21)|(2<<16)|(3<<11)|(5<<6)|fn for fn in range(64) if iop_aot.supported((1<<21)|(2<<16)|(3<<11)|(5<<6)|fn)]
    iop_entries=[(0x80100000+i*256,w) for i,w in enumerate(iop_words)]
    (out/'core/iop').mkdir();(out/'core/iop/iop_aot_generated.inc').write_text(iop_aot.generate(iop_entries))
    (out/'iop_aot_test_fixture.h').write_text('static const uint32_t iop_aot_fixtures[][2]={'+','.join('{0x%08xu,0x%08xu}'%pair for pair in iop_entries)+'};\nstatic const unsigned iop_aot_fixture_count=%du;\n'%len(iop_entries))
    shutil.copy(root/'source/core/recompiler/ee_aot.c',out/'ee_aot.c')
    fixture=['typedef struct { uint32_t pc, words[32]; unsigned count, memory; } aot_fixture;','static const aot_fixture aot_fixtures[]={']
    for pc, ws in blocks:
        fixture.append('{0x%08xu,{%s},%du,%du},'%(pc,','.join('0x%08xu'%w for w in ws),len(ws),int(ws[0]>>26 in memory_ops)))
    fixture += ['};',f'static const unsigned aot_fixture_count={len(blocks)}u;']
    (out/'aot_test_fixture.h').write_text('\n'.join(fixture))
    cc=os.environ.get('CC','cc')
    v=root/'third_party/libchdr'
    flags=['-O2','-ffp-contract=off','-DGEKKO2_EE_AOT','-I'+str(out),'-I'+str(root/'include'),'-I'+str(root/'source'),'-I'+str(v/'include'),'-DWANT_RAW_DATA_SECTOR=1','-DWANT_SUBCODE=1','-DVERIFY_BLOCK_CRC=1','-DMINIZ_NO_ARCHIVE_APIS','-DMINIZ_NO_DEFLATE_APIS','-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-D_FILE_OFFSET_BITS=64','-D_LARGEFILE_SOURCE=1']
    flags += ['-DGEKKO2_VU_AOT','-DGEKKO2_IOP_AOT']
    sources=[p for p in (root/'source').rglob('*.c') if p.name not in ('main.c','ppc_dynarec.c','ee_aot.c')]
    sources += list((v/'src').glob('*.c'))+[v/'deps/lzma-25.01/src/LzmaDec.c',v/'deps/miniz-3.1.1/miniz.c',v/'deps/zstd-1.5.7/zstddeclib.c',out/'ee_aot.c']
    def compile_one(pair):
        index, source=pair;obj=out/f'{index}.o'
        subprocess.run([cc,*flags,'-c',str(source),'-o',str(obj)],check=True,stdout=subprocess.DEVNULL)
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        objects=list(pool.map(compile_one,enumerate(sources)))
    for name in ['test_ee_aot','test_vu_aot','test_iop_aot']:
        exe=out/name
        subprocess.run([cc,*flags,str(root/f'tests/{name}.c'),*objects,'-lm','-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    regressions=[p for p in sorted((root/'tests').glob('test_*.c')) if p.name.startswith(('test_ee_mmi_','test_ee_cop0_','test_ee_cop2_','test_ee_fpu')) or p.stem in ('test_ee_exceptions','test_ee_vector_memory_internal','test_ee_traps_internal','test_iop_pipeline_internal','test_iop_divide_r1269','test_iop_hw_interrupt','test_iop_hle_thread','test_vu_micro','test_vu_q_pipeline_internal','test_vu_r_lfsr_r1326','test_system_handshake')]
    if '--focused' in sys.argv:regressions=[]
    for source in regressions:
        excluded={str(root/'source'/m) for m in re.findall(r'#include "((?:core|hw)/[^"]+\.c)"',source.read_text())}
        selected=[obj for p,obj in zip(sources,objects) if str(p) not in excluded]
        exe=out/source.stem
        subprocess.run([cc,*flags,str(source),*selected,'-lm','-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=60)
    # Compile the profiler as part of the tested end-to-end tooling.
    profiler=root.parent/'ee_aot_profile_host'
    subprocess.run([cc,*flags,str(root/'tools/ee_aot_profile.c'),*objects,'-lm','-o',str(profiler)],check=True)
    print(json.dumps({'generated_blocks':len(blocks),'ee_comparisons':len(blocks)*12,'vu_comparisons':len(vu_pairs)*16,'iop_comparisons':len(iop_entries)*5,'legacy_regressions':len(regressions),'all_passed':True,'wii_execution_verified':False}))
