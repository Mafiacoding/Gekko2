#!/usr/bin/env python3
"""GPL-3.0-or-later. End-to-end PS2Recomp -> Wii check on the committed fixtures.

For each real-generator fixture: strict-parse, bind to the reproducible
synthetic ELF, write a private build directory, cross-build the Wii ELF with
devkitPPC and run the linked-PPC differential under Unicorn. Needs DEVKITPRO
and `unicorn`. Modelled hardware only; not a Wii/Dolphin run.
"""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


p = load('strict', ROOT / 'tools/ps2recomp_strict_parser.py')
fx = load('fixture_elf', ROOT / 'tools/ps2recomp_fixture_elf.py')
FIXTURES = [
    ('ps2recomp_a11', 'a11', ['a11_int_leaf_0x100000', 'a11_zero_dest_0x100018']),
    ('ps2recomp_a12', 'a12', ['a12_imm_leaf_0x100000', 'a12_reg_leaf_0x100020', 'a12_zero_dest_0x100040']),
    ('ps2recomp_a13', 'a13', ['a13_ret_nop_0x100000', 'a13_ret_slot_0x100010', 'a13_ret_only_0x10001c',
                              'a13_plain_hex_0x100024', 'a13_ra_slot_0x10002c', 'a13_nop_alias_slot_0x100038']),
    ('ps2recomp_a15', 'a15', ['a15_lw_sw_0x100000', 'a15_alias_0x100020', 'a15_ret_store_slot_0x10002c',
                              'a15_ret_load_slot_0x100038']),
    ('ps2recomp_a19', 'a19', ['a19_jfwd_0x100000', 'a19_jnop_0x100018', 'a19_jback_0x100030', 'a19_jback2_0x10004c']),
    ('ps2recomp_a18', 'a18', ['a18_callee_0x100000', 'a18_caller_0x10000c', 'a18_caller2_0x100028',
                              'a18_callee2_0x100040', 'a18_callee3_0x10004c', 'a18_caller3_0x100058']),
    ('ps2recomp_a18', 'a18', ['a18_caller_0x10000c', 'a18_caller2_0x100028', 'a18_caller3_0x100058']),
    ('ps2recomp_a17', 'a17', ['a17_loop_0x100000', 'a17_skip_slot_0x10001c', 'a17_blez_bgtz_0x100034',
                              'a17_back_slot_0x100054', 'a17_uncond_0x100068']),
    ('ps2recomp_a16', 'a16', ['a16_byte_0x100000', 'a16_half_0x100020', 'a16_alias_0x100040',
                              'a16_ret_store_slot_0x100050', 'a16_ret_load_slot_0x10005c']),
]
devkitpro = Path(os.environ['DEVKITPRO'])
nm = devkitpro / 'devkitPPC/bin/powerpc-eabi-nm'
with tempfile.TemporaryDirectory(prefix='gekko-ppc-') as tmp:
    for index, (fixture, key, names) in enumerate(FIXTURES):
        d = Path(tmp) / f'{fixture}-{index}'
        d.mkdir()
        (d / 'in.elf').write_bytes(fx.build(fx.SETS[key]))
        source = (ROOT / 'tests/fixtures' / fixture / 'ps2_recompiled_functions.cpp').read_text()
        (d / 'image.json').write_text(json.dumps([{'pc': a, 'words': w} for _, a, w in fx.SETS[key]]))
        results = [p.bind_to_elf(p.parse_function(source, n), d / 'in.elf') for n in names]
        out = d / 'private'
        p.write_private_build(results, out)
        subprocess.run(['bash', str(ROOT / 'tools/build_wii_private_aot.sh'), str(out)], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/verify_wii_ps2recomp_aot.py'), '--nm', str(nm),
                        str(out / 'Gekko2-private-aot.elf'), '--functions', str(out / 'functions.private.json'),
                        '--image', str(d / 'image.json')],
                       check=True)
