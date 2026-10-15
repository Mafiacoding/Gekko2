#!/usr/bin/env python3
"""Execute independent native-ABI blocks. Synthetic PPC evidence, not Wii FPS."""
import ctypes as C
import random
import struct
import subprocess
import tempfile
from pathlib import Path

from unicorn import Uc, UC_ARCH_PPC, UC_MODE_PPC32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.ppc_const import UC_PPC_REG_0, UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_PC, UC_PPC_REG_LR

ROOT = Path(__file__).resolve().parents[1]
CODE, STATE, STACK, STOP = 0x10000, 0x20000, 0x3f000, 0x18000
PREP, COMMIT, RESOLVE = 0x70000100, 0x70000200, 0x70000300
GEN = 1024


class Context(C.Structure):
    _fields_ = [('code', C.POINTER(C.c_uint32)), ('capacity', C.c_size_t), ('used', C.c_size_t)]


class Map(C.Structure):
    _fields_ = [('offsets', C.c_uint16 * 12), ('count', C.c_uint), ('generation_offset', C.c_uint)]


def packed(values):
    return struct.pack('>' + str(len(values)) + 'I', *values)


def mmi(rs, rt, rd, kind):
    return (28 << 26) | (rs << 21) | (rt << 16) | (rd << 11) | ((18 + (kind & 1)) << 6) | (9 if kind < 2 else 41)


def oracle(state, word):
    op, rs, rt, rd, fn = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, word & 63
    if op == 28 and fn in (9, 41):
        kind = (0 if fn == 9 else 2) + (((word >> 6) & 31) - 18)
        a, b = state[rs*4:rs*4+4], state[rt*4:rt*4+4]
        out = [(x & y if kind == 0 else x ^ y if kind == 1 else x | y if kind == 2 else ~(x | y) & 0xffffffff) for x, y in zip(a, b)]
        if rd:
            state[rd*4:rd*4+4] = out
    elif op == 0 and fn in (16, 17, 18, 19):
        src = rs if fn in (17, 19) else 32 if fn == 16 else 33
        dst = rd if fn in (16, 18) else 32 if fn == 17 else 33
        if dst:
            state[dst*4:dst*4+2] = state[src*4:src*4+2]
    elif op == 0 and fn == 0x29:
        state[138] = state[rs*4+1]
    elif op == 0 and fn == 0x28:
        if rd:
            state[rd*4:rd*4+2] = [0, state[138]]
    else:
        raise AssertionError(hex(word))


with tempfile.TemporaryDirectory() as directory:
    so = Path(directory) / 'emitter.so'
    subprocess.run(['gcc', '-shared', '-fPIC', '-O1', '-Iinclude', '-Isource',
                    'source/core/recompiler/ppc_dynarec.c', 'source/core/recompiler/ppc_code_cache.c',
                    'source/core/recompiler/optimization.c', '-lm', '-o', str(so)], cwd=ROOT, check=True)
    lib = C.CDLL(str(so))
    lib.ppc_dynarec_init.argtypes = [C.POINTER(Context), C.c_size_t]
    lib.ppc_dynarec_free.argtypes = [C.POINTER(Context)]
    lib.ppc_dynarec_translate_ee_resident_chain.argtypes = [C.POINTER(Context), C.c_uint32, C.POINTER(Map)]
    lib.ppc_dynarec_translate_ee_chain_block.argtypes = [C.POINTER(Context), C.c_uint32, C.POINTER(C.c_uint32), C.c_uint,
                                                        C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.POINTER(Map)]
    C.c_uint32.in_dll(lib, 'gekko2_optimization_mask').value = 0xffffffff

    def emit(address, mapping, words=None, pc=0x100000):
        ctx = Context()
        assert lib.ppc_dynarec_init(C.byref(ctx), 16) == 0
        if words is None:
            result = lib.ppc_dynarec_translate_ee_resident_chain(C.byref(ctx), RESOLVE, C.byref(mapping))
        else:
            array = (C.c_uint32 * len(words))(*words)
            result = lib.ppc_dynarec_translate_ee_chain_block(C.byref(ctx), pc, array, len(words), PREP, 0, 0, COMMIT, C.byref(mapping))
        if result:
            assert ctx.used == 0, 'declined native map/body changed output'
            lib.ppc_dynarec_free(C.byref(ctx))
            return None
        values = list(ctx.code[:ctx.used]) + [0x4e800020]
        # ee_block_call can emit a relative BL when the host allocation happens
        # to be close to a callback. Relocate those calls to the emulated site.
        old = C.addressof(ctx.code.contents) & 0xffffffff
        for n, word in enumerate(values):
            if word >> 26 == 18 and word & 1 and not word & 2:
                delta = word & 0x03fffffc
                if delta & 0x02000000:
                    delta -= 0x04000000
                target = (old + n*4 + delta) & 0xffffffff
                relative = target - (address + n*4)
                assert -33554432 <= relative <= 33554428
                values[n] = 0x48000001 | (relative & 0x03fffffc)
        lib.ppc_dynarec_free(C.byref(ctx))
        return packed(values)

    rng, cases = random.Random(20261015), 0
    programs = [
        [mmi(1,2,3,0), mmi(3,2,1,1), mmi(1,3,2,2), mmi(2,1,3,3), mmi(2,3,1,0), mmi(1,3,2,1)],
        [(1<<21)|17, (2<<21)|19, (3<<11)|16, (1<<11)|18, (1<<21)|0x29, (2<<11)|0x28],
    ]
    maps = [list(range(16,64,4)), [16,20,24,28,512,516,520,524,528,532,536,540], [16,20,24,28,32,36,40,44,552]]
    for offsets in maps:
        mapping = Map((C.c_uint16*12)(*offsets), len(offsets), GEN)
        wrapper = emit(CODE, mapping)
        for program in programs:
            blocks = [emit(CODE+0x1000+i*0x1000, mapping, program[i*2:i*2+2], 0x100000+i*8) for i in range(3)]
            assert all(blocks)
            for trial in range(30):
                for fault in (-1, 0, 1, 3, 5):
                    for mutation in (-1, 2):
                        u = Uc(UC_ARCH_PPC, UC_MODE_PPC32 | UC_MODE_BIG_ENDIAN)
                        for base, size in [(CODE,0x10000),(STATE,0x10000),(0x30000,0x10000),(0x70000000,0x10000)]:
                            u.mem_map(base,size)
                        u.mem_write(CODE, wrapper)
                        for i, body in enumerate(blocks):
                            u.mem_write(CODE+0x1000+i*0x1000, body)
                        expected = [rng.getrandbits(32) for _ in range(139)]
                        expected[:4] = [0]*4
                        expected[136:138] = [0x100000,0x100004]
                        u.mem_write(STATE, packed(expected))
                        u.mem_write(STATE+GEN, packed([0xffffffff]))
                        saved = [rng.getrandbits(32) for _ in range(18)]
                        for n, value in enumerate(saved):
                            u.reg_write(UC_PPC_REG_0+14+n,value)
                        for n, value in enumerate([STATE,0,0,6,CODE+0x1000,2]):
                            u.reg_write(UC_PPC_REG_3+n,value)
                        u.reg_write(UC_PPC_REG_1,STACK)
                        u.reg_write(UC_PPC_REG_LR,STOP)
                        steps, reads = [0], [0]
                        budget_calls = []

                        def hook(uc, address, size, user):
                            if address not in (PREP,COMMIT,RESOLVE):
                                return
                            assert uc.reg_read(UC_PPC_REG_3) == STATE
                            if address != COMMIT:
                                assert bytes(uc.mem_read(STATE,556)) == packed(expected), ('helper observed stale state',steps[0])
                            lr, sp = uc.reg_read(UC_PPC_REG_LR), uc.reg_read(UC_PPC_REG_1)
                            uc.mem_write(sp+4,packed([lr]))
                            uc.mem_write(sp+8,bytes.fromhex('cafebabe')*8)
                            ret, count = 1, 0
                            if address == PREP:
                                assert uc.reg_read(UC_PPC_REG_4) == 0x100000+steps[0]*4
                                if steps[0] == mutation:
                                    # Existing helper mutation generation wraps through zero.
                                    expected[4] ^= 0xdeadbeef
                                    uc.mem_write(STATE+16,packed([expected[4]]))
                                    uc.mem_write(STATE+GEN,packed([0]))
                                if steps[0] == fault:
                                    ret = 0
                            elif address == COMMIT:
                                oracle(expected, program[steps[0]])
                                assert bytes(uc.mem_read(STATE,556)) == packed(expected), ('oracle mismatch',steps[0])
                                steps[0] += 1
                            else:
                                remaining = uc.reg_read(UC_PPC_REG_4)
                                budget_calls.append(remaining)
                                next_block = steps[0]//2
                                ret,count = (CODE+0x1000+next_block*0x1000,2) if remaining>=2 and next_block<3 else (0,0)
                            for reg in range(3,13):
                                uc.reg_write(UC_PPC_REG_0+reg,0xcafe0000+reg)
                            uc.reg_write(UC_PPC_REG_3,ret)
                            uc.reg_write(UC_PPC_REG_4,count)
                            uc.reg_write(UC_PPC_REG_PC,lr)

                        def read(uc, access, address, size, value, user):
                            if address-STATE in offsets:
                                reads[0] += 1

                        u.hook_add(UC_HOOK_CODE,hook)
                        u.hook_add(UC_HOOK_MEM_READ,read)
                        u.emu_start(CODE,STOP,count=20000)
                        want = 6 if fault<0 else fault
                        assert steps[0] == want == u.reg_read(UC_PPC_REG_3)
                        assert bytes(u.mem_read(STATE,556)) == packed(expected)
                        assert u.reg_read(UC_PPC_REG_1) == STACK
                        assert [u.reg_read(UC_PPC_REG_0+14+n) for n in range(18)] == saved
                        if mutation<0:
                            assert reads[0] == len(offsets), ('resident lane unnecessarily reloaded',reads[0],len(offsets))
                        cases += 1
    # Invalid maps and unsupported bodies decline without output mutation.
    for offsets, generation in [([0],GEN),([544],GEN),([548],GEN),([556],GEN),([65532],GEN),([16,16],GEN),([16],16),([16],0)]:
        bad = Map((C.c_uint16*12)(*offsets),len(offsets),generation)
        assert emit(CODE,bad) is None
    mapping = Map((C.c_uint16*12)(*maps[0]),12,GEN)
    for unsupported in [(35<<26)|(1<<21)|(2<<16), (4<<26)|1, 12]:
        assert emit(CODE,mapping,[unsupported,0]) is None
    print(f'PASS {cases} executed PPC cross-block 128-bit/HI/LO/SA, clean helper state, mutation-wrap, partial-exit and EABI cases; 11 decline cases')
