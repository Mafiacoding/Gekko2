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
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import types

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
    lines, closers = [], set()
    for raw in source.splitlines():
        line = ' '.join(raw.split())
        if line:
            if raw.rstrip() == '}':  # only a column-0 brace ends a function
                closers.add(len(lines))
            lines.append(line)
    return lines, closers


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
    lines, closers = normalize(source)
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

    close = next((i for i in sorted(closers) if i > head), None)
    if close is None:
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
    terminal = None
    inferred_slot = False
    address = start
    while address < end and terminal is None:
        if pos + 3 > len(body):
            _fail('body ends before the declared address range')
        c = INSTR_COMMENT.match(body[pos])
        if not c:
            _fail(f'missing instruction comment at {_canon(address)}')
        if _parse_hex(c.group(1), 'comment pc') != address:
            _fail(f'instruction comment pc is not contiguous at {_canon(address)}')
        word = _parse_hex(c.group(2), 'raw word')
        if word == JR_RA:
            pos, slot = _parse_jr_ra(body, pos, c, address, end)
            words.append(word)
            words.append(slot[0])
            inferred_slot = slot[1]
            terminal = 'jr_ra'
            address += 8
            break
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
        address += 4
    if address != end:
        _fail('instructions do not cover the declared address range exactly')
    if len(words) > MAX_WORDS:
        _fail(f'{len(words)} instructions exceed the {MAX_WORDS}-word AOT block limit')
    # Upstream prints the fallthrough PC lowercase after straight-line code and
    # uppercase after a control-flow block (fmt {:X}).
    expected_end = f'ctx->pc = 0x{end:X}u;' if terminal else f'ctx->pc = 0x{end:x}u;'
    if pos >= len(body) or body[pos] != expected_end:
        _fail('fallthrough pc is missing or does not equal the function end')
    pos += 1
    if pos != len(body):
        _fail('unexplained extra statements in function body')
    return {'name': name, 'start': start, 'end': end, 'words': words, 'terminal': terminal,
            'inferred_nop_slot': inferred_slot,
            'source_sha256': hashlib.sha256(source.encode()).hexdigest()}


JR_RA = 0x03e00008  # jr $ra (rd = 0, shamt = 0)
SLOT_COMMENT = re.compile(rf'^// ({HEX}): ({HEX})(?: (.*))? \(Delay Slot\)$')


def _parse_jr_ra(body, pos, comment, address, end):
    """Validate the exact upstream `jr $ra` block; return (new pos, (slot word, inferred)).

    The delay slot is either decoded in the source (`(Delay Slot)` comment and
    statement) or absent. Upstream elides a guest NOP slot, so an absent slot
    is lowered as the canonical NOP word 0; AOT execution still compares the
    live memory word, so a different real word falls back to the interpreter.
    """
    if end != address + 8:
        _fail('jr $ra must be the second-to-last word (its delay slot ends the function)')
    disassembly = comment.group(3)
    if disassembly is not None and disassembly.split(' ')[0] != 'jr':
        _fail('disassembly mnemonic disagrees with jr $ra')
    J, S = f'0x{address:X}', f'0x{address + 4:X}'
    head = [f'ctx->pc = {J}u;', '{', 'const uint32_t jumpTarget = GPR_U32(ctx, 31);']
    tail = ['ctx->pc = jumpTarget;',
            '#if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS',
            f'(void)runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, {J}u, 0u, '
            'PS2Runtime::GuestBranchKind::Return, "JR $ra");',
            'return;', '#else', 'ctx->pc = jumpTarget;', 'return;', '#endif', '}']
    start = pos + 1
    if body[start:start + 3] != head:
        _fail('jr $ra block differs from the pinned upstream emitter')
    cursor = start + 3
    slot_word, inferred = 0, True
    if cursor < len(body) and body[cursor] == f'ctx->pc = {S}u;':
        if body[cursor + 1:cursor + 3] != ['ctx->in_delay_slot = true;', f'ctx->branch_pc = {J}u;']:
            _fail('delay-slot prologue differs from the pinned upstream emitter')
        m = SLOT_COMMENT.match(body[cursor + 3]) if cursor + 3 < len(body) else None
        if not m or _parse_hex(m.group(1), 'slot pc') != address + 4:
            _fail('delay-slot comment missing or at the wrong address')
        slot_word = _parse_hex(m.group(2), 'slot word')
        if slot_word == 0:
            _fail('upstream elides a NOP delay slot; a decoded NOP slot is inconsistent')
        mnemonic, statement = expected_statement(slot_word)
        if m.group(3) is not None and m.group(3).split(' ')[0] != mnemonic:
            _fail('delay-slot disassembly mnemonic disagrees with its raw word')
        if body[cursor + 4:cursor + 6] != [statement, 'ctx->in_delay_slot = false;']:
            _fail('delay-slot statement differs from the decoded instruction')
        cursor += 6
        inferred = False
    if body[cursor:cursor + len(tail)] != tail:
        _fail('jr $ra epilogue differs from the pinned upstream emitter')
    return cursor + len(tail), (slot_word, inferred)


def _guest_nop(word):
    """Mirror of upstream isGuestNop(): the only slots upstream elides."""
    op, rs, rt, rd, sa, fn = word >> 26, word >> 21 & 31, word >> 16 & 31, word >> 11 & 31, word >> 6 & 31, word & 63
    return (op == 0 and fn == 0 and rd == 0 and rt == 0 and sa == 0) or \
           (op == 9 and rs == 0 and rt == 0 and (word & 0xffff) == 0)


def _elf_function_symbols(data):
    """(value, size, name) of STT_FUNC symbols of an ELF32 LE file, or []."""
    shoff, = struct.unpack_from('<I', data, 32)
    shentsize, shnum = struct.unpack_from('<HH', data, 46)
    if shoff == 0 or shentsize < 40 or shoff + shentsize * shnum > len(data):
        return []
    sections = [struct.unpack_from('<10I', data, shoff + i * shentsize) for i in range(shnum)]
    symbols = []
    for _, kind, _, _, offset, size, link, _, _, entsize in sections:
        if kind != 2 or entsize < 16 or link >= shnum or offset + size > len(data):
            continue
        _, _, _, _, stroff, strsize, _, _, _, _ = sections[link]
        for i in range(size // entsize):
            name_off, value, sym_size, info, _, _ = struct.unpack_from('<IIIBBH', data, offset + i * entsize)
            if info & 15 == 2:
                end = data.find(b'\0', stroff + name_off, stroff + strsize)
                symbols.append((value, sym_size, data[stroff + name_off:end].decode('ascii', 'replace')))
    return symbols


def bind_to_elf(result, elf_path):
    """Cross-check a parsed function against the ELF it was generated from.

    The words must equal the executable bytes at the function range. An
    elided delay slot (inferred word 0) is replaced by the real ELF word,
    which must be a guest NOP exactly as upstream defines it. A FUNC symbol at
    the start, if present, must have size end - start.
    """
    elf_path = Path(elf_path)
    data = elf_path.read_bytes()
    spec = importlib.util.spec_from_file_location('ee_aot_generate_bind', ROOT / 'tools/ee_aot_generate.py')
    aot = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(aot)
    try:
        segments = aot.load_segments(types.SimpleNamespace(image=None, elf=str(elf_path)))
    except (ValueError, struct.error) as error:
        _fail(f'ELF rejected: {error}')
    if data != elf_path.read_bytes():
        _fail('ELF changed during analysis')
    if struct.unpack_from('<H', data, 16)[0] != 2:
        _fail('only ET_EXEC ELF input is supported (no guessed relocations)')
    start, end = result['start'], result['end']
    backing = [(base, blob) for base, blob in segments if base <= start and end <= base + len(blob)]
    if len(backing) != 1:
        _fail('function range has missing or ambiguous executable backing in the ELF')
    base, blob = backing[0]
    actual = list(struct.unpack_from(f'<{(end - start) // 4}I', blob, start - base))
    words = list(result['words'])
    slot_source = 'source'
    if result['inferred_nop_slot']:
        if not _guest_nop(actual[-1]):
            _fail('source elides the delay slot but the ELF slot word is not a guest NOP')
        words[-1] = actual[-1]
        slot_source = 'elf'
    if words != actual:
        index = next(i for i, (a, b) in enumerate(zip(words, actual)) if a != b)
        _fail(f'ELF word at {_canon(start + 4 * index)} differs from the generated source')
    symbols = [(v, sz, n) for v, sz, n in _elf_function_symbols(data) if v == start]
    if symbols and all(sz != end - start for _, sz, _ in symbols):
        _fail('ELF FUNC symbol size disagrees with the generated address range')
    bound = dict(result)
    bound.update({'words': words, 'inferred_nop_slot': False, 'slot_source': slot_source,
                  'elf_sha256': hashlib.sha256(data).hexdigest(), 'elf_symbol_checked': bool(symbols)})
    return bound


def write_private_build(results, directory):
    """Write an include directory for a local build; refuse paths inside the checkout."""
    directory = Path(directory).resolve()
    if directory == ROOT or ROOT in directory.parents:
        _fail('generated guest output must be outside the Gekko2 checkout')
    directory.mkdir(parents=True, mode=0o700, exist_ok=False)
    (directory / 'core/ee').mkdir(parents=True, mode=0o700)
    report = lower(results, directory / 'ee_aot_generated.inc', directory / 'core/ee/ee_aot_complex_generated.inc')
    (directory / 'report.private.json').write_text(json.dumps(report, indent=2) + '\n')
    # Words stay in the private directory only (verifier input for linked-PPC checks).
    (directory / 'functions.private.json').write_text(json.dumps({'blocks': [
        {'name': r['name'], 'pc': r['start'], 'words': r['words'], 'terminal': r.get('terminal')}
        for r in results]}, indent=2) + '\n')
    for path in directory.rglob('*'):
        os.chmod(path, 0o700 if path.is_dir() else 0o600)
    return report


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
                           'source_sha256': r['source_sha256'],
                           'terminal': r.get('terminal'),
                           'slot_source': r.get('slot_source', 'inferred' if r.get('inferred_nop_slot') else 'source'),
                           'elf_sha256': r.get('elf_sha256'),
                           'elf_symbol_checked': r.get('elf_symbol_checked')} for r in results],
            'aot_sha256': report['sha256'], 'wii_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='generated .cpp file')
    parser.add_argument('--function', required=True, action='append', help='exact sanitized function name (repeatable)')
    parser.add_argument('--elf', type=Path, help='ELF the source was generated from (verifies words, NOP slots, symbol)')
    out = parser.add_mutually_exclusive_group()
    out.add_argument('--output', type=Path, help='AOT .inc to write (development; omit to parse only)')
    out.add_argument('--output-dir', type=Path,
                     help='new private include directory outside the checkout; requires --elf')
    args = parser.parse_args()
    try:
        source = args.source.read_text()
        results = [parse_function(source, name) for name in args.function]
        if args.elf:
            results = [bind_to_elf(r, args.elf) for r in results]
        if args.output_dir and not args.elf:
            _fail('--output-dir requires --elf: private builds must be bound to their ELF')
        summary = {'functions': [{'name': r['name'], 'start': r['start'], 'end': r['end'],
                                  'words': [f'0x{w:08x}' for w in r['words']]} for r in results],
                   'source_sha256': results[0]['source_sha256']}
        if args.output:
            summary['lowering'] = lower(results, args.output)
        elif args.output_dir:
            # No guest addresses or opcodes on stdout for private inputs.
            summary = {'functions': len(results), 'lowering': write_private_build(results, args.output_dir)}
        print(json.dumps(summary))
    except (ParseError, OSError, UnicodeDecodeError) as error:
        parser.exit(2, f'ps2recomp_strict_parser: {error}\n')


if __name__ == '__main__':
    main()
