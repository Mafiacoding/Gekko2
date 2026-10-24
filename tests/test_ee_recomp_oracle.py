#!/usr/bin/env python3
"""Independent synthetic checks for the isolated EE recompiler oracle."""
from pathlib import Path
import importlib.util,json,random,struct,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('fixture',ROOT/'experiments/ps2recomp-wii/synthetic_elf.py');fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
class Oracle(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.exe=subprocess.check_output(['python3','tools/build_host_core.py','ee_recomp_oracle'],cwd=ROOT,text=True).strip()
 def run_case(self,data,entry='0x200000',limit='32',a=0,b=0,upper=None):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'synthetic.elf';p.write_bytes(data)
   args=[self.exe,str(p),entry,limit,hex(a),hex(b)]
   if upper is not None:args.append(hex(upper))
   return subprocess.run(args,capture_output=True,text=True,timeout=10)
 def test_integer_and_delay_slot(self):
  rng=random.Random(0x5900)
  cases=[(0,0),(1,0),(0x7fffffff,2),(0x80000000,0xffffffff),(0xffffffffffffffff,1),(0x1234567887654321,0xfedcba9876543210)]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(32)]
  for a,b in cases:
   with self.subTest(a=a,b=b):
    p=self.run_case(fixture.build(),a=a,b=b);self.assertEqual(p.returncode,0,p.stderr);s=json.loads(p.stdout)
    self.assertTrue(s['completed']);self.assertEqual(s['retired'],6);self.assertEqual(s['steps'],6)
    self.assertEqual(int(s['gpr'][2][0],16),fixture.expected(a,b))
    self.assertEqual(s['gpr'][0],['0000000000000000']*2)
    self.assertEqual(int(s['gpr'][4][0],16),a);self.assertEqual(int(s['gpr'][5][0],16),b)
    self.assertFalse(s['host_PPC_execution']);self.assertFalse(s['game_boot'])
 def test_upper_lanes_preserved(self):
  seed=0xa5a5123456789000
  p=self.run_case(fixture.build(),a=123,b=456,upper=seed);self.assertEqual(p.returncode,0,p.stderr)
  s=json.loads(p.stdout)
  for r in range(1,32):self.assertEqual(int(s['gpr'][r][1],16),seed^r)
  self.assertEqual(s['gpr'][0],['0000000000000000']*2)
 def test_budget_is_failure(self):
  p=self.run_case(fixture.build(),limit='1');self.assertEqual(p.returncode,1);self.assertFalse(json.loads(p.stdout)['completed'])
 def test_reserved_instruction_is_failure(self):
  data=bytearray(fixture.build());struct.pack_into('<I',data,0x100,0x00000001)
  p=self.run_case(data);self.assertEqual(p.returncode,1);self.assertFalse(json.loads(p.stdout)['completed'])
 def test_validation(self):
  for entry,limit in [('0x200001','32'),('0x300000','32'),('-1','32'),('0x200000','0'),('0x200000','1000001'),('0x200000','-1'),('0x200000','1x')]:
   with self.subTest(entry=entry,limit=limit):self.assertEqual(self.run_case(fixture.build(),entry,limit).returncode,2)
  for data in [b'',fixture.build()[:50],b'bad!'+fixture.build()[4:]]:
   with self.subTest(length=len(data)):self.assertEqual(self.run_case(data).returncode,3)
if __name__=='__main__':unittest.main()
