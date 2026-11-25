#!/usr/bin/env python3
"""GPL-3.0-or-later. Generate guarded EE C blocks from explicit entry points.

No BIOS or game bytes are bundled. Unsupported instructions terminate a
block; the existing emulator executes them. This is a hybrid recompiler.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import re
from functools import lru_cache

ROOT = Path(__file__).resolve().parents[1]
MEMORY_OPS = {0x1a,0x1b,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x31,0x36,0x37,0x39,0x3e,0x3f}

@lru_cache(maxsize=65536)
def complex_family(word):
    op,rs,rt,sa,fn=word>>26,word>>21&31,word>>16&31,word>>6&31,word&63
    if op in MEMORY_OPS:return 'mem'
    if op==0x10:
        if rs in (0,4) or rs==8 and rt<4 or rs&16 and fn in (1,2,6,8,16,24,56,57):return 'cop0'
    if op==0x11:
        if rs in (0,2,4,6) or rs==8 and rt<4 or rs==20 and fn==32:return 'cop1'
        if rs==16 and fn in (0,1,2,3,4,5,6,7,0x16,0x18,0x19,0x1a,0x1c,0x1d,0x1e,0x1f,0x24,0x28,0x29,0x30,0x32,0x34,0x36):return 'cop1'
    if op==0x12:
        if rs in (0,1,2,4,5,6) or rs==8 and rt<4:return 'cop2'
        if rs&16:
            if fn<=0x32 or fn in (0x34,0x35,0x38,0x39):
                if fn!=0x33:return 'cop2'
            if fn>=60:
                idx=(word&3)|((word>>4)&0x7c)
                if idx<=42 or idx in (44,45,46,47,48,49) or 52<=idx<=67:return 'cop2'
    if op==0x1c:
        source=(ROOT/'source/core/ee/ee_mmi_switch.inc').read_text()
        top=[int(x,16) for x in re.findall(r'^        case 0x([0-9A-Fa-f]+):',source,re.M)]
        if fn not in top:return None
        if fn in (8,9,40,41):
            begin=re.search(rf'^        case 0x{fn:02X}:',source,re.M).start()
            following=re.search(r'^        case ',source[begin+1:],re.M)
            end=begin+1+following.start() if following else len(source)
            group=source[begin:end]
            supported=[int(x,16) for x in re.findall(r'^            case 0x([0-9A-Fa-f]+):',group,re.M)]
            if sa not in supported:return None
        return 'mmi'
    return None

def generate_complex(blocks):
    words=sorted({w for _,ws in blocks for w in ws if complex_family(w)})
    out=['/* GPL-3.0-or-later. Exact-word native specializations of shared EE semantics. */',
         '#define GPR(x) st->gpr[x].ud0', '#define GPR1(x) st->gpr[x].ud1',
         '#define BRANCH_TO(target) do { st->next_pc=(target);st->branch_pending=1; } while(0)',
         '#define LINK(reg) do { GPR(reg)=this_pc+8u; } while(0)']
    entries=[]
    for i,w in enumerate(words):
        out += [f'static int ee_complex_{i}(ee_state_t *st) {{',f'const uint32_t instr=UINT32_C(0x{w:08x});',
                'const uint32_t op=instr>>26,rs=(instr>>21)&31u,rt=(instr>>16)&31u,rd=(instr>>11)&31u,sa=(instr>>6)&31u,funct=instr&63u;',
                'const uint32_t rs32=(uint32_t)GPR(rs),rt32=(uint32_t)GPR(rt),uimm=instr&65535u;',
                'const int32_t imm=(int16_t)instr;const uint32_t this_pc=st->exc_this_pc,fallthrough_pc=st->pc;const int in_delay_slot=st->exc_in_delay_slot;',
                '(void)rs32;(void)rt32;(void)uimm;(void)imm;(void)this_pc;(void)fallthrough_pc;(void)in_delay_slot;(void)sa;(void)rd;(void)funct;',
                'switch(op) {',f'#include "core/ee/ee_{complex_family(w)}_switch.inc"','default: return 1;','}', 'return 0;','}']
        entries.append(f'{{UINT32_C(0x{w:08x}),ee_complex_{i}}}')
    out += ['#undef GPR','#undef GPR1','#undef BRANCH_TO','#undef LINK',
            'typedef struct { uint32_t word;int (*fn)(ee_state_t *); } ee_complex_entry;',
            'static const ee_complex_entry ee_complex_entries[] = {'+','.join(entries or ['{0u,NULL}'])+'};',
            'static const ee_complex_entry *ee_complex_find(uint32_t word) {',f'unsigned lo=0,hi={len(words)}u;',
            'while(lo<hi){unsigned mid=lo+(hi-lo)/2;const ee_complex_entry *e=&ee_complex_entries[mid];if(word<e->word)hi=mid;else if(word>e->word)lo=mid+1;else return e;}return NULL;','}',
            'int ee_aot_complex_available(uint32_t word){return ee_complex_find(word)!=NULL;}',
            'int ee_aot_execute_complex(ee_state_t *st,uint32_t word){const ee_complex_entry *e=ee_complex_find(word);return e?(e->fn(st)?-1:1):0;}','']
    return '\n'.join(out)


def number(value):
    result = int(value, 0) if isinstance(value, str) else int(value)
    if not 0 <= result <= 0xffffffff:
        raise ValueError('address/word outside uint32 range')
    return result


def has_delay_slot(word):
    op, rs, rt, fn = word >> 26, word >> 21 & 31, word >> 16 & 31, word & 63
    return (op in (2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or
            op == 0 and fn in (8, 9) or
            op == 1 and rt in (0, 1, 2, 3, 16, 17, 18, 19) or
            op in (16, 17, 18) and rs == 8 and rt < 4)


def supported_prefix(pc, values, max_words):
    words = []
    slot = False
    for value in values[:max_words]:
        word = number(value)
        result = translate(word, pc + len(words) * 4)
        if result is None or slot and result[1]:
            break
        words.append(word)
        if slot:
            break
        if result[1]:
            if not has_delay_slot(word):
                break
            slot = True
    return words


def translate(word, pc):
    op, rs, rt, rd, sa, fn = word >> 26, word >> 21 & 31, word >> 16 & 31, word >> 11 & 31, word >> 6 & 31, word & 63
    imm = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
    uimm = word & 0xffff
    g = lambda r: f'st->gpr[{r}].ud0'
    s32 = lambda x: f'ee_aot_sext32((uint32_t)({x}))'
    assign = lambda r, x: f'{g(r)} = {x};' if r else '(void)st;'
    target = (pc + 4 + imm * 4) & 0xffffffff
    family=complex_family(word)
    # COP/MMI/memory specializations execute the same bodies as the scalar core.
    if family and (op not in (32,33,35,36,37,39,55,40,41,43,63)):
        terminal=(op==0x10 and rs!=0) or (op in (0x11,0x12) and rs==8) or (op==0x12 and rs&16 and fn in (56,57))
        return f'(void)ee_aot_execute_complex(st, UINT32_C(0x{word:08x}));',bool(terminal)
    if op in (8,24) or op==0 and fn in (32,34,44,46):
        bits=32 if op==8 or op==0 and fn in (32,34) else 64
        rhs=f'(uint64_t)(int64_t)({imm})' if op else g(rt)
        dest=rt if op else rd
        subtract=int(op==0 and fn in (34,46))
        return f'ee_aot_signed(st,{g(rs)},{rhs},{bits}u,{subtract}u,{dest}u);',False
    if op==0 and fn in (48,49,50,51,52,54) or op==1 and rt in (8,9,10,11,12,14):
        kind=fn if op==0 else {8:48,9:49,10:50,11:51,12:52,14:54}[rt]
        right=g(rt) if op==0 else f'(uint64_t)(int64_t)({imm})'
        signed=kind in (48,50)
        left=f'(int64_t){g(rs)}' if signed else g(rs)
        right=f'(int64_t)({right})' if signed else right
        relation={48:'>=',49:'>=',50:'<',51:'<',52:'==',54:'!='}[kind]
        return f'if({left} {relation} {right}) ee_core_raise_trap(st);',False
    if op==0 and fn==40:return assign(rd,'st->sa_reg'),False
    if op==0 and fn==41:return f'st->sa_reg=(uint32_t){g(rs)};',False
    if op==1 and rt in (24,25):
        mask=15 if rt==24 else 7
        return f'st->sa_reg=(((uint32_t){g(rs)} & {mask}u) ^ ({uimm}u & {mask}u))'+(';' if rt==24 else ' << 1;'),False
    if op in (47,51):return '(void)st;',False
    if op==0 and fn in (24,25):
        signed=fn==24
        expr=f'(uint64_t)((int64_t)(int32_t){g(rs)} * (int64_t)(int32_t){g(rt)})' if signed else f'(uint64_t)(uint32_t){g(rs)} * (uint64_t)(uint32_t){g(rt)}'
        return f'{{ uint64_t value={expr};st->lo.ud0=ee_aot_sext32((uint32_t)value);st->hi.ud0=ee_aot_sext32((uint32_t)(value>>32));{assign(rd,"st->lo.ud0")} }}',False
    if op==0 and fn in (26,27):
        return f'ee_aot_div(st,(uint32_t){g(rs)},(uint32_t){g(rt)},{int(fn==26)}u);',False
    memory = {32:(8,True),33:(16,True),35:(32,True),36:(8,False),37:(16,False),39:(32,False),55:(64,False)}
    if op in memory:
        width, signed = memory[op]
        value = f'ee_mem_read{width}(st, (uint32_t){g(rs)} + (uint32_t)({imm}))'
        if not rt:
            return f'(void){value};', False
        if signed:
            value = f'(uint64_t)(int64_t)(int{width}_t){value}'
        return assign(rt, value), False
    if op in (40,41,43,63):
        width = {40:8,41:16,43:32,63:64}[op]
        return f'ee_mem_write{width}(st, (uint32_t){g(rs)} + (uint32_t)({imm}), (uint{width}_t){g(rt)});', False
    if op == 9:
        return assign(rt, s32(f'(uint32_t){g(rs)} + (uint32_t)({imm})')), False
    if op == 25:
        return assign(rt, f'{g(rs)} + (uint64_t)(int64_t)({imm})'), False
    if op in (10, 11):
        a, b = (f'(int64_t){g(rs)}', f'(int64_t)({imm})') if op == 10 else (g(rs), f'(uint64_t)(int64_t)({imm})')
        return assign(rt, f'({a} < {b})'), False
    if op in (12, 13, 14):
        return assign(rt, f'{g(rs)} { {12:"&",13:"|",14:"^"}[op]} UINT64_C({uimm})'), False
    if op == 15 and rs == 0:
        return assign(rt, s32(f'UINT32_C(0x{uimm << 16:08x})')), False
    if op == 0:
        if fn in (0, 2, 3, 4, 6, 7):
            shift = str(sa) if fn in (0, 2, 3) else f'((uint32_t){g(rs)} & 31u)'
            value = f'(uint32_t){g(rt)}'
            if fn in (3, 7):
                value = f'(uint32_t)ee_aot_sra64(ee_aot_sext32((uint32_t){g(rt)}), {shift})'
            else:
                value = f'{value} {"<<" if fn in (0,4) else ">>"} {shift}'
            return assign(rd, s32(value)), False
        if fn in (20, 22, 23, 56, 58, 59, 60, 62, 63):
            shift = f'({g(rs)} & 63u)' if fn in (20, 22, 23) else str(sa + (32 if fn >= 60 else 0))
            expr = f'ee_aot_sra64({g(rt)}, {shift})' if fn in (23, 59, 63) else f'{g(rt)} {"<<" if fn in (20,56,60) else ">>"} {shift}'
            return assign(rd, expr), False
        if fn in (33, 35):
            return assign(rd, s32(f'(uint32_t){g(rs)} {"+" if fn == 33 else "-"} (uint32_t){g(rt)}')), False
        if fn in (36, 37, 38, 39, 45, 47):
            oper = {36:'&',37:'|',38:'^',39:'|',45:'+',47:'-'}[fn]
            expr = f'({g(rs)} {oper} {g(rt)})'
            return assign(rd, '~' + expr if fn == 39 else expr), False
        if fn in (42, 43):
            a, b = (f'(int64_t){g(rs)}', f'(int64_t){g(rt)}') if fn == 42 else (g(rs), g(rt))
            return assign(rd, f'({a} < {b})'), False
        if fn in (10, 11):
            return f'if({g(rt)} {"==" if fn == 10 else "!="} 0u) {{ {assign(rd,g(rs))} }}', False
        if fn in (16, 18):
            return assign(rd, f'st->{"hi" if fn == 16 else "lo"}.ud0'), False
        if fn in (17, 19):
            return f'st->{"hi" if fn == 17 else "lo"}.ud0 = {g(rs)};', False
        if fn == 15:  # Existing core models SYNC as a no-op.
            return '(void)st;', False
        if fn in (8, 9):
            link = assign(rd, f'UINT64_C(0x{(pc+8)&0xffffffff:08x})') if fn == 9 else ''
            return f'{{ uint32_t target=(uint32_t){g(rs)}; {link} st->next_pc=target; st->branch_pending=1; }}', True
    if op in (2, 3):
        jump = ((pc + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2)
        link = assign(31, f'UINT64_C(0x{(pc+8)&0xffffffff:08x})') if op == 3 else ''
        return f'{link} st->next_pc=UINT32_C(0x{jump:08x}); st->branch_pending=1;', True
    if op in (4, 5, 6, 7, 20, 21, 22, 23) or op == 1 and rt in (0, 1, 2, 3, 16, 17, 18, 19):
        if op == 1:
            likely = rt in (2, 3, 18, 19)
            link = assign(31, f'UINT64_C(0x{(pc+8)&0xffffffff:08x})') if rt >= 16 else ''
            cond = f'(int64_t){g(rs)} {"<" if rt % 2 == 0 else ">="} 0'
        else:
            likely, link = op >= 20, ''
            base = op - 16 if likely else op
            cond = {4:f'{g(rs)} == {g(rt)}', 5:f'{g(rs)} != {g(rt)}', 6:f'(int64_t){g(rs)} <= 0', 7:f'(int64_t){g(rs)} > 0'}[base]
        taken = f'st->next_pc=UINT32_C(0x{target:08x}); st->branch_pending=1;'
        other = f'st->pc=UINT32_C(0x{(pc+8)&0xffffffff:08x}); st->next_pc=UINT32_C(0x{(pc+12)&0xffffffff:08x});' if likely else 'st->branch_pending=1;'
        return f'{link} if({cond}) {{ {taken} }} else {{ {other} }}', True
    return None


def load_segments(args):
    data = Path(args.elf or args.image).read_bytes()
    if args.image:
        base = number(args.base)
        if len(data) % 4 or base % 4 or base + len(data) > 0x100000000:
            raise ValueError('raw image must be aligned and fit in uint32 address space')
        return [(base, data)]
    if len(data) < 52 or data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('expected ELF32 little-endian file')
    if struct.unpack_from('<H', data, 18)[0] != 8:
        raise ValueError('expected MIPS ELF')
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)
    if entsize < 32 or phoff + entsize * count > len(data):
        raise ValueError('truncated ELF program headers')
    segments = []
    for i in range(count):
        kind, offset, addr, _, size, memsize, flags, _ = struct.unpack_from('<8I', data, phoff + i * entsize)
        if kind != 1 or not flags & 1:
            continue
        if offset + size > len(data) or size > memsize or addr + size > 0x100000000:
            raise ValueError('invalid executable segment')
        segments.append((addr, data[offset:offset + size]))
    if not segments:
        raise ValueError('no executable PT_LOAD segment')
    return segments


def read_blocks(args):
    if args.manifest:
        blocks = json.loads(Path(args.manifest).read_text())['blocks']
        return [(number(b['pc']), [number(w) for w in b['words']]) for b in blocks]
    if args.profile:
        candidates = json.loads(Path(args.profile).read_text())['blocks']
        blocks = []
        for candidate in sorted(candidates, key=lambda b: -int(b.get('selection_weight',b.get('hits',0)))):
            pc = number(candidate['pc'])
            words = supported_prefix(pc, candidate['words'], args.max_words)
            if words:
                blocks.append((pc, words))
                if len(blocks) == args.max_blocks:
                    break
        if not blocks:
            raise ValueError('profile has no supported blocks')
        return blocks
    if not args.pc:
        raise ValueError('supply explicit --pc entry points; data is never guessed to be code')
    segments = load_segments(args)
    blocks = []
    for value in args.pc:
        pc = number(value)
        matching = [(base, data) for base, data in segments if base <= pc < base + len(data)]
        if len(matching) != 1:
            raise ValueError(f'entry {pc:#x} has missing/ambiguous executable backing')
        base, data = matching[0]
        values = []
        for i in range(args.max_words):
            off = pc - base + i * 4
            if off + 4 > len(data):
                break
            word = struct.unpack_from('<I', data, off)[0]
            values.append(word)
        words = supported_prefix(pc, values, args.max_words)
        if not words:
            raise ValueError(f'entry {pc:#x} starts with an unsupported opcode')
        blocks.append((pc, words))
    return blocks


def pure_window_candidate(word):
    # Integer subset of ee_jit_event_candidate; no memory, exceptions,
    # branches, helpers or COP state changes in this AOT window.
    op, fn = word >> 26, word & 63
    return op in (9,12,13,14,15,25) or op == 0 and fn in (
        0,2,3,4,6,7,33,35,36,37,38,39,42,43,45,47)


def generate(blocks):
    if len(blocks) > 4096:
        raise ValueError('at most 4096 explicitly selected blocks')
    seen = set()
    output = ['/* GPL-3.0-or-later. Locally generated EE blocks; do not distribute private guest code. */',
              'static inline uint64_t ee_aot_sext32(uint32_t x) { return (uint64_t)x | ((x & 0x80000000u) ? UINT64_C(0xffffffff00000000) : 0u); }',
              'static inline uint64_t ee_aot_sra64(uint64_t x, unsigned n) { return n ? (x >> n) | ((x >> 63) ? (~UINT64_C(0) << (64u-n)) : 0u) : x; }']
    output += ['static void ee_aot_signed(ee_state_t *st,uint64_t a,uint64_t b,unsigned bits,unsigned sub,unsigned dest){uint64_t mask=bits==32u?UINT32_MAX:UINT64_MAX,sign=UINT64_C(1)<<(bits-1u);a&=mask;b&=mask;uint64_t r=(sub?a-b:a+b)&mask;if(((a^r)&(sub?(a^b):~(a^b))&sign)!=0u){ee_core_raise_overflow(st);return;}if(dest)st->gpr[dest].ud0=bits==32u?ee_aot_sext32((uint32_t)r):r;}',
               'static void ee_aot_div(ee_state_t *st,uint32_t a,uint32_t b,unsigned s){if(!s){st->lo.ud0=b?ee_aot_sext32(a/b):UINT64_MAX;st->hi.ud0=ee_aot_sext32(b?a%b:a);}else if(a==0x80000000u&&b==UINT32_MAX){st->lo.ud0=ee_aot_sext32(a);st->hi.ud0=0;}else if(b){st->lo.ud0=ee_aot_sext32((uint32_t)((int32_t)a/(int32_t)b));st->hi.ud0=ee_aot_sext32((uint32_t)((int32_t)a%(int32_t)b));}else{st->lo.ud0=(int32_t)a<0?1:UINT64_MAX;st->hi.ud0=ee_aot_sext32(a);}}']
    entries = []
    for index, (pc, words) in enumerate(sorted(blocks)):
        if pc % 4 or not pc or pc in seen or not 1 <= len(words) <= 32 or pc + len(words) * 4 > 0x100000000:
            raise ValueError('unaligned, duplicate, overflowing, or oversized block')
        seen.add(pc)
        output += [f'static unsigned ee_aot_{index}(ee_state_t *st, unsigned budget)', '{', '    unsigned n=0;']
        prefix = 0
        while prefix < len(words) and pure_window_candidate(words[prefix]):
            prefix += 1
        if prefix >= 2:
            constants = ','.join(f'UINT32_C(0x{w:08x})' for w in words[:prefix])
            output += [f'    static const uint32_t window_words[] = {{{constants}}};',
                       f'    unsigned fast=ee_core_native_window_begin_aot(st,UINT32_C(0x{pc:08x}),window_words,{prefix}u,budget);',
                       '    if(fast) {']
            for i, word in enumerate(words[:prefix]):
                output += [f'      if(fast>{i}u) {{ {translate(word,pc+i*4)[0]} }}']
            output += ['      n=fast;ee_core_interleave_account(fast);',
                       f'      if(fast<{prefix}u || n>=budget) return n;', '    }']
        for i, word in enumerate(words):
            address = pc + i * 4
            result = translate(word, address)
            if (result is None or result[1] and i != len(words)-1 and
                not (i == len(words)-2 and has_delay_slot(word) and
                     translate(words[i+1],address+4) is not None and
                     not translate(words[i+1],address+4)[1])):
                raise ValueError(f'unsupported opcode or nonterminal branch at {address:#x}')
            if complex_family(word) and word>>26 not in (32,33,35,36,37,39,55,40,41,43,63):
                output += [f'    if(!ee_aot_complex_available(UINT32_C(0x{word:08x}))) return n;']
            output += [f'    if(n<={i}u) {{ unsigned was_delay=st->branch_pending;',
                       f'    if(n>=budget || !ee_aot_prepare(st, UINT32_C(0x{address:08x}), UINT32_C(0x{word:08x}), {int(not result[1])}u, {int(word >> 26 in MEMORY_OPS)}u)) return n;',
                       '    { unsigned quantum=ee_core_interleave_quantum;',
                       f'      {result[0]}',
                       '      if(st->halted) return n;',
                       '      ee_core_block_commit(st); ++n;',
                       '      ee_core_interleave_account(1u);',
                       '      if(was_delay || quantum==1u) return n;', '    }','    }']
        output += ['    return n;', '}']
        entries.append(f'    {{UINT32_C(0x{pc:08x}), UINT32_C(0x{words[0]:08x}), ee_aot_{index}}},')
    output += ['static const ee_aot_entry ee_aot_entries[] = {', *(entries or ['    {0u, 0u, NULL},']), '};', f'static const unsigned ee_aot_entries_count = {len(entries)}u;', '']
    return '\n'.join(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--manifest', help='JSON {blocks:[{pc,words}]}')
    group.add_argument('--profile', help='observed JSON {blocks:[{pc,words,hits}]}')
    group.add_argument('--image', help='local little-endian raw BIOS/code image')
    group.add_argument('--elf', help='local PS2 MIPS ELF32 image')
    parser.add_argument('--base', default='0xbfc00000')
    parser.add_argument('--pc', action='append')
    parser.add_argument('--max-words', type=int, default=32, choices=range(1,33))
    parser.add_argument('--max-blocks', type=int, default=1024, help='profile block limit, 1..4096; Wii default reserves MEM1 space')
    parser.add_argument('--output', required=True)
    parser.add_argument('--complex-output', help='required output for shared COP/MMI/memory specializations')
    args = parser.parse_args()
    if not 1 <= args.max_blocks <= 4096:
        parser.error('--max-blocks must be between 1 and 4096')
    try:
        blocks = read_blocks(args)
        generated = generate(blocks)
        complex_words=any(complex_family(w) for _,ws in blocks for w in ws if w>>26 not in (32,33,35,36,37,39,55,40,41,43,63))
        if complex_words and not args.complex_output:
            raise ValueError('--complex-output is required for COP/MMI/wide-memory blocks')
        if args.complex_output:Path(args.complex_output).write_text(generate_complex(blocks))
        Path(args.output).write_text(generated)
        print(json.dumps({'blocks':len(blocks), 'instructions':sum(len(w) for _, w in blocks), 'sha256':hashlib.sha256(generated.encode()).hexdigest(), 'wii_verified':False}))
    except (ValueError, KeyError, TypeError, OSError, struct.error) as error:
        parser.exit(2, f'ee_aot_generate: {error}\n')


if __name__ == '__main__':
    main()
