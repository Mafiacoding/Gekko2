#!/usr/bin/env python3
"""Batch A11: strict generated-C++ parser tests (positive, negative, lowering).

The golden file is the unmodified output of the pinned upstream generator
(see tests/fixtures/ps2recomp_a11/PROVENANCE.json). Mutation tests prove that
anything outside the audited subset is rejected as a whole.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIX = ROOT / 'tests/fixtures/ps2recomp_a11'
spec = importlib.util.spec_from_file_location('strict', ROOT / 'tools/ps2recomp_strict_parser.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
spec = importlib.util.spec_from_file_location('fixture_elf', ROOT / 'tools/ps2recomp_fixture_elf.py')
fx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fx)

GOLDEN = (FIX / 'ps2_recompiled_functions.cpp').read_text()
LEAF = 'a11_int_leaf_0x100000'
ZERO = 'a11_zero_dest_0x100018'
LEAF_WORDS = [0x24081234, 0x2509ffff, 0x01095021, 0x01485826, 0x000b6100, 0]


def render(name, start, items, end=None, preamble=True):
    """Independent hand-written upstream-shaped function (literal statements)."""
    end = start + 4 * len(items) if end is None else end
    out = [f'// Function: {name}', f'// Address: 0x{start:x} - 0x{end:x}',
           f'void {name}(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {{']
    if preamble:
        out += ['#ifdef PS2_FUNCTION_LOG_TRACKER', f'    PS_LOG_ENTRY("{name}");', '#endif', '']
    out += [f'    ctx->pc = 0x{start:x}u;', '']
    for i, (word, disasm, statement) in enumerate(items):
        a = start + 4 * i
        out += [f'    // 0x{a:x}: 0x{word:x}  {disasm}', f'    ctx->pc = 0x{a:x}u;', f'    {statement}']
    out += [f'    ctx->pc = 0x{end:x}u;', '}', '']
    return '\n'.join(out)


ITEMS = [
    (0x24081234, 'addiu $t0, $zero, 0x1234', 'SET_GPR_S32(ctx, 8, (int32_t)ADD32(GPR_U32(ctx, 0), 4660));'),
    (0x2509ffff, 'addiu $t1, $t0, -0x1', 'SET_GPR_S32(ctx, 9, (int32_t)ADD32(GPR_U32(ctx, 8), 4294967295));'),
    (0x01095021, 'addu $t2, $t0, $t1', 'SET_GPR_S32(ctx, 10, (int32_t)ADD32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));'),
    (0x01485826, 'xor $t3, $t2, $t0', 'SET_GPR_U64(ctx, 11, GPR_U64(ctx, 10) ^ GPR_U64(ctx, 8));'),
    (0x000b6100, 'sll $t4, $t3, 4', 'SET_GPR_S32(ctx, 12, (int32_t)SLL32(GPR_U32(ctx, 11), 4));'),
    (0, 'nop', '// NOP'),
]


def parse_text(text, name='f_0x1000'):
    return p.parse_function(text, name)


class GoldenTests(unittest.TestCase):
    def test_provenance_hashes(self):
        prov = json.loads((FIX / 'PROVENANCE.json').read_text())
        self.assertEqual(prov['upstream_commit'], p.UPSTREAM_COMMIT)
        self.assertEqual(prov['output']['sha256'], hashlib.sha256(GOLDEN.encode()).hexdigest())
        self.assertEqual(prov['input_elf']['sha256'], hashlib.sha256(fx.build()).hexdigest())

    def test_golden_leaf(self):
        r = p.parse_function(GOLDEN, LEAF)
        self.assertEqual((r['start'], r['end'], r['words']), (0x100000, 0x100018, LEAF_WORDS))

    def test_golden_zero_dest_and_function_selection(self):
        r = p.parse_function(GOLDEN, ZERO)
        self.assertEqual((r['start'], r['end']), (0x100018, 0x100034))
        self.assertEqual(r['words'][:2], [0x01090021, 0x01090026])  # ADDU/XOR to $zero
        # Selecting one function never leaks the other function's words.
        self.assertNotEqual(r['words'], p.parse_function(GOLDEN, LEAF)['words'])

    def test_golden_words_match_elf_builder(self):
        for name, _, words in fx.FUNCTIONS:
            key = [n for n in (LEAF, ZERO) if n.startswith(name)][0]
            self.assertEqual(p.parse_function(GOLDEN, key)['words'], words)

    def test_each_supported_instruction_and_no_preamble(self):
        for preamble in (True, False):
            r = parse_text(render('f_0x1000', 0x1000, ITEMS, preamble=preamble))
            self.assertEqual(r['words'], LEAF_WORDS)

    def test_whitespace_normalised(self):
        # Runs of blanks/tabs and blank lines collapse; the tokens do not change.
        text = render('f_0x1000', 0x1000, ITEMS).replace('    ', '\t \t').replace(', ', ',  ') + '\n\n\n'
        self.assertEqual(parse_text(text)['words'], LEAF_WORDS)
        # Adding or removing a space between tokens is a semantic text change.
        base = render('f_0x1000', 0x1000, ITEMS)
        for old, new in (('ADD32(', 'ADD32 ('), ('(int32_t)ADD32', '(int32_t) ADD32'), ('ctx, 8,', 'ctx,8,')):
            with self.assertRaises(p.ParseError):
                parse_text(base.replace(old, new, 1))


class RejectTests(unittest.TestCase):
    def rejects(self, text, name='f_0x1000', fragment=None):
        with self.assertRaises(p.ParseError) as ctx:
            parse_text(text, name)
        if fragment:
            self.assertIn(fragment, str(ctx.exception))

    def base(self):
        return render('f_0x1000', 0x1000, ITEMS)

    def test_wrong_signature(self):
        t = self.base()
        for bad in ('void f_0x1000(uint8_t* rdram, R5900Context* ctx) {',
                    'void f_0x1000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime)',
                    'static void f_0x1000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {',
                    'void f_0x1000(uint8_t* rdram, ee_state_t* ctx, PS2Runtime *runtime) {',
                    'void f_0x1000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) { }'):
            self.rejects(t.replace('void f_0x1000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {', bad))

    def test_wrong_or_duplicate_function(self):
        self.rejects(self.base(), 'g_0x1000', 'found 0 times')
        self.rejects(self.base() + self.base(), 'f_0x1000', 'found 2 times')
        self.rejects(self.base(), 'bad name')
        self.rejects(self.base().replace('f_0x1000', 'f_0x2000'), 'f_0x2000', 'suffix')

    def test_missing_comments(self):
        t = self.base()
        self.rejects(t.replace('// Address: 0x1000 - 0x1018\n', ''))
        self.rejects(t.replace('// Function: f_0x1000\n', ''))
        for a in range(6):
            line = f'    // 0x{0x1000 + 4 * a:x}: '
            lines = t.split('\n')
            idx = [i for i, l in enumerate(lines) if l.startswith(line)][0]
            del lines[idx]
            self.rejects('\n'.join(lines))

    def test_pc_gap_and_wrong_pc(self):
        t = self.base()
        self.rejects(t.replace('// 0x1008: ', '// 0x100c: '), fragment='contiguous')
        self.rejects(t.replace('ctx->pc = 0x1008u;', 'ctx->pc = 0x100cu;'), fragment='ctx->pc')
        self.rejects(t.replace('ctx->pc = 0x1000u;\n\n    //', 'ctx->pc = 0x1004u;\n\n    //'))
        self.rejects(t.replace('ctx->pc = 0x1000u;', 'ctx->pc = 0x1001u;'))
        self.rejects(t.replace('ctx->pc = 0x1008u;', 'ctx->pc = 0x01008u;'), fragment='canonical')
        self.rejects(t.replace('// 0x1008: 0x1095021', '// 0x1008: 0x01095021'), fragment='canonical')

    def test_wrong_end_address(self):
        for end in (0x1014, 0x101c, 0x1019):
            self.rejects(render('f_0x1000', 0x1000, ITEMS, end=end))
        t = self.base()
        self.rejects(t.replace('    ctx->pc = 0x1018u;\n}', '    ctx->pc = 0x101cu;\n}'), fragment='fallthrough')
        self.rejects(t.replace('    ctx->pc = 0x1018u;\n}', '}'))
        self.rejects(t.replace('0x1000 - 0x1018', '0x1000 - 0x1000'))
        self.rejects(t.replace('0x1000 - 0x1018', '0x1002 - 0x1018'))

    def test_changed_statements(self):
        t = self.base()
        for old, new in (('SET_GPR_S32(ctx, 8,', 'SET_GPR_S32(ctx, 9,'),
                         ('4660', '4661'),
                         ('4294967295', '-1'),
                         ('GPR_U32(ctx, 8), GPR_U32(ctx, 9)', 'GPR_U32(ctx, 9), GPR_U32(ctx, 8)'),
                         ('^ GPR_U64(ctx, 8)', '| GPR_U64(ctx, 8)'),
                         ('SLL32(GPR_U32(ctx, 11), 4)', 'SLL32(GPR_U32(ctx, 11), 5)'),
                         ('SET_GPR_U64(ctx, 11', 'SET_GPR_S32(ctx, 11'),
                         ('// NOP', '// nop')):
            self.assertIn(old, t)
            self.rejects(t.replace(old, new, 1))

    def test_comment_word_vs_statement_mismatch(self):
        t = self.base()
        self.rejects(t.replace('0x2509ffff', '0x2509fffe'))
        self.rejects(t.replace('addiu $t1', 'addu $t1'))
        self.rejects(t.replace('0x0  nop', '0x1  nop'))

    def test_extra_statements(self):
        t = self.base()
        extras = ['    int x = 0;', '    ctx->pc = 0x1018u;', '    SET_GPR_S32(ctx, 1, 0);',
                  '    (void)runtime;', '    return;', '    ;', '    // stray comment']
        for extra in extras:
            self.rejects(t.replace('    ctx->pc = 0x1018u;\n}', f'{extra}\n    ctx->pc = 0x1018u;\n}}'))
            self.rejects(t.replace('    SET_GPR_S32(ctx, 8,', f'{extra}\n    SET_GPR_S32(ctx, 8,'))
        self.rejects(t.replace('4660));', '4660)); ctx->pc = 1;'))
        self.rejects(t.replace('4660));', '4660)); // MMIO: 0x1000'))
        self.rejects(t.replace('    ctx->pc = 0x1018u;\n}', '    ctx->pc = 0x1018u;\n    x;\n}'))

    def test_branches_and_labels(self):
        t = self.base()
        branch = [(0x10220003, 'beq $at, $v0, 0x1010', 'if (GPR_U32(ctx, 1) == GPR_U32(ctx, 2)) { goto label_1010; }')]
        self.rejects(render('f_0x1000', 0x1000, branch))
        self.rejects(render('f_0x1000', 0x1000, [(0x03e00008, 'jr $ra', 'ctx->pc = GPR_U32(ctx, 31); return;')]))
        self.rejects(render('f_0x1000', 0x1000, [(0x0c000400, 'jal 0x1000', 'f_0x1000(rdram, ctx, runtime);')]))
        self.rejects(t.replace('    // 0x1004: ', 'label_1004:\n    // 0x1004: '))
        self.rejects(t.replace('    ctx->pc = 0x1000u;\n\n', '    switch (ctx->pc) {\n        default: break;\n    }\n    ctx->pc = 0x1000u;\n\n'))
        self.rejects(t.replace('    SET_GPR_S32(ctx, 8,', '    goto label_1004;\n    SET_GPR_S32(ctx, 8,'))

    def test_memory_macros_and_helpers(self):
        lw = [(0x8c280000, 'lw $t0, 0($at)', 'SET_GPR_S32(ctx, 8, (int32_t)READ32(ADD32(GPR_U32(ctx, 1), 0)));')]
        self.rejects(render('f_0x1000', 0x1000, lw))
        sw = [(0xac280000, 'sw $t0, 0($at)', 'WRITE32(ADD32(GPR_U32(ctx, 1), 0), GPR_U32(ctx, 8));')]
        self.rejects(render('f_0x1000', 0x1000, sw))
        sq = [(0x7c280000, 'sq $t0, 0($at)', 'WRITE128(ADD32(GPR_U32(ctx, 1), 0), GPR_VEC(ctx, 8));')]
        self.rejects(render('f_0x1000', 0x1000, sq))
        sc = [(0x0000000c, 'syscall', 'runtime->handleSyscall(rdram, ctx);')]
        self.rejects(render('f_0x1000', 0x1000, sc))
        helper = [(0x24081234, 'addiu $t0, $zero, 0x1234', 'ps2_stub_call(rdram, ctx, runtime);')]
        self.rejects(render('f_0x1000', 0x1000, helper))
        self.rejects(self.base().replace('    SET_GPR_S32(ctx, 8,', '    runtime->SignalException(ctx, 1);\n    SET_GPR_S32(ctx, 8,'))
        self.rejects(self.base().replace('    SET_GPR_S32(ctx, 8,', '    helper(rdram, ctx, runtime);\n    SET_GPR_S32(ctx, 8,'))

    def test_unsupported_instruction_classes(self):
        # COP0 (mfc0), FPU (add.s), VU/COP2 (vadd), MMI (padduw), shifts/ops not yet audited.
        # Even with a plausible-looking statement, unsupported words are rejected
        # by the decoder (fragment check), not merely by a statement mismatch.
        for word in (0x40086000, 0x46020000, 0x4a000028, 0x70411028, 0x00094c02, 0x00094c03,
                     0x00494022, 0x01095020, 0x0109502d, 0x0109502c, 0x01095018, 0x01095010,
                     0x0109000b, 0x68000000, 0x1c000000):
            self.rejects(render('f_0x1000', 0x1000, [(word, 'x', 'SET_GPR_S32(ctx, 8, 0);')]),
                         fragment='outside the supported subset')

    def test_special_cases_rejected(self):
        # ADDIU $zero: upstream emits a distinct NOP comment; SLL $zero: empty statement.
        self.rejects(render('f_0x1000', 0x1000, [(0x24000001, 'addiu $zero, $zero, 0x1', '// NOP (addiu $zero, ...)')]), fragment='ADDIU to $zero')
        self.rejects(render('f_0x1000', 0x1000, [(0x24000001, 'addiu $zero, $zero, 0x1', 'SET_GPR_S32(ctx, 0, (int32_t)ADD32(GPR_U32(ctx, 0), 1));')]), fragment='ADDIU to $zero')
        self.rejects(render('f_0x1000', 0x1000, [(0x00090040, 'sll $zero, $t1, 1', '')]), fragment='SLL to $zero')
        # Non-canonical NOP aliases (nonzero reserved bits) are not accepted.
        self.rejects(render('f_0x1000', 0x1000, [(0x00200000, 'sll $zero, $zero, 0', '// NOP')]))
        self.rejects(render('f_0x1000', 0x1000, [(0x00000040, 'sll $zero, $zero, 1', '// NOP')]))
        # Reserved field must be zero for ADDU/XOR.
        self.rejects(render('f_0x1000', 0x1000, [(0x01095061, 'addu', 'SET_GPR_S32(ctx, 10, (int32_t)ADD32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));')]), fragment='shamt')

    def test_zero_destination_addu_xor_accepted(self):
        items = [(0x01090021, 'addu $zero, $t0, $t1', 'SET_GPR_S32(ctx, 0, (int32_t)ADD32(GPR_U32(ctx, 8), GPR_U32(ctx, 9)));'),
                 (0x01090026, 'xor $zero, $t0, $t1', 'SET_GPR_U64(ctx, 0, GPR_U64(ctx, 8) ^ GPR_U64(ctx, 9));')]
        self.assertEqual(parse_text(render('f_0x1000', 0x1000, items))['words'], [0x01090021, 0x01090026])

    def test_preprocessor(self):
        t = self.base()
        for directive in ('#define PS2_FAST 1', '#if 0', '#ifdef _DEBUG', '#pragma once',
                          '#include "evil.h"', '#else', '#endif', '#undef GPR_U32'):
            self.rejects(t.replace('    ctx->pc = 0x1000u;\n\n', f'{directive}\n    ctx->pc = 0x1000u;\n\n', 1))
            self.rejects(t.replace('    ctx->pc = 0x1018u;\n}', f'{directive}\n    ctx->pc = 0x1018u;\n}}'))
        # Preamble that is not byte-identical to upstream is rejected.
        self.rejects(t.replace('PS_LOG_ENTRY("f_0x1000");', 'PS_LOG_ENTRY("other");'))
        self.rejects(t.replace('PS_LOG_ENTRY("f_0x1000");', 'PS_LOG_ENTRY("f_0x1000"); (void)0;'))
        self.rejects(t.replace('#ifdef PS2_FUNCTION_LOG_TRACKER', '#ifdef PS2_FUNCTION_LOG'))
        self.rejects(t.replace('#endif\n', '', 1))

    def test_hostile_text(self):
        t = self.base()
        self.rejects(t.replace('4660', '4660 /* hidden */'))
        self.rejects(t + '‮')
        self.rejects(t.replace('ctx', 'ctx\0', 1))
        self.rejects(t.replace('4660));', '4660)); \\\n'))
        self.rejects(t.replace('}\n', '', 1)[:-2])

    def test_over_long_function_rejected(self):
        items = [(0, 'nop', '// NOP')] * 33
        self.rejects(render('f_0x1000', 0x1000, items), fragment='block limit')
        self.assertEqual(len(parse_text(render('f_0x1000', 0x1000, items[:32]))['words']), 32)

    def test_cli_failure_leaves_no_output(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            (d / 'bad.cpp').write_text(self.base().replace('4660', '1'))
            out = d / 'out.inc'
            r = subprocess.run([sys.executable, str(ROOT / 'tools/ps2recomp_strict_parser.py'), '--source',
                                str(d / 'bad.cpp'), '--function', 'f_0x1000', '--output', str(out)],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 2)
            self.assertFalse(out.exists())


GOLDEN12 = (ROOT / 'tests/fixtures/ps2recomp_a12/ps2_recompiled_functions.cpp').read_text()
A12 = ['a12_imm_leaf_0x100000', 'a12_reg_leaf_0x100020', 'a12_zero_dest_0x100018'.replace('0018', '0040')]


class A12Tests(unittest.TestCase):
    def test_provenance_and_builder(self):
        prov = json.loads((ROOT / 'tests/fixtures/ps2recomp_a12/PROVENANCE.json').read_text())
        self.assertEqual(prov['output']['sha256'], hashlib.sha256(GOLDEN12.encode()).hexdigest())
        self.assertEqual(prov['input_elf']['sha256'], hashlib.sha256(fx.build(fx.FUNCTIONS_A12)).hexdigest())

    def test_golden_functions_match_builder_words(self):
        for (name, start, words), key in zip(fx.FUNCTIONS_A12, A12):
            r = p.parse_function(GOLDEN12, key)
            self.assertEqual((r['start'], r['words']), (start, words))
            self.assertEqual(r['end'], start + 4 * len(words))

    def test_every_number_in_every_statement_is_checked(self):
        # Exhaustive single-number mutation of both real golden files: the raw
        # word fixes every register/immediate, so any edit must be rejected.
        import re
        for text, names in ((GOLDEN, (LEAF, ZERO)), (GOLDEN12, A12)):
            lines = text.split('\n')
            for name in names:
                parse_ok = p.parse_function(text, name)
                self.assertTrue(parse_ok['words'])
                first = [i for i, l in enumerate(lines) if l.startswith(f'void {name}(')][0]
                last = lines.index('}', first)
                for i in range(first, last):
                    line = lines[i]
                    if not line.lstrip().startswith(('SET_GPR', 'ctx->pc')) or '(' not in line and 'pc' not in line:
                        continue
                    for m in re.finditer(r'(?<![A-Za-z_0-9])\d+(?![A-Za-z_0-9])|(?<=ctx, )\d+', line):
                        mutated = lines[:]
                        mutated[i] = line[:m.start()] + str(int(m.group()) + 1) + line[m.end():]
                        mutated = '\n'.join(mutated)
                        if mutated == text:
                            continue
                        try:
                            r = p.parse_function(mutated, name)
                        except p.ParseError:
                            continue
                        self.fail(f'mutation accepted in {name}: {line.strip()}')

    def test_operator_and_type_confusion_rejected(self):
        t = GOLDEN12
        for old, new in (('| (uint64_t)(uint16_t)65535', '& (uint64_t)(uint16_t)65535'),
                         ('(uint64_t)(uint16_t)65280', '(uint64_t)(int16_t)65280'),
                         ('(int64_t)GPR_S64(ctx, 10) < (int64_t)(int32_t)4294967295',
                          '(uint64_t)GPR_U64(ctx, 10) < (uint64_t)(int64_t)(int32_t)4294967295'),
                         ('~(GPR_U64(ctx, 8) | GPR_U64(ctx, 9))', '(GPR_U64(ctx, 8) | GPR_U64(ctx, 9))'),
                         ('SET_GPR_S32(ctx, 15, (int32_t)SUB32', 'SET_GPR_S32(ctx, 15, (int32_t)ADD32'),
                         ('(int32_t)((uint32_t)32769 << 16)', '(int32_t)((uint32_t)32769 << 15)')):
            self.assertIn(old, t)
            mutated = t.replace(old, new, 1)
            names = [n for n in A12 if p.parse_function(t, n)]
            with self.assertRaises(p.ParseError):
                for n in names:
                    p.parse_function(mutated, n)

    def test_reserved_fields_and_unsupported_neighbours(self):
        # LUI with rs != 0, ADDU-family with shamt != 0, MOVZ/MOVN/MULT/DIV/ADD/SUB
        # (different upstream templates) and a branch-like REGIMM stay rejected.
        for word in (0x3c288001, 0x0109 << 16 | 0x5081, 0x0109500a, 0x0109500b, 0x01090018,
                     0x0109001a, 0x01095020, 0x01095022, 0x04010001, 0x01095018 | 1):
            self.rejects(render('f_0x1000', 0x1000, [(word, 'x', 'SET_GPR_U64(ctx, 8, 0);')]))

    def rejects(self, text):
        with self.assertRaises(p.ParseError):
            parse_text(text)


class LoweringTests(unittest.TestCase):
    def test_lowering_passes_exactly_validated_words(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / 'ee_aot_generated.inc'
            r = p.lower(p.parse_function(GOLDEN, LEAF), out)
            gen = importlib.util.spec_from_file_location('gen', ROOT / 'tools/ee_aot_generate.py')
            g = importlib.util.module_from_spec(gen)
            gen.loader.exec_module(g)
            expected = g.generate([(0x100000, LEAF_WORDS)])
            self.assertEqual(out.read_text(), expected)
            self.assertEqual(r['aot_sha256'], hashlib.sha256(expected.encode()).hexdigest())
            self.assertEqual(r['functions'][0]['instructions'], 6)
            self.assertFalse(r['wii_verified'])
            self.assertFalse(list(Path(d).glob('*.manifest.json')))
            # Table entry is keyed by the verified function start and first word.
            self.assertFalse(list(Path(d).glob('*.tmp')))
            self.assertIn('{UINT32_C(0x00100000), UINT32_C(0x24081234), ee_aot_0}', expected)


if __name__ == '__main__':
    unittest.main()
