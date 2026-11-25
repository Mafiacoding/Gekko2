#!/usr/bin/env python3
"""Synthetic CFG selection and private-output tests; no proprietary fixtures."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('hybrid',ROOT/'experiments/ps2recomp-wii/hybrid_compile.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)


def elf(words,entry=0x1000):
    raw=struct.pack('<%dI'%len(words),*words)
    ident=b'\x7fELF\x01\x01\x01'+bytes(9)
    return (ident+struct.pack('<HHIIIIIHHHHHH',2,8,1,entry,52,0,0,52,32,1,0,0,0)+
        struct.pack('<8I',1,84,0x1000,0x1000,len(raw),len(raw),5,4)+raw)

class HybridTests(unittest.TestCase):
    def test_direct_call_return_and_unreachable_data(self):
        # JAL 0x1020, slot, return site; no scanning through JR into fake data.
        words=[0x0c000408,0x24020007,0x03e00008,0,0xffffffff,0xffffffff,0xffffffff,0xffffffff,
               0x24420005,0x03e00008,0]
        blocks,gaps,limited=h.discover([(0x1000,struct.pack('<11I',*words))],[0x1000])
        self.assertEqual({pc for pc,_ in blocks},{0x1000,0x1008,0x1020})
        self.assertFalse(limited)
        self.assertEqual(blocks[0][1],words[:2])
        self.assertNotIn(0x1010,{pc for pc,_ in blocks})
        self.assertEqual(sum(g['reason']=='dynamic_target' for g in gaps),2)

    def test_two_branch_successors_and_delay_fallback(self):
        # Both outcomes are reachable. SYSCALL slot stays with the interpreter.
        words=[0x10220003,0x0000000c,0x03e00008,0,0x24420001,0x03e00008,0]
        blocks,gaps,_=h.discover([(0x1000,struct.pack('<7I',*words))],[0x1000])
        self.assertEqual(blocks[0],(0x1000,[words[0]]))
        self.assertEqual({pc for pc,_ in blocks},{0x1000,0x1008,0x1010})
        self.assertTrue(any(g['reason']=='interpreter_delay_slot' for g in gaps))

    def test_unsupported_and_ambiguous_memory_stop(self):
        data=struct.pack('<4I',0x24020007,0x00000005,0x24020009,0)
        blocks,gaps,_=h.discover([(0x1000,data)],[0x1000])
        self.assertEqual(blocks,[(0x1000,[0x24020007])])
        self.assertEqual(gaps,[{'pc':0x1004,'reason':'interpreter_required'}])
        self.assertEqual(h.discover([(0x1000,data),(0x1000,data)],[0x1000])[0],[])

    def test_limits_loops_and_no_slot_capacity(self):
        words=struct.pack('<4I',0x1000ffff,0,0,0)
        blocks,_,limited=h.discover([(0x1000,words)],[0x1000],1,1)
        self.assertEqual(blocks,[(0x1000,[0x1000ffff])]);self.assertTrue(limited)
        for limits in ((0,32),(4097,32),(2,0),(2,33)):
            with self.assertRaises(ValueError):h.discover([],[],*limits)

    def test_private_elf_output_and_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);p=d/'fixture.elf';p.write_bytes(elf([0x24020007,0x03e00008,0]))
            out=d/'result';report=h.compile_elf(p,out)
            self.assertFalse(report['standalone_game_port']);self.assertEqual(report['blocks'],1)
            self.assertEqual(json.loads((out/'manifest.private.json').read_text())['blocks'][0]['pc'],0x1000)
            self.assertEqual(out.stat().st_mode&0o777,0o700)
            self.assertEqual((out/'ee_aot_generated.inc').stat().st_mode&0o777,0o600)
            with self.assertRaises(FileExistsError):h.compile_elf(p,out)
            with self.assertRaises(ValueError):h.compile_elf(p,ROOT/'should-not-exist')
            with self.assertRaises(ValueError):h.compile_elf(p,d/'bad',[3])
            with self.assertRaises(ValueError):h.compile_elf(p,d/'outside',[0x8000])
            dynamic=bytearray(elf([0x24020007]));struct.pack_into('<H',dynamic,16,3)
            p.write_bytes(dynamic)
            with self.assertRaises(ValueError):h.compile_elf(p,d/'relocatable')
            p.write_bytes(elf([0x24020007])[:70])
            with self.assertRaises(ValueError):h.compile_elf(p,d/'truncated')

if __name__=='__main__':unittest.main()
