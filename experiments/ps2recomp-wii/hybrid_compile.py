#!/usr/bin/env python3
"""GPL-3.0-or-later. Conservative ELF-to-guarded-EE-AOT experiment.

Reuse Gekko2's full EE state and interpreter fallback. Do not mistake static
coverage for a complete native port. Generated code contains private input.
"""
import argparse
from collections import deque, Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('ee_aot', ROOT/'tools/ee_aot_generate.py')
aot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aot)


def successors(pc, word):
    """Return only statically justified edges. Never guess indirect targets."""
    op, rs, rt, fn = word >> 26, word >> 21 & 31, word >> 16 & 31, word & 63
    after = (pc + 8) & 0xffffffff
    if op in (2, 3):
        target = ((pc + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2)
        return ([target, after] if op == 3 else [target]), False
    if op == 0 and fn in (8, 9):
        # JALR may return; a JR does not establish an independent fallthrough.
        return ([after] if fn == 9 and (word >> 11 & 31) else []), True
    if (op in (4, 5, 6, 7, 20, 21, 22, 23) or
        op == 1 and rt in (0, 1, 2, 3, 16, 17, 18, 19) or
        op in (16, 17, 18) and rs == 8 and rt < 4):
        immediate = word & 65535
        if immediate & 32768:
            immediate -= 65536
        return [after, (pc + 4 + immediate * 4) & 0xffffffff], False
    return [], False


def discover(segments, entries, max_blocks=512, max_words=32):
    if not 1 <= max_blocks <= 4096 or not 1 <= max_words <= 32:
        raise ValueError('limits: 1..4096 blocks and 1..32 words')
    pending, seen, blocks, gaps = deque(entries), set(), [], []
    def fetch(pc):
        if pc & 3:
            return None
        matches = [(base, data) for base, data in segments if base <= pc and pc+4 <= base+len(data)]
        if len(matches) != 1:
            return None
        base, data = matches[0]
        return struct.unpack_from('<I', data, pc-base)[0]
    while pending and len(blocks) < max_blocks:
        start = pending.popleft()
        if start in seen:
            continue
        seen.add(start)
        pc, words = start, []
        while len(words) < max_words:
            word = fetch(pc)
            if word is None:
                gaps.append({'pc': pc, 'reason': 'no_unique_executable_backing'})
                break
            # Keep CACHE at an explicit runtime boundary, even though the
            # present shared AOT backend can specialize its current no-op model.
            # This prototype does not fold cache maintenance into native regions.
            if word >> 26 == 47:
                gaps.append({'pc': pc, 'reason': 'interpreter_cache_boundary'})
                pending.append((pc+4) & 0xffffffff)
                break
            translated = aot.translate(word, pc)
            if translated is None:
                gaps.append({'pc': pc, 'reason': 'interpreter_required'})
                # A syscall can resume at its architectural continuation;
                # only the runtime/handler decides whether it actually does.
                # Precompiling that entry never forces a return or skips a trap.
                # Do not scan linearly past an unknown control-flow instruction.
                if word >> 26 == 0 and word & 63 == 12:
                    pending.append((pc+4) & 0xffffffff)
                break
            words.append(word)
            if aot.has_delay_slot(word):
                targets, indirect = successors(pc, word)
                if indirect:
                    gaps.append({'pc': pc, 'reason': 'dynamic_target'})
                # A slot is included only if it fits and is nonterminal.
                slot = fetch((pc+4) & 0xffffffff)
                decoded_slot = aot.translate(slot, (pc+4) & 0xffffffff) if slot is not None else None
                if len(words) < max_words and decoded_slot is not None and not decoded_slot[1]:
                    words.append(slot)
                else:
                    gaps.append({'pc': (pc+4) & 0xffffffff, 'reason': 'interpreter_delay_slot'})
                pending.extend(targets)
                break
            if translated[1]:
                gaps.append({'pc': pc, 'reason': 'runtime_control_barrier'})
                # COP0/VU/event barriers finish AOT; the runtime selects the next PC.
                break
            pc = (pc + 4) & 0xffffffff
        else:
            pending.append(pc)
        if words:
            blocks.append((start, words))
    # Generation validates the final block contracts, including delay-slot shape.
    aot.generate(blocks)
    return blocks, gaps, bool(pending)


def compile_elf(elf, output, entries=(), max_blocks=512, max_words=32):
    output = output.resolve()
    if output == ROOT or output.is_relative_to(ROOT):
        raise ValueError('generated guest output must be outside the Gekko2 checkout')
    if not 52 <= elf.stat().st_size <= 32*1024*1024:
        raise ValueError('ELF input must be 52 bytes..32 MiB')
    data = elf.read_bytes()
    segments = aot.load_segments(argparse.Namespace(image=None, elf=str(elf)))
    if data != elf.read_bytes():
        raise ValueError('ELF changed during analysis')
    if struct.unpack_from('<H', data, 16)[0] != 2:
        raise ValueError('only relocated ET_EXEC is supported; no guessed relocations')
    entry = struct.unpack_from('<I', data, 24)[0]
    entries = list(entries) or [entry]
    for pc in entries:
        if pc == 0 or pc & 3 or not 0 <= pc <= 0xffffffff:
            raise ValueError('entry must be a nonzero aligned uint32 address')
    blocks, gaps, limited = discover(segments, entries, max_blocks, max_words)
    if not blocks:
        raise ValueError('no supported executable entry; interpreter-only image')
    output.mkdir(parents=True, mode=0o700, exist_ok=False)
    (output/'core/ee').mkdir(parents=True)
    code = aot.generate(blocks)
    # Keep quoted generated includes isolated; never overwrite production includes.
    (output/'ee_aot.c').write_text((ROOT/'source/core/recompiler/ee_aot.c').read_text())
    (output/'ee_aot_generated.inc').write_text(code)
    (output/'core/ee/ee_aot_complex_generated.inc').write_text(aot.generate_complex(blocks))
    (output/'manifest.private.json').write_text(json.dumps({'blocks':[
        {'pc':pc, 'words':words} for pc, words in blocks]}, indent=2)+'\n')
    report = {'kind':'guarded-ee-native-hybrid', 'schema':1,
        'input_sha256':hashlib.sha256(data).hexdigest(),
        'code_sha256':hashlib.sha256(code.encode()).hexdigest(),
        'blocks':len(blocks), 'static_instructions':sum(len(w) for _,w in blocks),
        'selection_limit_reached':limited, 'gaps':gaps,
        'gap_counts':dict(Counter(g['reason'] for g in gaps)),
        'state':'existing_128_bit_ee_state',
        'fallback':'existing_interpreter_at_live_pc',
        'devices':'existing_Gekko2_runtime_required',
        'wii_build':False, 'wii_execution_verified':False,
        'standalone_game_port':False}
    (output/'report.private.json').write_text(json.dumps(report, indent=2)+'\n')
    for p in output.rglob('*'):
        p.chmod(0o700 if p.is_dir() else 0o600)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--entry',type=lambda x:int(x,0),action='append',default=[])
    parser.add_argument('--max-blocks',type=int,default=512)
    parser.add_argument('--max-words',type=int,default=32)
    args=parser.parse_args()
    try:
        report=compile_elf(args.elf,args.output,args.entry,args.max_blocks,args.max_words)
    except (ValueError,OSError,struct.error) as exc:
        parser.exit(2,f'hybrid_compile: {exc}\n')
    # No guest addresses, opcodes or private filesystem paths on stdout.
    print(json.dumps({key:report[key] for key in (
        'kind','blocks','static_instructions','selection_limit_reached',
        'gap_counts','wii_build','wii_execution_verified','standalone_game_port')}))

if __name__=='__main__':
    main()
