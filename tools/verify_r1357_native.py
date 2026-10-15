"""Rebuild and run R1357 focused native regressions; no BIOS/disc required."""
import concurrent.futures, json, os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
cc=os.environ.get('CC','cc')
tests={
 'test_vu_ior_r1357':[], 'test_vu_ior_codegen_r1357':[],
 'test_vu_micro':[], 'test_vu_jit_pair_r1269':[],
 'test_pad_new_dma_r1357':['source/core/ee/ee_core.c'],
 'test_pad_legacy_rpc_r1356':['source/core/ee/ee_core.c'],
 'test_cdrom_checkpoint_r1357':['source/hw/iop_asyncio.c','source/hw/iop_cdrom_legacy.c','source/hw/iop_intc.c','source/hw/iop_hle_events.c'],
 'test_dma_tospr_tte_r1357':['source/hw/dma.c'],
 'test_dma_mfifo_producer':['source/hw/dma.c'],
 'test_dma_chain':['source/hw/dma.c'],
 'test_dma_scratch_transfer':['source/hw/dma.c'],
 'test_dma_vif_tag_transfer':[]}
flags=['-O2','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'source')]
with tempfile.TemporaryDirectory(prefix='gekko2-r1357-tests-') as tmp:
 out=pathlib.Path(tmp)
 sources=sorted(p for p in (root/'source').rglob('*.c') if p.name not in ('main.c','ppc_dynarec.c'))
 def compile_one(p):
  o=out/(str(p.relative_to(root)).replace('/','_')+'.o')
  subprocess.run([cc,*flags,'-c',str(p),'-o',str(o)],check=True)
  return str(p.relative_to(root)),str(o)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=dict(pool.map(compile_one,sources))
 results=[]
 for name,skip in tests.items():
  exe=out/name
  subprocess.run([cc,*flags,str(root/'tests'/(name+'.c')),*[o for p,o in objects.items() if p not in skip],'-lm','-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  print(result.stdout,end='');results.append({'test':name,'exit_code':result.returncode})
  if result.returncode:raise SystemExit(result.returncode)
 print(json.dumps({'native_regressions':results,'all_passed':True,'wii_runtime_test':False},indent=2))
