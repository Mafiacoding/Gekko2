#!/usr/bin/env python3
"""Generate a synthetic leaf with pinned PS2Recomp; compare host/PPC to Gekko2.
No BIOS/game inputs, device runtime, Wii execution or performance claim.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,random,re,struct,subprocess
PIN='e5636981a1c9de368f6b71e819c3dc188dfedbf4'
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--upstream',type=Path,required=True);p.add_argument('--recompiler',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--devkitppc',type=Path,required=True);p.add_argument('--private-elf',type=Path,help='optional owner ELF; generated source/output must remain private')
p.add_argument('--function',help='generated C++ function identifier, required with --private-elf')
p.add_argument('--entry',type=lambda s:int(s,0),help='guest function address, required with --private-elf')
a=p.parse_args()
if a.private_elf:
 if not a.function or a.entry is None or not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',a.function) or a.entry&3 or not 0<=a.entry<=0xffffffff:p.error('private input requires valid --function and aligned --entry')
elif a.function or a.entry is not None:p.error('--function/--entry require --private-elf')
out=a.output.resolve()
if ROOT==out or ROOT in out.parents:p.error('keep generated output outside the source repository')
sha=subprocess.check_output(['git','-C',str(a.upstream),'rev-parse','HEAD'],text=True).strip()
if sha!=PIN:p.error('upstream commit differs from pinned reviewed source: '+sha)
subprocess.run(['git','-C',str(a.upstream),'diff','--exit-code','HEAD','--'],check=True)
recompiler=a.recompiler.resolve();out.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('synthetic',HERE/'synthetic_elf.py');fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
elf=a.private_elf.resolve() if a.private_elf else out/'probe.elf'
if not a.private_elf:elf.write_bytes(fixture.build())
entry=a.entry if a.private_elf else fixture.ENTRY
function=a.function if a.private_elf else 'probe_0x200000'
gen=out/'generated'
config=out/'config.toml';config.write_text('[general]\ninput = '+json.dumps(str(elf))+'\noutput = '+json.dumps(str(gen))+'''\nsingle_file_output = false
low_memory_mode = true
output_worker_threads = 1
patch_syscalls = false
patch_cop0 = false
patch_cache = false
stubs = []
skip = []
''')
r=subprocess.run([str(recompiler),str(config)],capture_output=True,text=True,check=True);(out/'generation.txt').write_text(r.stdout+r.stderr)
source=gen/(function+'.cpp');assert source.is_file()
assert 'ps2_stubs::' not in source.read_text(),'selected function is a stub, not translated code'
flags=['-std=c++20','-O2','-ffp-contract=off','-DGEKKO2_PS2RECOMP_LEAF_PROBE','-DRECOMP_LEAF_FUNCTION='+function,'-I'+str(HERE/'leaf_adapter'),'-I'+str(gen)]
subprocess.run(['g++',*flags,str(source),str(HERE/'leaf_entry.cpp'),str(HERE/'leaf_host_main.cpp'),'-o',str(out/'probe-host')],check=True)
cc=a.devkitppc/'bin/powerpc-eabi-g++';nm=a.devkitppc/'bin/powerpc-eabi-nm';target=out/'probe-ppc.elf'
subprocess.run([str(cc),*flags,'-mcpu=750','-meabi','-mhard-float','-G0','-fno-exceptions','-fno-rtti','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-ffunction-sections','-fdata-sections','-nostdlib',str(source),str(HERE/'leaf_entry.cpp'),'-Wl,--gc-sections','-Wl,-Ttext=0x80004000','-Wl,-e,recomp_leaf','-Wl,--undefined=recomp_leaf_layout','-o',str(target)],check=True)
oracle=subprocess.check_output(['python3','tools/build_host_core.py','ee_recomp_oracle'],cwd=ROOT,text=True).strip()
from unicorn import Uc,UC_ARCH_PPC,UC_MODE_PPC32,UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import UC_PPC_REG_0,UC_PPC_REG_1,UC_PPC_REG_3,UC_PPC_REG_LR,UC_PPC_REG_PC
binary=target.read_bytes();assert binary[:6]==b'\x7fELF\x01\x02'
phoff=struct.unpack_from('>I',binary,28)[0];phsize,phcount=struct.unpack_from('>HH',binary,42)
segments=[]
for i in range(phcount):
 kind,off,addr,_,size,mem,_,_=struct.unpack_from('>8I',binary,phoff+i*phsize)
 if kind==1:segments.append((addr,binary[off:off+size],mem))
syms={row.split()[-1]:int(row.split()[0],16) for row in subprocess.check_output([str(nm),str(target)],text=True).splitlines() if len(row.split())>=3}
u=Uc(UC_ARCH_PPC,UC_MODE_PPC32|UC_MODE_BIG_ENDIAN);u.mem_map(0x80000000,0x200000);u.mem_map(0x90000000,0x10000)
for addr,data,mem in segments:
 assert 0x80000000<=addr and addr+mem<=0x80200000;u.mem_write(addr,data)
size,regs,pc,delay,branch,hi,lo,sa=struct.unpack('>8I',u.mem_read(syms['recomp_leaf_layout'],32))
assert size<=0x10000 and regs==0
rng=random.Random(0x5900);cases=[(0,0),(1,0),(0x7fffffff,2),(0x80000000,0xffffffff),(0xffffffffffffffff,1),(0x1234567887654321,0xfedcba9876543210)]+[(v,0) for v in [0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000000,0x0010000000000000,0x000fffffffffffff,0x3ff0000000000000]]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(64)]
for case,(first,second) in enumerate(cases):
 upper=(0x5aa5123400000000|case)
 original=json.loads(subprocess.check_output([oracle,str(elf),hex(entry),'32',hex(first),hex(second),hex(upper)],text=True,timeout=20))
 host=json.loads(subprocess.check_output([str(out/'probe-host'),hex(first),hex(second),hex(upper)],text=True,timeout=20))
 context=bytearray(size)
 for n in range(1,32):struct.pack_into('>Q',context,regs+n*16+8,upper^n)
 for n,value in [(4,first),(5,second),(29,0x01fff000),(31,0x01fffff0)]:struct.pack_into('>Q',context,regs+n*16,value)
 u.mem_write(0x90000000,bytes(context));stack=0x801fe000
 for n in range(14,32):u.reg_write(UC_PPC_REG_0+n,0xa5000000+n)
 u.reg_write(UC_PPC_REG_1,stack);u.reg_write(UC_PPC_REG_3,0x90000000);u.reg_write(UC_PPC_REG_LR,0x80001000)
 u.emu_start(syms['recomp_leaf'],0x80001000,count=100000)
 assert u.reg_read(UC_PPC_REG_PC)==0x80001000 and u.reg_read(UC_PPC_REG_1)==stack
 assert all(u.reg_read(UC_PPC_REG_0+n)==0xa5000000+n for n in range(14,32))
 data=bytes(u.mem_read(0x90000000,size));assert not data[delay]
 ppc={'pc':'%08x'%struct.unpack_from('>I',data,pc)[0],'gpr':[['%016x'%v for v in struct.unpack_from('>QQ',data,regs+n*16)] for n in range(32)],'hi':['%016x'%struct.unpack_from('>Q',data,hi)[0],'0000000000000000'],'lo':['%016x'%struct.unpack_from('>Q',data,lo)[0],'0000000000000000'],'sa':struct.unpack_from('>I',data,sa)[0]}
 for field in ('pc','gpr','hi','lo','sa'):
  assert original[field]==host[field]==ppc[field],(field,hex(first),hex(second),original[field],host[field],ppc[field])
 if not a.private_elf:assert int(original['gpr'][2][0],16)==fixture.expected(first,second)
report=dict(kind=('private' if a.private_elf else 'synthetic')+'-PS2Recomp-integer-leaf-probe',upstream_commit=sha,recompiler_sha256=hashlib.sha256(recompiler.read_bytes()).hexdigest(),generated_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),ppc_elf_sha256=hashlib.sha256(binary).hexdigest(),cases=len(cases),compared=['all GPR128 storage; nonzero upper lanes preserved (no MMI arithmetic)','HI/LO','SA','return PC','delay-slot result','PPC callee-save and SP'],host_and_ppc_match_gekko2=True,adapter_scope='test-only scalar integer leaf; no devices, memory helpers, VU or MMI',physical_wii_tested=False,dolphin_tested=False,owner_input=bool(a.private_elf),game_recompiled=False,fps_claim=False)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
