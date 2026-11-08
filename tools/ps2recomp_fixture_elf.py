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


def _i(op, rs, rt, imm):
    return op << 26 | rs << 21 | rt << 16 | imm & 0xffff


def _r(rs, rt, rd, fn):
    return rs << 21 | rt << 16 | rd << 11 | fn


# Batch A12: SLTI/SLTIU/ANDI/ORI/XORI/LUI and OR/AND/NOR/SLT/SLTU/SUBU.
FUNCTIONS_A12 = [
    ('a12_imm_leaf', 0x00100000, [
        _i(0x0f, 0, 8, 0x8001),      # lui   t0,0x8001
        _i(0x0d, 8, 8, 0xffff),      # ori   t0,t0,0xffff
        _i(0x0c, 8, 9, 0xff00),      # andi  t1,t0,0xff00
        _i(0x0e, 9, 10, 0x1234),     # xori  t2,t1,0x1234
        _i(0x0a, 10, 11, -1),        # slti  t3,t2,-1
        _i(0x0b, 10, 12, -1),        # sltiu t4,t2,-1
        _i(0x0a, 8, 13, 0x7fff),     # slti  t5,t0,0x7fff
        _i(0x0b, 0, 14, 1),          # sltiu t6,zero,1
    ]),
    ('a12_reg_leaf', 0x00100020, [
        _r(8, 9, 10, 0x25),          # or   t2,t0,t1
        _r(8, 9, 11, 0x24),          # and  t3,t0,t1
        _r(8, 9, 12, 0x27),          # nor  t4,t0,t1
        _r(8, 9, 13, 0x2a),          # slt  t5,t0,t1
        _r(8, 9, 14, 0x2b),          # sltu t6,t0,t1
        _r(8, 9, 15, 0x23),          # subu t7,t0,t1
        _r(10, 10, 10, 0x23),        # subu t2,t2,t2
        _r(9, 8, 9, 0x2a),           # slt  t1,t1,t0
    ]),
    ('a12_zero_dest', 0x00100040, [
        _i(0x0f, 0, 0, 1),           # lui   zero,1
        _i(0x0d, 8, 0, 5),           # ori   zero,t0,5
        _i(0x0c, 8, 0, 0xffff),      # andi  zero,t0,0xffff
        _i(0x0e, 8, 0, 3),           # xori  zero,t0,3
        _i(0x0a, 8, 0, -2),          # slti  zero,t0,-2
        _i(0x0b, 8, 0, -2),          # sltiu zero,t0,-2
        _r(8, 9, 0, 0x25),           # or    zero,t0,t1
        _r(8, 9, 0, 0x24),           # and   zero,t0,t1
        _r(8, 9, 0, 0x27),           # nor   zero,t0,t1
        _r(8, 9, 0, 0x2a),           # slt   zero,t0,t1
        _r(8, 9, 0, 0x2b),           # sltu  zero,t0,t1
        _r(8, 9, 0, 0x23),           # subu  zero,t0,t1
        _i(0x0d, 8, 8, 0),           # ori   t0,t0,0 (register copy)
    ]),
]
# Batch A13: JR $ra with delay slot ends otherwise straight-line integer leaves.
JR_RA = 0x03e00008
FUNCTIONS_A13 = [
    ('a13_ret_nop', 0x00100000, [
        _i(0x09, 0, 2, 7),           # addiu v0,zero,7
        _r(4, 5, 2, 0x21),           # addu  v0,a0,a1 (v0 = a0 + a1)
        JR_RA,                       # jr    ra
        0x00000000,                  # nop   (delay slot, elided by upstream)
    ]),
    ('a13_ret_slot', 0x00100010, [
        _r(4, 5, 2, 0x26),           # xor   v0,a0,a1
        JR_RA,                       # jr    ra
        _i(0x09, 2, 2, 1),           # addiu v0,v0,1 (delay slot)
    ]),
    ('a13_ret_only', 0x0010001c, [
        JR_RA,                       # jr    ra
        _i(0x0d, 0, 3, 0xbeef),      # ori   v1,zero,0xbeef (delay slot)
    ]),
    ('a13_plain_hex', 0x00100024, [
        _i(0x09, 4, 4, 1),           # addiu a0,a0,1
        0x00000000,                  # nop (fallthrough end 0x10002c has a hex letter)
    ]),
    ('a13_ra_slot', 0x0010002c, [
        _i(0x09, 31, 2, 0),          # addiu v0,ra,0
        JR_RA,                       # jr    ra (target is read before the slot)
        _i(0x0d, 0, 31, 0x1234),     # ori   ra,zero,0x1234 (delay slot clobbers ra)
    ]),
    ('a13_nop_alias_slot', 0x00100038, [
        _r(4, 5, 3, 0x2b),           # sltu  v1,a0,a1
        JR_RA,                       # jr    ra
        0x24000000,                  # addiu zero,zero,0: upstream guest-NOP alias, elided
    ]),
]
# Batch A15: LW and SW (word loads/stores) through a register base.
FUNCTIONS_A15 = [
    ('a15_lw_sw', 0x00100000, [
        _i(0x23, 4, 8, 0),           # lw  t0,0(a0)
        _i(0x23, 4, 9, 4),           # lw  t1,4(a0)
        _r(8, 9, 10, 0x21),          # addu t2,t0,t1
        _i(0x2b, 4, 10, 8),          # sw  t2,8(a0)
        _i(0x23, 4, 0, 12),          # lw  zero,12(a0) (read performed, result discarded)
        _i(0x2b, 4, 10, -4),         # sw  t2,-4(a0)
        _i(0x23, 4, 11, 0x7ffc),     # lw  t3,0x7ffc(a0)
        _i(0x2b, 4, 0, 16),          # sw  zero,16(a0)
    ]),
    ('a15_alias', 0x00100020, [
        _i(0x23, 4, 4, 0),           # lw  a0,0(a0) (rt == rs)
        _i(0x2b, 4, 4, 4),           # sw  a0,4(a0)
        _i(0x23, 5, 5, -8),          # lw  a1,-8(a1)
    ]),
    ('a15_ret_store_slot', 0x0010002c, [
        _i(0x23, 4, 2, 0),           # lw v0,0(a0)
        JR_RA,                       # jr ra
        _i(0x2b, 5, 2, 0),           # sw v0,0(a1) (delay slot store)
    ]),
    ('a15_ret_load_slot', 0x00100038, [
        JR_RA,                       # jr ra
        _i(0x23, 4, 3, 8),           # lw v1,8(a0) (delay slot load)
    ]),
    ('a15_hinted', 0x00100040, [
        _i(0x0f, 0, 8, 0x8030),      # lui t0,0x8030
        _i(0x23, 8, 9, 0),           # lw  t1,0(t0)  (upstream folds the constant base)
        _i(0x2b, 8, 9, 4),           # sw  t1,4(t0)
        _i(0x23, 4, 10, 0),          # lw  t2,0(a0)  (base not constant)
    ]),
]
SETS = {'a15': FUNCTIONS_A15, 'a11': FUNCTIONS, 'a12': FUNCTIONS_A12, 'a13': FUNCTIONS_A13}


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
    open(sys.argv[1], 'wb').write(build(SETS[sys.argv[2]] if len(sys.argv) > 2 else FUNCTIONS))
