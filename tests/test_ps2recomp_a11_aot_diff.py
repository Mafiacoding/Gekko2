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
    ('ps2recomp_a11', 'a11', ['a11_int_leaf_0x100000', 'a11_zero_dest_0x100018']),
    ('ps2recomp_a12', 'a12', ['a12_imm_leaf_0x100000', 'a12_reg_leaf_0x100020', 'a12_zero_dest_0x100040']),
    ('ps2recomp_a13', 'a13', ['a13_ret_nop_0x100000', 'a13_ret_slot_0x100010', 'a13_ret_only_0x10001c',
                              'a13_plain_hex_0x100024', 'a13_ra_slot_0x10002c', 'a13_nop_alias_slot_0x100038']),
]
spec = importlib.util.spec_from_file_location('fixture_elf', ROOT / 'tools/ps2recomp_fixture_elf.py')
fx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fx)
spec = importlib.util.spec_from_file_location('strict', ROOT / 'tools/ps2recomp_strict_parser.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


def dest_mask(words):
    mask = 0
    for w in words:
        op, rt, rd = w >> 26, w >> 16 & 31, w >> 11 & 31
        mask |= 1 << (rt if 9 <= op <= 15 else rd) if w else 0
    return mask & ~1  # $zero is never a changeable destination


for fixture, elf_set, names, bound in [(f, e, n, b) for f, e, n in FIXTURES for b in (False, True)]:
    print('== fixture', fixture, 'ELF-bound' if bound else 'source-only', flush=True)
    with tempfile.TemporaryDirectory(prefix='gekko-a11-') as tmp:
        d = Path(tmp)
        gen = d / 'generated'
        (gen / 'core/ee').mkdir(parents=True)
        source = (ROOT / 'tests/fixtures' / fixture / 'ps2_recompiled_functions.cpp').read_text()
        results = [p.parse_function(source, n) for n in names]
        if bound:  # A14: words (and elided NOP slots) come from the verified ELF
            elf = d / 'fixture.elf'
            elf.write_bytes(fx.build(fx.SETS[elf_set]))
            results = [p.bind_to_elf(r, elf) for r in results]
        report = p.lower(results, gen / 'ee_aot_generated.inc', gen / 'core/ee/ee_aot_complex_generated.inc')
        (gen / 'ee_aot.c').write_text((ROOT / 'source/core/recompiler/ee_aot.c').read_text())
        rows = []
        ram_by_start = {start: words for _, start, words in getattr(fx, 'SETS')[elf_set]}
        for r in results:
            ram = ram_by_start[r['start']]
            assert len(ram) == len(r['words'])
            # The parsed words are the source of truth; the ELF words are what the
            # guest really contains. They may differ only in an inferred NOP slot.
            diff = [i for i, (a, b) in enumerate(zip(ram, r['words'])) if a != b]
            assert not diff or (r['inferred_nop_slot'] and diff == [len(ram) - 1]), (r['name'], diff)
            assert not bound or not diff
            pad = lambda ws: ','.join(f'UINT32_C(0x{w:08x})' for w in ws) + ',' + ','.join(['0'] * (32 - len(ws)))
            rows.append(f'{{UINT32_C(0x{r["start"]:08x}),{len(r["words"])}u,UINT32_C(0x{dest_mask(r["words"]):x}),'
                        f'{int(r["terminal"] == "jr_ra")}u,{{{pad(r["words"])}}},{{{pad(ram)}}}}}')
        (gen / 'a11_fixtures.h').write_text(
            '#include <stdint.h>\ntypedef struct{uint32_t pc;unsigned count;uint32_t dest_mask;unsigned jr_ra;uint32_t words[32];uint32_t ram[32];}a11_fixture;\n'
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
