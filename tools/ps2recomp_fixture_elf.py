#!/usr/bin/env python3
"""GPL-3.0-or-later. Build the tiny synthetic MIPS ELF32 used by Batch A11.

The ELF contains only hand-assembled public integer words and one FUNC
symbol; it is not derived from any game, BIOS or disc. It exists so the
pinned upstream PS2Recomp generator can be run on a reproducible input.
"""
import struct
import sys

BASE = 0x00100000
# (symbol, address, words). Hand-assembled public integer words only.
# a11_int_leaf: addiu t0,zero,0x1234 / addiu t1,t0,-1 / addu t2,t0,t1 /
#               xor t3,t2,t0 / sll t4,t3,4 / nop
# a11_zero_dest: addu zero,t0,t1 / xor zero,t0,t1 / addiu t0,t0,-0x8000 /
#               sll t1,t1,31 / addiu t1,t1,0x7fff / addu t2,t2,t2 / xor t3,t3,t3
FUNCTIONS = [
    ('a11_int_leaf', 0x00100000,
     [0x24081234, 0x2509ffff, 0x01095021, 0x01485826, 0x000b6100, 0x00000000]),
    ('a11_zero_dest', 0x00100018,
     [0x01090021, 0x01090026, 0x25088000, 0x00094fc0, 0x25297fff, 0x014a5021, 0x016b5826]),
]


def build(functions=FUNCTIONS):
    base = functions[0][1]
    text = b''
    for _, addr, words in functions:
        assert base + len(text) == addr, 'functions must be contiguous'
        text += struct.pack('<%dI' % len(words), *words)
    strtab = b'\0'
    symtab = bytes(16)
    for name, addr, words in functions:
        symtab += struct.pack('<IIIBBH', len(strtab), addr, 4 * len(words), 0x12, 0, 1)
        strtab += name.encode() + b'\0'
    shstr = b'\0.text\0.symtab\0.strtab\0.shstrtab\0'
    off = lambda s: shstr.index(s.encode())
    pos = 52 + 32  # ehdr + one phdr
    t_off = pos
    sym_off = t_off + len(text)
    str_off = sym_off + len(symtab)
    shs_off = str_off + len(strtab)
    sh_off = (shs_off + len(shstr) + 3) & ~3
    ehdr = b'\x7fELF\x01\x01\x01' + bytes(9) + struct.pack(
        '<HHIIIIIHHHHHH', 2, 8, 1, base, 52, sh_off, 0x20924001, 52, 32, 1, 40, 5, 4)
    phdr = struct.pack('<8I', 1, t_off, base, base, len(text), len(text), 5, 4)
    sh = lambda *a: struct.pack('<10I', *a)
    shdrs = (bytes(40) +
             sh(off('.text'), 1, 6, base, t_off, len(text), 0, 0, 4, 0) +
             sh(off('.symtab'), 2, 0, 0, sym_off, len(symtab), 3, 1, 4, 16) +
             sh(off('.strtab'), 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0) +
             sh(off('.shstrtab'), 3, 0, 0, shs_off, len(shstr), 0, 0, 1, 0))
    blob = ehdr + phdr + text + symtab + strtab + shstr
    blob += bytes(sh_off - len(blob)) + shdrs
    return blob


if __name__ == '__main__':
    open(sys.argv[1], 'wb').write(build())
