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
        lw = [(0x8c280000, 'lw $t0, 0($at)', 'SET_GPR_S32(ctx, 8, (int32_t)READ32(GPR_U32(ctx, 1)));')]
        self.rejects(render('f_0x1000', 0x1000, lw))
        sw = [(0xac280000, 'sw $t0, 0($at)', 'WRITE32(GPR_U32(ctx, 1), GPR_U32(ctx, 8));')]
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
                     0x0109000b, 0x68000000, 0x50000000):
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


GOLDEN13 = (ROOT / 'tests/fixtures/ps2recomp_a13/ps2_recompiled_functions.cpp').read_text()
A13 = ['a13_ret_nop_0x100000', 'a13_ret_slot_0x100010', 'a13_ret_only_0x10001c',
       'a13_plain_hex_0x100024', 'a13_ra_slot_0x10002c', 'a13_nop_alias_slot_0x100038']


def span(text, name):
    lines = text.split('\n')
    first = [i for i, l in enumerate(lines) if l.startswith(f'void {name}(')][0]
    return lines, first, lines.index('}', first)


class A13Tests(unittest.TestCase):
    def test_provenance_and_builder(self):
        prov = json.loads((ROOT / 'tests/fixtures/ps2recomp_a13/PROVENANCE.json').read_text())
        self.assertEqual(prov['output']['sha256'], hashlib.sha256(GOLDEN13.encode()).hexdigest())
        self.assertEqual(prov['input_elf']['sha256'], hashlib.sha256(fx.build(fx.FUNCTIONS_A13)).hexdigest())

    def test_golden_jr_ra_functions(self):
        expect_inferred = {'a13_ret_nop_0x100000': True, 'a13_nop_alias_slot_0x100038': True}
        for (name, start, words), key in zip(fx.FUNCTIONS_A13, A13):
            r = p.parse_function(GOLDEN13, key)
            inferred = expect_inferred.get(key, False)
            self.assertEqual(r['inferred_nop_slot'], inferred, key)
            self.assertEqual(r['terminal'], None if 'plain' in key else 'jr_ra')
            self.assertEqual((r['start'], r['end']), (start, start + 4 * len(words)))
            want = words[:-1] + [0] if inferred else words
            self.assertEqual(r['words'], want, key)

    def test_uppercase_hex_only_after_control_flow(self):
        # `ctx->pc = 0x10002cu;` after straight-line code is lowercase; the
        # same value uppercased is rejected, and vice versa for JR functions.
        self.assertEqual(p.parse_function(GOLDEN13, 'a13_plain_hex_0x100024')['end'], 0x10002c)
        lines, first, close = span(GOLDEN13, 'a13_plain_hex_0x100024')
        bad = lines[:]
        bad[close - 1] = bad[close - 1].replace('0x10002cu', '0x10002Cu')
        with self.assertRaises(p.ParseError):
            p.parse_function('\n'.join(bad), 'a13_plain_hex_0x100024')
        lines, first, close = span(GOLDEN13, 'a13_ret_only_0x10001c')
        bad = lines[:]
        bad[close - 1] = bad[close - 1].replace('ctx->pc = 0x100024u', 'ctx->pc  =   0x100024u')  # whitespace only: accepted
        p.parse_function('\n'.join(bad), 'a13_ret_only_0x10001c')
        bad[close - 1] = 'ctx->pc = 0x100025u;'
        with self.assertRaises(p.ParseError):
            p.parse_function('\n'.join(bad), 'a13_ret_only_0x10001c')

    def test_exhaustive_single_token_mutation(self):
        import re
        total = 0
        for name in A13:
            lines, first, close = span(GOLDEN13, name)
            for i in range(first, close):
                line = lines[i]
                if line.lstrip().startswith('// Function:'):
                    continue
                for m in re.finditer(r'0x[0-9A-Fa-f]+|(?<![A-Za-z_0-9])\d+(?![A-Za-z_0-9])', line):
                    if line.lstrip().startswith('// 0x') and m.start() > line.index(':', 5) + 14 + 2:
                        continue  # free-text disassembly operands are not interpreted
                    token = m.group()
                    if token.startswith('0x'):
                        digit = token[-1]
                        repl = token[:-1] + ('0' if digit != '0' else '1')
                    else:
                        repl = str(int(token) + 1)
                    mutated = lines[:]
                    mutated[i] = line[:m.start()] + repl + line[m.end():]
                    total += 1
                    try:
                        p.parse_function('\n'.join(mutated), name)
                    except p.ParseError:
                        continue
                    self.fail(f'mutation accepted in {name}: {line.strip()} -> {mutated[i].strip()}')
        self.assertGreater(total, 100)

    def test_jr_block_tampering_rejected(self):
        name = 'a13_ret_slot_0x100010'
        lines, first, close = span(GOLDEN13, name)
        body = lines[first:close + 1]
        edits = [
            ('GPR_U32(ctx, 31)', 'GPR_U32(ctx, 30)'),
            ('const uint32_t jumpTarget', 'uint32_t jumpTarget'),
            ('ctx->in_delay_slot = true;', 'ctx->in_delay_slot = false;'),
            ('ctx->in_delay_slot = false;', 'ctx->in_delay_slot = true;'),
            ('        #else', '        #elif 1'),
            ('        return;\n        #else', '        #else'),
            ('PS2Runtime::GuestBranchKind::Return', 'PS2Runtime::GuestBranchKind::Call'),
            ('"JR $ra"', '"JR $t0"'),
            ('(void)runtime->dispatchGuestBranch', 'runtime->dispatchGuestBranch'),
            ('ctx->branch_pc = 0x100014u;', 'ctx->branch_pc = 0x100010u;'),
            ('ctx->pc = 0x100018u;', 'ctx->pc = 0x10001Cu;'),
            ('(Delay Slot)', ''),
            ('    }\n    ctx->pc = 0x10001Cu;', '    }\n    ctx->pc = 0x10001Cu;\n    ctx->pc = 0x10001Cu;'),
        ]
        text = '\n'.join(lines[first - 2:close + 1]) + '\n'
        p.parse_function(text, name)
        for old, new in edits:
            self.assertIn(old, text)
            with self.assertRaises(p.ParseError, msg=old):
                p.parse_function(text.replace(old, new, 1), name)
        # Line-wise removals, insertions and swaps inside the function.
        fn = lines[first - 2:close + 1]
        for i in range(3, len(fn) - 1):
            if fn[i].strip() == '':
                continue
            for variant in (fn[:i] + fn[i + 1:], fn[:i] + ['    int x = 0;'] + fn[i:]):
                with self.assertRaises(p.ParseError, msg=fn[i]):
                    p.parse_function('\n'.join(variant) + '\n', name)

    def test_slot_statements(self):
        name = 'a13_ret_slot_0x100010'
        text = GOLDEN13
        slot = '(int32_t)ADD32(GPR_U32(ctx, 2), 1)'
        self.assertIn(slot, text)
        for new in ('(int32_t)ADD32(GPR_U32(ctx, 2), 2)', '(int32_t)ADD32(GPR_U32(ctx, 3), 1)'):
            with self.assertRaises(p.ParseError):
                p.parse_function(text.replace(slot, new, 1), name)
        # A decoded NOP slot can never appear (upstream elides it).
        lines, first, close = span(text, name)
        mutated = '\n'.join(lines).replace('// 0x100018: 0x24420001  addiu       $v0, $v0, 0x1 (Delay Slot)',
                                          '// 0x100018: 0x0  nop (Delay Slot)')
        with self.assertRaises(p.ParseError):
            p.parse_function(mutated, name)

    def test_other_control_flow_rejected(self):
        # JR to another register, JALR, J, JAL, branches share the JR template shape but not the word.
        jr_block = [l for l in span(GOLDEN13, 'a13_ret_only_0x10001c')[0][
            span(GOLDEN13, 'a13_ret_only_0x10001c')[1]:span(GOLDEN13, 'a13_ret_only_0x10001c')[2] + 1]]
        text = '\n'.join(jr_block)
        for word, disasm in ((0x01000008, 'jr $t0'), (0x0100f809, 'jalr $t0'), (0x08000400, 'j 0x1000'),
                             (0x0c000400, 'jal 0x1000'), (0x10000001, 'b 0x1008'), (0x04010001, 'bgez $zero')):
            mutated = text.replace('0x3e00008  jr          $ra', f'0x{word:x}  {disasm}')
            with self.assertRaises(p.ParseError, msg=disasm):
                p.parse_function(mutated, 'a13_ret_only_0x10001c')

    def test_range_constraints(self):
        name = 'a13_ret_nop_0x100000'
        text = GOLDEN13
        for old, new in (('0x100000 - 0x100010', '0x100000 - 0x100014'), ('0x100000 - 0x100010', '0x100000 - 0x10000c'),
                         ('ctx->pc = 0x100010u;\n}\n\n\n// Function: a13_ret_slot', 'ctx->pc = 0x100014u;\n}\n\n\n// Function: a13_ret_slot')):
            if old in text:
                with self.assertRaises(p.ParseError, msg=old):
                    p.parse_function(text.replace(old, new, 1), name)

    def test_limit_32_including_slot(self):
        nops = [(0x24040001, 'addiu $a0, $zero, 0x1', 'SET_GPR_S32(ctx, 4, (int32_t)ADD32(GPR_U32(ctx, 0), 1));')]
        for count, ok in ((30, True), (31, False)):
            lines = [f'// Function: g_0x1000', f'// Address: 0x1000 - 0x{0x1000 + 4 * (count + 2):x}',
                     'void g_0x1000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {', '    ctx->pc = 0x1000u;']
            for i in range(count):
                a = 0x1000 + 4 * i
                lines += [f'    // 0x{a:x}: 0x24040001  addiu $a0, $zero, 0x1', f'    ctx->pc = 0x{a:x}u;', f'    {nops[0][2]}']
            j = 0x1000 + 4 * count
            J, S, E = f'0x{j:X}', f'0x{j + 4:X}', f'0x{j + 8:X}'
            block = [f'    // 0x{j:x}: 0x3e00008  jr          $ra', f'    ctx->pc = {J}u;', '    {',
                     '        const uint32_t jumpTarget = GPR_U32(ctx, 31);',
                     f'        ctx->pc = {S}u;', '        ctx->in_delay_slot = true;', f'        ctx->branch_pc = {J}u;',
                     f'        // 0x{j + 4:x}: 0x24040001  addiu $a0, $zero, 0x1 (Delay Slot)', f'        {nops[0][2]}',
                     '        ctx->in_delay_slot = false;', '        ctx->pc = jumpTarget;',
                     '        #if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS',
                     f'        (void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, {J}u, 0u, PS2Runtime::GuestBranchKind::Return, "JR $ra");',
                     '        return;', '        #else', '        ctx->pc = jumpTarget;', '        return;', '        #endif',
                     '    }', f'    ctx->pc = {E}u;']
            lines += block + ['}', '']
            text = '\n'.join(lines)
            if ok:
                self.assertEqual(len(p.parse_function(text, 'g_0x1000')['words']), 32)
            else:
                with self.assertRaises(p.ParseError):
                    p.parse_function(text, 'g_0x1000')

    def test_lowering_jr_block_words(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / 'x.inc'
            r = p.parse_function(GOLDEN13, 'a13_ret_slot_0x100010')
            report = p.lower(r, out)
            self.assertEqual(report['functions'][0]['instructions'], 3)
            self.assertIn('st->next_pc=target', out.read_text())


class A14Tests(unittest.TestCase):
    """ELF identity binding and the private build directory (Batch A14)."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='gekko-a14-')
        self.dir = Path(self.tmp.name)
        self.elf = self.dir / 'fixture.elf'
        self.elf.write_bytes(fx.build(fx.FUNCTIONS_A13))
        self.fixture = ROOT / 'tests/fixtures/ps2recomp_a13/ps2_recompiled_functions.cpp'

    def tearDown(self):
        self.tmp.cleanup()

    def bound(self, name, elf=None):
        return p.bind_to_elf(p.parse_function(GOLDEN13, name), elf or self.elf)

    def test_bind_all_a13_functions(self):
        sets = {'a11': (GOLDEN, [LEAF, ZERO], fx.FUNCTIONS), 'a12': (GOLDEN12, A12, fx.FUNCTIONS_A12),
                'a13': (GOLDEN13, A13, fx.FUNCTIONS_A13)}
        for key, (text, names, functions) in sets.items():
            elf = self.dir / f'{key}.elf'
            elf.write_bytes(fx.build(functions))
            for name, (_, start, words) in zip(names, functions):
                b = p.bind_to_elf(p.parse_function(text, name), elf)
                self.assertEqual(b['words'], words)  # the ELF words, including NOP-alias slots
                self.assertTrue(b['elf_symbol_checked'])
                self.assertEqual(b['elf_sha256'], hashlib.sha256(elf.read_bytes()).hexdigest())
                self.assertFalse(b['inferred_nop_slot'])

    def test_elided_slot_takes_real_elf_word(self):
        self.assertEqual(self.bound('a13_nop_alias_slot_0x100038')['words'][-1], 0x24000000)
        self.assertEqual(self.bound('a13_nop_alias_slot_0x100038')['slot_source'], 'elf')
        self.assertEqual(self.bound('a13_ret_slot_0x100010')['slot_source'], 'source')

    def patched(self, offset_of, new):
        data = bytearray(self.elf.read_bytes())
        i = offset_of(data)
        data[i:i + len(new)] = new
        out = self.dir / 'patched.elf'
        out.write_bytes(data)
        return out

    def test_any_changed_text_word_rejected(self):
        text_off = 84
        for name, (_, start, words) in zip(A13, fx.FUNCTIONS_A13):
            first = (start - 0x100000) // 4
            for k in range(len(words)):
                data = bytearray(self.elf.read_bytes())
                o = text_off + 4 * (first + k)
                data[o] ^= 0x04  # flip one bit of the word
                bad = self.dir / 'bad.elf'
                bad.write_bytes(data)
                # An elided slot that stays a guest NOP is the only legal change.
                if name == 'a13_ret_nop_0x100000' and k == 3 or name == 'a13_nop_alias_slot_0x100038' and k == 2:
                    continue
                with self.assertRaises(p.ParseError, msg=f'{name}[{k}]'):
                    self.bound(name, bad)

    def test_elided_slot_must_be_guest_nop(self):
        for word in (0x24020001, 0x00200000 + 1, 0x24000001, 0x01090021, 0x3c000001):
            data = bytearray(self.elf.read_bytes())
            struct_off = 84 + 4 * 3  # a13_ret_nop slot
            data[struct_off:struct_off + 4] = word.to_bytes(4, 'little')
            bad = self.dir / 'bad.elf'
            bad.write_bytes(data)
            with self.assertRaises(p.ParseError):
                self.bound('a13_ret_nop_0x100000', bad)
        # Every upstream-recognised NOP form is accepted: rs bits ignored.
        for word in (0, 0x00200000, 0x24000000):
            data = bytearray(self.elf.read_bytes())
            data[84 + 12:84 + 16] = word.to_bytes(4, 'little')
            ok = self.dir / 'ok.elf'
            ok.write_bytes(data)
            self.assertEqual(self.bound('a13_ret_nop_0x100000', ok)['words'][-1], word)

    def test_symbol_size_and_identity(self):
        import struct as st
        bad = self.patched(lambda d: d.index(st.pack('<II', 0x100000, 16)) + 4, st.pack('<I', 12))
        with self.assertRaises(p.ParseError):
            self.bound('a13_ret_nop_0x100000', bad)

    def test_elf_structure_rejected(self):
        data = self.elf.read_bytes()
        cases = {'not elf': b'hello', 'truncated': data[:60], 'ET_DYN': data[:16] + b'\x03\x00' + data[18:],
                 'big endian': data[:5] + b'\x02' + data[6:]}
        for label, blob in cases.items():
            bad = self.dir / 'bad.elf'
            bad.write_bytes(blob)
            with self.assertRaises(p.ParseError, msg=label):
                self.bound('a13_ret_nop_0x100000', bad)
        with self.assertRaises(OSError):
            self.bound('a13_ret_nop_0x100000', self.dir / 'missing.elf')

    def test_function_outside_segment_rejected(self):
        r = p.parse_function(GOLDEN13, 'a13_ret_nop_0x100000')
        moved = dict(r, start=0x200000, end=0x200010)
        with self.assertRaises(p.ParseError):
            p.bind_to_elf(moved, self.elf)
        straddle = dict(r, end=0x100100)
        with self.assertRaises(p.ParseError):
            p.bind_to_elf(straddle, self.elf)

    def test_private_build_directory(self):
        results = [self.bound(n) for n in A13]
        with self.assertRaises(p.ParseError):
            p.write_private_build(results, ROOT / 'validation/should-not-exist')
        self.assertFalse((ROOT / 'validation/should-not-exist').exists())
        out = self.dir / 'private'
        report = p.write_private_build(results, out)
        self.assertEqual(report['functions'][0]['elf_sha256'], hashlib.sha256(self.elf.read_bytes()).hexdigest())
        self.assertFalse(report['wii_verified'])
        mode = lambda path: path.stat().st_mode & 0o777
        self.assertEqual(mode(out), 0o700)
        for f in out.rglob('*'):
            self.assertEqual(mode(f), 0o700 if f.is_dir() else 0o600, f)
        self.assertTrue((out / 'core/ee/ee_aot_complex_generated.inc').exists())
        self.assertTrue((out / 'ee_aot_generated.inc').read_text().count('static unsigned ee_aot_') == len(A13))
        with self.assertRaises(OSError):  # never overwrite an existing build directory
            p.write_private_build(results, out)

    def test_cli_private_pipeline(self):
        cli = [sys.executable, str(ROOT / 'tools/ps2recomp_strict_parser.py'), '--source', str(self.fixture)]
        names = sum((['--function', n] for n in A13), [])
        r = subprocess.run(cli + names + ['--output-dir', str(self.dir / 'x')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 2)  # no --elf
        self.assertFalse((self.dir / 'x').exists())
        r = subprocess.run(cli + names + ['--elf', str(self.elf), '--output-dir', str(self.dir / 'x')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        summary = json.loads(r.stdout)
        self.assertEqual(summary['functions'], 6)
        self.assertNotIn('0x24020007', r.stdout)  # no guest opcodes on stdout for private builds
        bad = bytearray(self.elf.read_bytes())
        bad[84] ^= 1
        (self.dir / 'bad.elf').write_bytes(bad)
        r = subprocess.run(cli + names + ['--elf', str(self.dir / 'bad.elf'), '--output-dir', str(self.dir / 'y')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2)
        self.assertFalse((self.dir / 'y').exists())


GOLDEN15 = (ROOT / 'tests/fixtures/ps2recomp_a15/ps2_recompiled_functions.cpp').read_text()
A15 = ['a15_lw_sw_0x100000', 'a15_alias_0x100020', 'a15_ret_store_slot_0x10002c', 'a15_ret_load_slot_0x100038']


class A15Tests(unittest.TestCase):
    def test_provenance_and_builder(self):
        prov = json.loads((ROOT / 'tests/fixtures/ps2recomp_a15/PROVENANCE.json').read_text())
        self.assertEqual(prov['output']['sha256'], hashlib.sha256(GOLDEN15.encode()).hexdigest())
        self.assertEqual(prov['input_elf']['sha256'], hashlib.sha256(fx.build(fx.FUNCTIONS_A15)).hexdigest())

    def test_golden_word_load_store_functions(self):
        for (name, start, words), key in zip(fx.FUNCTIONS_A15, A15):
            r = p.parse_function(GOLDEN15, key)
            self.assertEqual((r['start'], r['words']), (start, words))

    def test_real_upstream_constant_folded_form_is_rejected(self):
        # Real generator output for lui + lw/sw: FAST_READ32 / FAST_WRITE32 with a literal
        # address and trace call. A different shape from the audited unhinted form.
        self.assertIn('FAST_READ32(0x80300000u)', GOLDEN15)
        with self.assertRaises(p.ParseError) as ctx:
            p.parse_function(GOLDEN15, 'a15_hinted_0x100040')
        self.assertIn('0x100044', str(ctx.exception))

    def test_memory_template_variants_rejected(self):
        name = 'a15_lw_sw_0x100000'
        edits = [('READ32(ADD32(GPR_U32(ctx, 4), 0))', 'READ16(ADD32(GPR_U32(ctx, 4), 0))'),
                 ('(int32_t)READ32(ADD32(GPR_U32(ctx, 4), 0))', '(uint32_t)READ32(ADD32(GPR_U32(ctx, 4), 0))'),
                 ('READ32(ADD32(GPR_U32(ctx, 4), 0))', 'FAST_READ32(ADD32(GPR_U32(ctx, 4), 0))'),
                 ('READ32(ADD32(GPR_U32(ctx, 4), 0))', 'runtime->Load32(rdram, ctx, ADD32(GPR_U32(ctx, 4), 0))'),
                 ('WRITE32(ADD32(GPR_U32(ctx, 4), 8), GPR_U32(ctx, 10));', 'WRITE16(ADD32(GPR_U32(ctx, 4), 8), GPR_U32(ctx, 10));'),
                 ('WRITE32(ADD32(GPR_U32(ctx, 4), 8), GPR_U32(ctx, 10));', 'WRITE32(ADD32(GPR_U32(ctx, 10), 8), GPR_U32(ctx, 4));'),
                 ('WRITE32(ADD32(GPR_U32(ctx, 4), 8), GPR_U32(ctx, 10));', 'runtime->Store32(rdram, ctx, ADD32(GPR_U32(ctx, 4), 8), GPR_U32(ctx, 10));'),
                 ('ADD32(GPR_U32(ctx, 4), 4294967292)', 'ADD32(GPR_U32(ctx, 4), -4)'),
                 ('ADD32(GPR_U32(ctx, 4), 32764)', 'ADD32(GPR_U32(ctx, 4), 32765)'),
                 ('WRITE32(ADD32(GPR_U32(ctx, 4), 16), GPR_U32(ctx, 0));', 'WRITE32(ADD32(GPR_U32(ctx, 4), 16), 0);')]
        for old, new in edits:
            self.assertIn(old, GOLDEN15, old)
            with self.assertRaises(p.ParseError, msg=old):
                p.parse_function(GOLDEN15.replace(old, new, 1), name)

    def test_other_memory_opcodes_rejected(self):
        # lwl lwr lwu swl swr ld sd lq sq lwc1 swc1 ldl ldr: no template yet (byte/half are A16).
        for op in (0x22, 0x26, 0x27, 0x2a, 0x2e, 0x37, 0x3f, 0x1e, 0x1f,
                   0x31, 0x39, 0x1a, 0x1b):
            word = op << 26 | 4 << 21 | 8 << 16
            r = render('f_0x1000', 0x1000, [(word, 'x', 'SET_GPR_S32(ctx, 8, (int32_t)READ32(ADD32(GPR_U32(ctx, 4), 0)));')])
            with self.assertRaises(p.ParseError, msg=hex(op)):
                parse_text(r)

    def test_exhaustive_number_mutation_memory_functions(self):
        import re
        total = 0
        for name in A15:
            lines, first, close = span(GOLDEN15, name)
            for i in range(first, close):
                line = lines[i]
                if line.lstrip().startswith('// Function:'):
                    continue
                for m in re.finditer(r'0x[0-9A-Fa-f]+|(?<![A-Za-z_0-9])\d+(?![A-Za-z_0-9])', line):
                    if line.lstrip().startswith('// 0x') and m.start() > line.index(':', 5) + 14 + 2:
                        continue
                    token = m.group()
                    repl = token[:-1] + ('0' if token[-1] != '0' else '1') if token.startswith('0x') else str(int(token) + 1)
                    mutated = lines[:]
                    mutated[i] = line[:m.start()] + repl + line[m.end():]
                    total += 1
                    try:
                        p.parse_function('\n'.join(mutated), name)
                    except p.ParseError:
                        continue
                    self.fail(f'mutation accepted in {name}: {line.strip()} -> {mutated[i].strip()}')
        self.assertGreater(total, 60)

    def test_elf_binding_and_delay_slot_memory(self):
        elf = Path(tempfile.mkdtemp()) / 'a15.elf'
        elf.write_bytes(fx.build(fx.FUNCTIONS_A15))
        for (name, start, words), key in zip(fx.FUNCTIONS_A15, A15):
            self.assertEqual(p.bind_to_elf(p.parse_function(GOLDEN15, key), elf)['words'], words)
        slot = p.parse_function(GOLDEN15, 'a15_ret_store_slot_0x10002c')
        self.assertEqual((slot['terminal'], slot['inferred_nop_slot'], slot['words'][-1] >> 26), ('jr_ra', False, 0x2b))


GOLDEN16 = (ROOT / 'tests/fixtures/ps2recomp_a16/ps2_recompiled_functions.cpp').read_text()
A16 = ['a16_byte_0x100000', 'a16_half_0x100020', 'a16_alias_0x100040', 'a16_ret_store_slot_0x100050',
       'a16_ret_load_slot_0x10005c']


class A16Tests(unittest.TestCase):
    def test_provenance_and_builder(self):
        prov = json.loads((ROOT / 'tests/fixtures/ps2recomp_a16/PROVENANCE.json').read_text())
        self.assertEqual(prov['output']['sha256'], hashlib.sha256(GOLDEN16.encode()).hexdigest())
        self.assertEqual(prov['input_elf']['sha256'], hashlib.sha256(fx.build(fx.FUNCTIONS_A16)).hexdigest())

    def test_golden_byte_half_functions(self):
        for (name, start, words), key in zip(fx.FUNCTIONS_A16, A16):
            r = p.parse_function(GOLDEN16, key)
            self.assertEqual((r['start'], r['words']), (start, words))
        ops = {w >> 26 for _, _, ws in fx.FUNCTIONS_A16 for w in ws}
        self.assertTrue({0x20, 0x21, 0x24, 0x25, 0x28, 0x29} <= ops)

    def test_constant_folded_byte_half_forms_rejected(self):
        self.assertIn('FAST_READ8(0x80300000u)', GOLDEN16)
        self.assertIn('FAST_WRITE16(0x80300002u, _value)', GOLDEN16)
        with self.assertRaises(p.ParseError):
            p.parse_function(GOLDEN16, 'a16_hinted_0x100064')

    def test_width_sign_and_cast_confusion_rejected(self):
        name = 'a16_byte_0x100000'
        edits = [('(int8_t)READ8(ADD32(GPR_U32(ctx, 4), 0))', '(uint8_t)READ8(ADD32(GPR_U32(ctx, 4), 0))'),
                 ('SET_GPR_S32(ctx, 8, (int8_t)READ8', 'SET_GPR_ZE32(ctx, 8, (int8_t)READ8'),
                 ('SET_GPR_ZE32(ctx, 9, (uint8_t)READ8', 'SET_GPR_S32(ctx, 9, (uint8_t)READ8'),
                 ('(int8_t)READ8(ADD32(GPR_U32(ctx, 4), 0))', '(int8_t)READ16(ADD32(GPR_U32(ctx, 4), 0))'),
                 ('WRITE8(ADD32(GPR_U32(ctx, 4), 2), (uint8_t)GPR_U32(ctx, 8));', 'WRITE16(ADD32(GPR_U32(ctx, 4), 2), (uint8_t)GPR_U32(ctx, 8));'),
                 ('WRITE8(ADD32(GPR_U32(ctx, 4), 2), (uint8_t)GPR_U32(ctx, 8));', 'WRITE8(ADD32(GPR_U32(ctx, 4), 2), GPR_U32(ctx, 8));'),
                 ('WRITE8(ADD32(GPR_U32(ctx, 4), 2), (uint8_t)GPR_U32(ctx, 8));', 'WRITE8(ADD32(GPR_U32(ctx, 4), 2), (uint16_t)GPR_U32(ctx, 8));'),
                 ('ADD32(GPR_U32(ctx, 4), 4294967295)', 'ADD32(GPR_U32(ctx, 4), -1)')]
        for old, new in edits:
            self.assertIn(old, GOLDEN16, old)
            with self.assertRaises(p.ParseError, msg=old):
                p.parse_function(GOLDEN16.replace(old, new, 1), name)
        name = 'a16_half_0x100020'
        for old, new in (('(int16_t)READ16(ADD32(GPR_U32(ctx, 4), 0))', '(int16_t)READ8(ADD32(GPR_U32(ctx, 4), 0))'),
                         ('WRITE16(ADD32(GPR_U32(ctx, 4), 4), (uint16_t)GPR_U32(ctx, 8));', 'WRITE8(ADD32(GPR_U32(ctx, 4), 4), (uint16_t)GPR_U32(ctx, 8));'),
                         ('SET_GPR_ZE32(ctx, 9, (uint16_t)READ16', 'SET_GPR_ZE32(ctx, 9, (uint32_t)READ16')):
            self.assertIn(old, GOLDEN16, old)
            with self.assertRaises(p.ParseError, msg=old):
                p.parse_function(GOLDEN16.replace(old, new, 1), name)

    def test_opcode_swaps_rejected(self):
        # The raw word decides the template: swap signedness/width in the *word* but keep the text.
        name = 'a16_byte_0x100000'
        for old, new in (('0x80880000', '0x84880000'), ('0x90890001', '0x94890001'), ('0xa0880002', '0xa4880002')):
            lines, first, close = span(GOLDEN16, name)
            text = '\n'.join(lines)
            word = old[2:].lstrip('0')
            if word not in text:
                continue
            mutated = text.replace(word, new[2:], 1)
            with self.assertRaises(p.ParseError):
                p.parse_function(mutated, name)

    def test_exhaustive_number_mutation_byte_half(self):
        import re
        total = 0
        for name in A16:
            lines, first, close = span(GOLDEN16, name)
            for i in range(first, close):
                line = lines[i]
                if line.lstrip().startswith('// Function:'):
                    continue
                for m in re.finditer(r'0x[0-9A-Fa-f]+|(?<![A-Za-z_0-9])\d+(?![A-Za-z_0-9])', line):
                    if line.lstrip().startswith('// 0x') and m.start() > line.index(':', 5) + 14 + 2:
                        continue
                    token = m.group()
                    repl = token[:-1] + ('0' if token[-1] != '0' else '1') if token.startswith('0x') else str(int(token) + 1)
                    mutated = lines[:]
                    mutated[i] = line[:m.start()] + repl + line[m.end():]
                    total += 1
                    try:
                        p.parse_function('\n'.join(mutated), name)
                    except p.ParseError:
                        continue
                    self.fail(f'mutation accepted in {name}: {line.strip()} -> {mutated[i].strip()}')
        self.assertGreater(total, 90)

    def test_elf_binding_and_delay_slots(self):
        elf = Path(tempfile.mkdtemp()) / 'a16.elf'
        elf.write_bytes(fx.build(fx.FUNCTIONS_A16))
        for (name, start, words), key in zip(fx.FUNCTIONS_A16, A16):
            self.assertEqual(p.bind_to_elf(p.parse_function(GOLDEN16, key), elf)['words'], words)
        store_slot = p.parse_function(GOLDEN16, 'a16_ret_store_slot_0x100050')
        load_slot = p.parse_function(GOLDEN16, 'a16_ret_load_slot_0x10005c')
        self.assertEqual((store_slot['words'][-1] >> 26, load_slot['words'][-1] >> 26), (0x28, 0x25))
        self.assertFalse(store_slot['inferred_nop_slot'] or load_slot['inferred_nop_slot'])


GOLDEN17 = (ROOT / 'tests/fixtures/ps2recomp_a17/ps2_recompiled_functions.cpp').read_text()
A17 = ['a17_loop_0x100000', 'a17_skip_slot_0x10001c', 'a17_blez_bgtz_0x100034', 'a17_back_slot_0x100054',
       'a17_uncond_0x100068']
A17_NAMES = ['a17_loop_0x100000', 'a17_skip_slot_0x10001c', 'a17_blez_bgtz_0x100034', 'a17_back_slot_0x100054',
             'a17_uncond_0x100068', 'a17_external_0x10007c']


def fn_text(text, name):
    lines, first, close = span(text, name)
    return lines[first - 2:close + 1]


class A17Tests(unittest.TestCase):
    def test_provenance_and_builder(self):
        prov = json.loads((ROOT / 'tests/fixtures/ps2recomp_a17/PROVENANCE.json').read_text())
        self.assertEqual(prov['output']['sha256'], hashlib.sha256(GOLDEN17.encode()).hexdigest())
        self.assertEqual(prov['input_elf']['sha256'], hashlib.sha256(fx.build(fx.FUNCTIONS_A17)).hexdigest())

    def test_golden_branch_functions(self):
        expect = {'a17_loop_0x100000': ([(0x10000c, 0x100004, True)], [(0x100000, 1), (0x100004, 4), (0x100014, 2)]),
                  'a17_skip_slot_0x10001c': ([(0x10001c, 0x100028, False)], [(0x10001c, 2), (0x100024, 1), (0x100028, 3)]),
                  'a17_blez_bgtz_0x100034': ([(0x100034, 0x100040, False), (0x100040, 0x10004c, False)],
                                             [(0x100034, 2), (0x10003c, 1), (0x100040, 2), (0x100048, 1), (0x10004c, 2)]),
                  'a17_back_slot_0x100054': ([(0x100058, 0x100054, True)], [(0x100054, 3), (0x100060, 2)]),
                  'a17_uncond_0x100068': ([(0x100068, 0x100074, False)], [(0x100068, 2), (0x100070, 1), (0x100074, 2)])}
        for (name, start, words), key in zip(fx.FUNCTIONS_A17, A17_NAMES):
            if key not in expect:
                continue
            r = p.parse_function(GOLDEN17, key)
            self.assertEqual([(b['addr'], b['target'], b['backward']) for b in r['branches']], expect[key][0])
            self.assertEqual([(pc, len(ws)) for pc, ws in r['blocks']], expect[key][1])
            self.assertEqual(r['terminal'], 'jr_ra')
            # Elided NOP slots are inferred as word 0; everything else is the ELF word.
            for i, (a, b) in enumerate(zip(words, r['words'])):
                self.assertTrue(a == b or i in r['inferred_slot_indexes'], (key, i))

    def test_external_branch_rejected_real_output(self):
        self.assertIn('ctx->pc = 0x100140u;\n            return;', GOLDEN17)
        with self.assertRaises(p.ParseError) as ctx:
            p.parse_function(GOLDEN17, 'a17_external_0x10007c')
        self.assertIn('outside the function', str(ctx.exception))

    def test_elf_binding_resolves_every_elided_slot(self):
        elf = Path(tempfile.mkdtemp()) / 'a17.elf'
        elf.write_bytes(fx.build(fx.FUNCTIONS_A17))
        for (name, start, words), key in zip(fx.FUNCTIONS_A17, A17_NAMES):
            if key not in A17:
                continue
            b = p.bind_to_elf(p.parse_function(GOLDEN17, key), elf)
            self.assertEqual(b['words'], words)
            self.assertEqual(sum(len(ws) for _, ws in b['blocks']), len(words))
        bad = bytearray(elf.read_bytes())
        # ELF slot of the first branch in a17_loop (word 4) must be a guest NOP.
        bad[84 + 4 * 4:84 + 4 * 4 + 4] = (0x24020001).to_bytes(4, 'little')
        (elf.parent / 'bad.elf').write_bytes(bad)
        with self.assertRaises(p.ParseError):
            p.bind_to_elf(p.parse_function(GOLDEN17, 'a17_loop_0x100000'), elf.parent / 'bad.elf')

    def test_every_line_edit_rejected(self):
        for key in A17:
            fn = fn_text(GOLDEN17, key)
            for i in range(3, len(fn) - 1):
                if not fn[i].strip():
                    continue
                for variant in (fn[:i] + fn[i + 1:], fn[:i] + ['    int x = 0;'] + fn[i:]):
                    with self.assertRaises(p.ParseError, msg=f'{key}: {fn[i].strip()}'):
                        p.parse_function('\n'.join(variant) + '\n', key)

    def test_exhaustive_number_mutation_branches(self):
        import re
        total = 0
        for key in A17:
            lines, first, close = span(GOLDEN17, key)
            for i in range(first, close):
                line = lines[i]
                if line.lstrip().startswith('// Function:'):
                    continue
                for m in re.finditer(r'0x[0-9A-Fa-f]+|(?<![A-Za-z_0-9])\d+(?![A-Za-z_0-9])', line):
                    if line.lstrip().startswith('// 0x') and m.start() > line.index(':', 5) + 14 + 2:
                        continue
                    token = m.group()
                    repl = token[:-1] + ('0' if token[-1] != '0' else '1') if token.startswith('0x') else str(int(token) + 1)
                    mutated = lines[:]
                    mutated[i] = line[:m.start()] + repl + line[m.end():]
                    total += 1
                    try:
                        p.parse_function('\n'.join(mutated), key)
                    except p.ParseError:
                        continue
                    self.fail(f'mutation accepted in {key}: {line.strip()} -> {mutated[i].strip()}')
        self.assertGreater(total, 150)

    def test_semantic_tamper_rejected(self):
        loop = '\n'.join(fn_text(GOLDEN17, 'a17_loop_0x100000'))
        skip = '\n'.join(fn_text(GOLDEN17, 'a17_skip_slot_0x10001c'))
        blez = '\n'.join(fn_text(GOLDEN17, 'a17_blez_bgtz_0x100034'))
        cases = [
            (loop, 'a17_loop_0x100000', 'GPR_U64(ctx, 8) != GPR_U64(ctx, 0)', 'GPR_U64(ctx, 8) == GPR_U64(ctx, 0)'),
            (loop, 'a17_loop_0x100000', 'if (runtime->eeCheckpointDue()) {', 'if (runtime->other()) {'),
            (loop, 'a17_loop_0x100000', '        case 0x100004u: goto label_100004;\n', ''),
            (loop, 'a17_loop_0x100000', 'goto label_100004;\n        }', 'goto label_100000;\n        }'),
            (loop, 'a17_loop_0x100000', 'const bool branch_taken_0x10000c', 'const bool branch_taken_0x10000d'),
            (skip, 'a17_skip_slot_0x10001c', 'ctx->in_delay_slot = true;', 'ctx->in_delay_slot = false;'),
            (skip, 'a17_skip_slot_0x10001c', 'ctx->branch_pc = 0x10001Cu;', 'ctx->branch_pc = 0x10001cu;'),
            (skip, 'a17_skip_slot_0x10001c', 'ctx->pc = 0x100028u;\n            goto', 'ctx->pc = 0x100028u;\n            if (runtime->eeCheckpointDue()) {\n                return;\n            }\n            goto'),
            (blez, 'a17_blez_bgtz_0x100034', 'GPR_S32(ctx, 4) <= 0', 'GPR_S64(ctx, 4) <= 0'),
            (blez, 'a17_blez_bgtz_0x100034', 'GPR_S32(ctx, 5) > 0', 'GPR_S32(ctx, 5) >= 0'),
        ]
        for text, key, old, new in cases:
            self.assertIn(old, text, old)
            with self.assertRaises(p.ParseError, msg=old):
                p.parse_function(text.replace(old, new, 1) + '\n', key)

    def test_unsupported_branch_forms_rejected(self):
        # likely, REGIMM, link, jump words with plausible-looking text are not accepted.
        for word in (0x50000001, 0x54000001, 0x04010001, 0x04110001, 0x08000400, 0x0c000400, 0x0100f809):
            r = render('f_0x1000', 0x1000, [(word, 'x', 'ctx->pc = 0x1004u;')])
            with self.assertRaises(p.ParseError, msg=hex(word)):
                parse_text(r)
        # rt must be zero for blez/bgtz.
        word = 0x18a40002
        self.assertEqual(word >> 26, 6)
        lines = fn_text(GOLDEN17, 'a17_blez_bgtz_0x100034')
        mutated = '\n'.join(lines).replace('0x18800002', '0x18840002')
        with self.assertRaises(p.ParseError):
            p.parse_function(mutated, 'a17_blez_bgtz_0x100034')

    def test_lowering_emits_one_entry_per_block_and_matches_generator(self):
        gen = importlib.util.spec_from_file_location('gen17', ROOT / 'tools/ee_aot_generate.py')
        g = importlib.util.module_from_spec(gen)
        gen.loader.exec_module(g)
        with tempfile.TemporaryDirectory() as d:
            r = p.parse_function(GOLDEN17, 'a17_blez_bgtz_0x100034')
            out = Path(d) / 'x.inc'
            report = p.lower(r, out)
            self.assertEqual(out.read_text(), g.generate(r['blocks']))
            self.assertEqual(out.read_text().count('static unsigned ee_aot_'), 5)
            self.assertEqual((report['functions'][0]['blocks'], report['functions'][0]['branches']), (5, 2))


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
