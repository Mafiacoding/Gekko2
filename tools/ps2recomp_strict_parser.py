#!/usr/bin/env python3
"""GPL-3.0-or-later. Strict, fail-closed parser for PS2Recomp generated C++.

Batch A11. Accepts exactly one narrow shape emitted by the pinned upstream
generator (ran-j/PS2Recomp e5636981a1c9de368f6b71e819c3dc188dfedbf4): a
straight-line integer function made only of the instructions in I_TYPE,
R_TYPE, ADDIU, LUI, SLL and the canonical NOP. Every statement is reconstructed from the raw MIPS word and
must equal the generated text; anything else rejects the whole function.

The generated C++ is never compiled or executed. Only the validated raw words
are handed to tools/ee_aot_generate.py (Gekko2's own AOT backend).
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

PARSER_VERSION = 1
UPSTREAM_COMMIT = 'e5636981a1c9de368f6b71e819c3dc188dfedbf4'
MAX_WORDS = 32  # tools/ee_aot_generate.py block limit
ROOT = Path(__file__).resolve().parents[1]

SIGNATURE = 'void {name}(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {{'
IDENT = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')
VOID_LINE = re.compile(r'^void ([A-Za-z_][A-Za-z0-9_]*) ?\(')
HEX = r'0x[0-9a-f]+'
ADDRESS_COMMENT = re.compile(rf'^// Address: ({HEX}) - ({HEX})$')
FUNCTION_COMMENT = re.compile(r'^// Function: \S.*$')
INSTR_COMMENT = re.compile(rf'^// ({HEX}): ({HEX})(?: (.*))?$')
PC_ASSIGN = re.compile(rf'^ctx->pc = ({HEX})u;$')


class ParseError(ValueError):
    """The source is outside the audited subset; nothing may be lowered."""


def _fail(message):
    raise ParseError(message)


def _canon(value):
    return f'0x{value:x}'


def _parse_hex(text, what):
    value = int(text, 16)
    if text != _canon(value):
        _fail(f'{what} {text} is not canonical lowercase hex')
    if value > 0xffffffff:
        _fail(f'{what} {text} outside uint32')
    return value


def normalize(source):
    """Collapse whitespace; drop blank lines. Semantics are never rewritten."""
    if not source.isascii():
        _fail('non-ASCII source rejected')
    if '\0' in source or '/*' in source or '\\\n' in source:
        _fail('NUL, block comment or line continuation rejected')
    lines = []
    for raw in source.splitlines():
        line = ' '.join(raw.split())
        if line:
            lines.append(line)
    return lines


# Statement templates reconstructed from the pinned upstream translators
# (instruction_translator.cpp / special_translator.cpp). `{rd}`-style fields
# come from the raw word; `{simm}` is the sign-extended immediate printed as
# an unsigned 32-bit decimal and `{imm}` the zero-extended 16-bit immediate.
I_TYPE = {  # opcode -> (mnemonic, template). All write rt.
    0x0a: ('slti', 'SET_GPR_U64(ctx, {rt}, ((int64_t)GPR_S64(ctx, {rs}) < (int64_t)(int32_t){simm}) ? 1 : 0);'),
    0x0b: ('sltiu', 'SET_GPR_U64(ctx, {rt}, ((uint64_t)GPR_U64(ctx, {rs}) < (uint64_t)(int64_t)(int32_t){simm}) ? 1 : 0);'),
    0x0c: ('andi', 'SET_GPR_U64(ctx, {rt}, GPR_U64(ctx, {rs}) & (uint64_t)(uint16_t){imm});'),
    0x0d: ('ori', 'SET_GPR_U64(ctx, {rt}, GPR_U64(ctx, {rs}) | (uint64_t)(uint16_t){imm});'),
    0x0e: ('xori', 'SET_GPR_U64(ctx, {rt}, GPR_U64(ctx, {rs}) ^ (uint64_t)(uint16_t){imm});'),
}
R_TYPE = {  # SPECIAL funct -> (mnemonic, template). All write rd; shamt must be 0.
    0x21: ('addu', 'SET_GPR_S32(ctx, {rd}, (int32_t)ADD32(GPR_U32(ctx, {rs}), GPR_U32(ctx, {rt})));'),
    0x23: ('subu', 'SET_GPR_S32(ctx, {rd}, (int32_t)SUB32(GPR_U32(ctx, {rs}), GPR_U32(ctx, {rt})));'),
    0x24: ('and', 'SET_GPR_U64(ctx, {rd}, GPR_U64(ctx, {rs}) & GPR_U64(ctx, {rt}));'),
    0x25: ('or', 'SET_GPR_U64(ctx, {rd}, GPR_U64(ctx, {rs}) | GPR_U64(ctx, {rt}));'),
    0x26: ('xor', 'SET_GPR_U64(ctx, {rd}, GPR_U64(ctx, {rs}) ^ GPR_U64(ctx, {rt}));'),
    0x27: ('nor', 'SET_GPR_U64(ctx, {rd}, ~(GPR_U64(ctx, {rs}) | GPR_U64(ctx, {rt})));'),
    0x2a: ('slt', 'SET_GPR_U64(ctx, {rd}, ((int64_t)GPR_S64(ctx, {rs}) < (int64_t)GPR_S64(ctx, {rt})) ? 1 : 0);'),
    0x2b: ('sltu', 'SET_GPR_U64(ctx, {rd}, ((uint64_t)GPR_U64(ctx, {rs}) < (uint64_t)GPR_U64(ctx, {rt})) ? 1 : 0);'),
}


def expected_statement(word):
    """Return (mnemonic, statement) the pinned upstream emits, or reject."""
    op, rs, rt, rd = word >> 26, word >> 21 & 31, word >> 16 & 31, word >> 11 & 31
    sa, fn = word >> 6 & 31, word & 63
    if word == 0:
        return 'nop', '// NOP'
    fields = {'rs': rs, 'rt': rt, 'rd': rd, 'imm': word & 0xffff,
              'simm': (((word & 0xffff) ^ 0x8000) - 0x8000) & 0xffffffff}
    if op == 9:  # ADDIU
        if rt == 0:
            _fail('ADDIU to $zero is rejected (upstream emits a distinct NOP comment)')
        return 'addiu', ('SET_GPR_S32(ctx, {rt}, (int32_t)ADD32(GPR_U32(ctx, {rs}), {simm}));'
                         .format(**fields))
    if op in I_TYPE:
        mnemonic, template = I_TYPE[op]
        return mnemonic, template.format(**fields)
    if op == 0x0f:  # LUI
        if rs != 0:
            _fail('reserved rs field nonzero in LUI')
        return 'lui', 'SET_GPR_S32(ctx, {rt}, (int32_t)((uint32_t){imm} << 16));'.format(**fields)
    if op == 0 and fn in R_TYPE:
        if sa != 0:
            _fail('reserved shamt field nonzero')
        mnemonic, template = R_TYPE[fn]
        return mnemonic, template.format(**fields)
    if op == 0 and fn == 0:  # SLL (non-canonical-NOP)
        if rs != 0:
            _fail('reserved rs field nonzero in SLL')
        if rd == 0:
            _fail('SLL to $zero is rejected (upstream emits an empty statement)')
        return 'sll', f'SET_GPR_S32(ctx, {rd}, (int32_t)SLL32(GPR_U32(ctx, {rt}), {sa}));'
    _fail(f'instruction word 0x{word:08x} is outside the supported subset')


def parse_function(source, name):
    """Parse the one function called `name`; return a result dict."""
    if not IDENT.match(name):
        _fail('invalid function name')
    lines = normalize(source)
    heads = [i for i, line in enumerate(lines)
             if (m := VOID_LINE.match(line)) and m.group(1) == name]
    if len(heads) != 1:
        _fail(f'function {name!r} found {len(heads)} times; exactly one required')
    head = heads[0]
    if lines[head] != SIGNATURE.format(name=name):
        _fail('function signature differs from the pinned upstream shape')
    if head < 2:
        _fail('missing address-range comment')
    m = ADDRESS_COMMENT.match(lines[head - 1])
    if not m or not FUNCTION_COMMENT.match(lines[head - 2]):
        _fail('missing "// Function:" / "// Address:" comments before the function')
    start, end = _parse_hex(m.group(1), 'range start'), _parse_hex(m.group(2), 'range end')
    suffix = re.search(r'_0x([0-9a-f]+)$', name)
    if suffix and int(suffix.group(1), 16) != start:
        _fail('function name address suffix disagrees with the address range')
    if start == 0 or start & 3 or end & 3 or end <= start:
        _fail('range must be nonzero, 4-byte aligned and non-empty')
    count = (end - start) // 4
    if count > MAX_WORDS:
        _fail(f'{count} instructions exceed the {MAX_WORDS}-word AOT block limit')

    try:
        close = lines.index('}', head + 1)
    except ValueError:
        _fail('function body is not terminated')
    body = lines[head + 1:close]
    pos = 0
    # Optional logging preamble: exactly the upstream text, nothing else.
    preamble = ['#ifdef PS2_FUNCTION_LOG_TRACKER', f'PS_LOG_ENTRY("{name}");', '#endif']
    if body[:3] == preamble:
        pos = 3
    if pos >= len(body) or PC_ASSIGN.match(body[pos]) is None or \
            _parse_hex(PC_ASSIGN.match(body[pos]).group(1), 'entry pc') != start:
        _fail('entry "ctx->pc = start" missing or wrong')
    pos += 1
    words = []
    for index in range(count):
        if pos + 3 > len(body):
            _fail('body ends before the declared address range')
        address = start + 4 * index
        c = INSTR_COMMENT.match(body[pos])
        if not c:
            _fail(f'missing instruction comment at {_canon(address)}')
        if _parse_hex(c.group(1), 'comment pc') != address:
            _fail(f'instruction comment pc is not contiguous at {_canon(address)}')
        word = _parse_hex(c.group(2), 'raw word')
        p = PC_ASSIGN.match(body[pos + 1])
        if not p or _parse_hex(p.group(1), 'ctx->pc') != address:
            _fail(f'ctx->pc does not match instruction address {_canon(address)}')
        mnemonic, statement = expected_statement(word)
        disassembly = c.group(3)
        if disassembly is not None and disassembly.split(' ')[0] != mnemonic:
            _fail(f'disassembly mnemonic disagrees with raw word at {_canon(address)}')
        if body[pos + 2] != statement:
            _fail(f'statement at {_canon(address)} differs from the decoded instruction')
        words.append(word)
        pos += 3
    end_pc = PC_ASSIGN.match(body[pos]) if pos < len(body) else None
    if not end_pc or _parse_hex(end_pc.group(1), 'fallthrough pc') != end:
        _fail('fallthrough pc is missing or does not equal the function end')
    pos += 1
    if pos != len(body):
        _fail('unexplained extra statements in function body')
    return {'name': name, 'start': start, 'end': end, 'words': words,
            'source_sha256': hashlib.sha256(source.encode()).hexdigest()}


def lower(results, output, complex_output=None):
    """Hand only the validated words to the existing AOT generator.

    `results` is one parse_function() result or a list of them (distinct
    functions become distinct AOT entries keyed by their verified start PC).
    """
    results = [results] if isinstance(results, dict) else list(results)
    manifest = {'blocks': [{'pc': r['start'], 'words': r['words']} for r in results]}
    manifest_path = Path(str(output) + '.manifest.json')
    manifest_path.write_text(json.dumps(manifest))
    try:
        command = [sys.executable, str(ROOT / 'tools/ee_aot_generate.py'),
                   '--manifest', str(manifest_path), '--output', str(output)]
        if complex_output:
            command += ['--complex-output', str(complex_output)]
        done = subprocess.run(command, capture_output=True, text=True)
    finally:
        manifest_path.unlink(missing_ok=True)
    if done.returncode != 0:
        _fail(f'ee_aot_generate rejected the validated words: {done.stderr.strip()}')
    report = json.loads(done.stdout)
    return {'parser_version': PARSER_VERSION, 'upstream_commit': UPSTREAM_COMMIT,
            'functions': [{'function': r['name'], 'start': r['start'], 'end': r['end'],
                           'instructions': len(r['words']),
                           'source_sha256': r['source_sha256']} for r in results],
            'aot_sha256': report['sha256'], 'wii_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='generated .cpp file')
    parser.add_argument('--function', required=True, action='append', help='exact sanitized function name (repeatable)')
    parser.add_argument('--output', type=Path, help='AOT .inc to write (omit to parse only)')
    parser.add_argument('--report', type=Path, help='JSON lowering report')
    args = parser.parse_args()
    try:
        source = args.source.read_text()
        results = [parse_function(source, name) for name in args.function]
        summary = {'functions': [{'name': r['name'], 'start': r['start'], 'end': r['end'],
                                  'words': [f'0x{w:08x}' for w in r['words']]} for r in results],
                   'source_sha256': results[0]['source_sha256']}
        if args.output:
            summary['lowering'] = lower(results, args.output)
            if args.report:
                args.report.write_text(json.dumps(summary, indent=2) + '\n')
        print(json.dumps(summary))
    except (ParseError, OSError, UnicodeDecodeError) as error:
        parser.exit(2, f'ps2recomp_strict_parser: {error}\n')


if __name__ == '__main__':
    main()
