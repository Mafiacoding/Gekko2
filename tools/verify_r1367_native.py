"""Rebuild and run R1367 focused native regressions; no BIOS/disc required."""
import concurrent.futures, json, os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
cc=os.environ.get('CC','cc')
tests={
 'test_gx_pipeline_r1298':['source/hw/gif.c'], 'test_gx_surface_r1300':[], 'test_gx_scanout_black_r1356':[],
 'test_pad_config_r1365':[],
 'test_controller_r1364':[], 'test_controller_guest_r1364':[],
 'test_sound_options_r1363':[], 'test_sound_beta_r1363':[], 'test_checkpoint_sound_r1363':[],
 'test_spu2_mixer':[], 'test_spu2_regs':['source/hw/iop_spu2.c'], 'test_iop_spu2':[], 'test_iop_dma_spu2':[],
 'test_ipu_idec_r1337':[], 'test_ipu_bdec_r1337':[], 'test_rspu2_stream_r1340':[],
 'test_checkpoint_rspu2_r1340':[],
 'test_code_cache_classes_r1362':[],
 'test_vu0_reset_r1361':[], 'test_vu_ior_r1357':[], 'test_vu_ior_codegen_r1357':[],
 'test_vu_micro':[], 'test_vu_jit_pair_r1269':[],
 'test_pad_new_dma_r1357':['source/core/ee/ee_core.c'],
 'test_pad_legacy_rpc_r1356':['source/core/ee/ee_core.c'],
 'test_cdrom_checkpoint_r1357':['source/hw/iop_asyncio.c','source/hw/iop_cdrom_legacy.c','source/hw/iop_intc.c','source/hw/iop_hle_events.c'],
 'test_dma_tospr_tte_r1357':['source/hw/dma.c'],
 'test_dma_mfifo_producer':['source/hw/dma.c'],
 'test_dma_chain':['source/hw/dma.c'],
 'test_dma_scratch_transfer':['source/hw/dma.c'],
 'test_dma_vif_tag_transfer':[],
 'test_iop_lightrec_pipeline_r1358':[],
 'test_code_cache_admission_r1358':['source/core/recompiler/optimization.c'],
 'test_ppc_code_cache_r1331':['source/core/recompiler/optimization.c'],
 'test_code_cache_random_r1337':[],
 'test_iso_cache_r1358':['source/core/iso_loader.c'],
 'test_iso_loader':['source/core/iso_loader.c'],
 'test_iso_nested_path':['source/core/iso_loader.c']}
v=root/'third_party'
flags=['-DGEKKO2_IOP_LIGHTREC_PILOT','-DHAVE_MMAP=1','-DLOG_LEVEL=0',*( '-I'+str(p) for p in [v/'lightrec',v/'lightrec/tlsf',v/'lightning/include']),'-O2','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'source')]
if True:
 tmp=str(root.parent/'r1367-private/native-objects');pathlib.Path(tmp).mkdir(parents=True,exist_ok=True)
 out=pathlib.Path(tmp)
 sources=sorted(p for p in (root/'source').rglob('*.c') if p.name not in ('main.c','ppc_dynarec.c'))
 sources += [*(v/'lightrec'/n for n in ['blockcache.c','constprop.c','emitter.c','interpreter.c','lightrec.c','memmanager.c','optimizer.c','regcache.c']),v/'lightrec/tlsf/tlsf.c',*(v/'lightning/lib'/n for n in ['jit_disasm.c','jit_memory.c','jit_names.c','jit_note.c','jit_print.c','jit_size.c','lightning.c'])]
 def compile_one(p):
  o=out/(str(p.relative_to(root)).replace('/','_')+'.o')
  subprocess.run([cc,*flags,'-c',str(p),'-o',str(o)],check=True)
  return str(p.relative_to(root)),str(o)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=dict(pool.map(compile_one,sources))
 results=[]
 for name,skip in tests.items():
  exe=out/name
  selected=[objects['source/hw/rspu2_stream.c']] if name=='test_rspu2_stream_r1340' else [o for p,o in objects.items() if p not in skip]
  subprocess.run([cc,*flags,str(root/'tests'/(name+'.c')),*selected,'-lm','-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  print(result.stdout,end='');results.append({'test':name,'exit_code':result.returncode})
  if result.returncode:raise SystemExit(result.returncode)
 print(json.dumps({'native_regressions':results,'all_passed':True,'wii_runtime_test':False},indent=2))
