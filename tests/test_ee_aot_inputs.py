"""Exercise CLI inputs and validation before any generated output is replaced."""
import json
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class InputTests(unittest.TestCase):
    def test_mem1_boundaries(self):
        spec=importlib.util.spec_from_file_location('memory',ROOT/'tools/verify_wii_memory.py')
        memory=importlib.util.module_from_spec(spec);spec.loader.exec_module(memory)
        elf=bytearray(84);elf[:7]=b'\x7fELF\x01\x02\x01'
        struct.pack_into('>H',elf,18,20);struct.pack_into('>I',elf,28,52);struct.pack_into('>HH',elf,42,32,1)
        def segment(end):struct.pack_into('>8I',elf,52,1,84,0x80003f00,0,0,end-0x80003f00,6,4)
        segment(0x81600000)
        self.assertEqual(memory.check(elf)['mem1_remaining_bytes'],2*1024*1024)
        segment(0x8183e800)
        with self.assertRaises(ValueError):memory.check(elf)
        segment(0x81780000)
        with self.assertRaises(ValueError):memory.check(elf)
    def test_raw_elf_profile_and_rejected_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);out=d/'output.inc'
            def run(*args,ok=True):
                result=subprocess.run(['python3',str(ROOT/'tools/ee_aot_generate.py'),*map(str,args),'--output',str(out)],capture_output=True,text=True)
                self.assertEqual(result.returncode,0 if ok else 2,result.stderr)
                return result
            raw=d/'code.bin';raw.write_bytes(struct.pack('<4I',0x24010007,0x24220008,0x0000000c,0))
            result=run('--image',raw,'--base','0x80200000','--pc','0x80200000')
            self.assertEqual(json.loads(result.stdout)['instructions'],2)
            original=out.read_bytes()
            run('--image',raw,'--base','0x80200000','--pc','0x80200008',ok=False)
            self.assertEqual(out.read_bytes(),original)
            run('--image',raw,ok=False)
            elf=bytearray(84)+raw.read_bytes();elf[:7]=b'\x7fELF\x01\x01\x01'
            struct.pack_into('<H',elf,18,8);struct.pack_into('<I',elf,28,52);struct.pack_into('<HH',elf,42,32,1)
            struct.pack_into('<8I',elf,52,1,84,0x80200000,0,16,16,5,4)
            image=d/'game.elf';image.write_bytes(elf)
            result=run('--elf',image,'--pc','0x80200000')
            self.assertEqual(json.loads(result.stdout)['instructions'],2)
            image.write_bytes(elf[:60]);run('--elf',image,'--pc','0x80200000',ok=False)
            profile=d/'profile.json';profile.write_text(json.dumps({'blocks':[{'pc':'0x80200000','words':['0x24010007',12],'hits':100},{'pc':'0x80201000','words':[12],'hits':1000}]}))
            result=run('--profile',profile)
            self.assertEqual(json.loads(result.stdout)['blocks'],1)
            self.assertEqual(json.loads(result.stdout)['instructions'],1)
            run('--profile',profile,'--max-blocks','0',ok=False)
            manifest=d/'manifest.json';manifest.write_text(json.dumps({'blocks':[{'pc':0xffffffff,'words':[0]}]}))
            run('--manifest',manifest,ok=False)
            manifest.write_text(json.dumps({'blocks':[{'pc':'0x80200000','words':['0x40026000']}]}))
            run('--manifest',manifest,ok=False)
            run('--manifest',manifest,'--complex-output',d/'complex.inc')
            self.assertIn('ee_aot_complex_available',(d/'complex.inc').read_text())
            manifest.write_text(json.dumps({'blocks':[{'pc':'0x80200000','words':['0x70221989']}]}))
            run('--manifest',manifest,'--complex-output',d/'complex.inc',ok=False)
    def test_vu_and_iop_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);out=d/'output.inc'
            def run(tool,*args,ok=True):
                r=subprocess.run(['python3',str(ROOT/'tools'/tool),*map(str,args),'--output',str(out)],capture_output=True,text=True)
                self.assertEqual(r.returncode,0 if ok else 2,r.stderr)
            micro=d/'micro.bin';micro.write_bytes(bytes(4096))
            run('vu_aot_generate.py','--micro',micro)
            original=out.read_bytes();micro.write_bytes(bytes(4095));run('vu_aot_generate.py','--micro',micro,ok=False)
            self.assertEqual(out.read_bytes(),original)
            profile=d/'iop.json';profile.write_text(json.dumps({'iop_blocks':[{'pc':'0x80100000','words':['0x24020007'],'hits':10}]}))
            run('iop_aot_generate.py','--profile',profile)
            self.assertIn('iop_core_block_prepare',out.read_text())
            run('iop_aot_generate.py','--profile',profile,'--max-entries','0',ok=False)
            profile.write_text(json.dumps({'iop_blocks':[{'pc':'0x80100000','words':['0x24020007'],'hits':10}]*2}))
            run('iop_aot_generate.py','--profile',profile,ok=False)

if __name__=='__main__':unittest.main()
