#!/usr/bin/env python3
"""GPL-3.0-or-later. Batch A11 differential: strict parser -> AOT -> real EE core.

Parses the unmodified golden upstream output, lowers only the validated words
through tools/ee_aot_generate.py and runs them under the live scheduler against
the interpreter. Host-native evidence only (no Wii/Dolphin).
"""
import importlib.util
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[1]
FIXTURES = [  # (fixture dir, generated function names). One AOT table per fixture.
    ('ps2recomp_a11', ['a11_int_leaf_0x100000', 'a11_zero_dest_0x100018']),
    ('ps2recomp_a12', ['a12_imm_leaf_0x100000', 'a12_reg_leaf_0x100020', 'a12_zero_dest_0x100040']),
]
spec = importlib.util.spec_from_file_location('strict', ROOT / 'tools/ps2recomp_strict_parser.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


def dest_mask(words):
    mask = 0
    for w in words:
        op, rt, rd = w >> 26, w >> 16 & 31, w >> 11 & 31
        mask |= 1 << (rt if 9 <= op <= 15 else rd) if w else 0
    return mask & ~1  # $zero is never a changeable destination


for fixture, names in FIXTURES:
    with tempfile.TemporaryDirectory(prefix='gekko-a11-') as tmp:
        d = Path(tmp)
        gen = d / 'generated'
        (gen / 'core/ee').mkdir(parents=True)
        source = (ROOT / 'tests/fixtures' / fixture / 'ps2_recompiled_functions.cpp').read_text()
        results = [p.parse_function(source, n) for n in names]
        report = p.lower(results, gen / 'ee_aot_generated.inc', gen / 'core/ee/ee_aot_complex_generated.inc')
        (gen / 'ee_aot.c').write_text((ROOT / 'source/core/recompiler/ee_aot.c').read_text())
        rows = []
        for r in results:
            words = ','.join(f'UINT32_C(0x{w:08x})' for w in r['words']) + ',' + ','.join(['0'] * (32 - len(r['words'])))
            rows.append(f'{{UINT32_C(0x{r["start"]:08x}),{len(r["words"])}u,UINT32_C(0x{dest_mask(r["words"]):x}),{{{words}}}}}')
        (gen / 'a11_fixtures.h').write_text(
            '#include <stdint.h>\ntypedef struct{uint32_t pc;unsigned count;uint32_t dest_mask;uint32_t words[32];}a11_fixture;\n'
            'static const a11_fixture a11_fixtures[]={' + ','.join(rows) + '};\n'
            'static const unsigned a11_fixture_count=%du;\n' % len(rows))
        saved = sys.argv
        sys.argv = ['tools/build_host_core.py']
        try:
            s = runpy.run_path(str(ROOT / 'tools/build_host_core.py'))
        finally:
            sys.argv = saved
        flags = ['-DGEKKO2_EE_AOT', '-I' + str(gen), *s['flags']]
        objects = []
        for path, obj in s['objects'].items():
            if path == ROOT / 'source/core/ee/ee_core.c' or path == ROOT / 'source/core/recompiler/ee_aot.c':
                src = gen / 'ee_aot.c' if path.name == 'ee_aot.c' else path
                obj = d / (path.stem + '.o')
                subprocess.run(['gcc', *flags, '-c', str(src), '-o', str(obj)], cwd=ROOT, check=True)
            objects.append(str(obj))
        exe = d / 'a11-diff'
        subprocess.run(['gcc', *flags, str(ROOT / 'tests/test_ps2recomp_a11_aot_diff.c'), *objects,
                        '-Wl,--gc-sections', '-lm', '-o', str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe)], cwd=ROOT, check=True, timeout=120)
        print('AOT sha256', report['aot_sha256'])
