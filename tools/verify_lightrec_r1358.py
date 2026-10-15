"""Execute real Lightrec/GNU Lightning on the host; does not execute PPC code."""
import pathlib,subprocess,concurrent.futures
r=pathlib.Path(__file__).resolve().parents[1];out=r.parent/'r1359-private/lightrec-host';out.mkdir(parents=True,exist_ok=True)
v=r/'third_party'
files=[*(v/'lightrec'/n for n in ['blockcache.c','constprop.c','emitter.c','interpreter.c','lightrec.c','memmanager.c','optimizer.c','regcache.c']),v/'lightrec/tlsf/tlsf.c',*(v/'lightning/lib'/n for n in ['jit_disasm.c','jit_memory.c','jit_names.c','jit_note.c','jit_print.c','jit_size.c','lightning.c']),r/'source/core/recompiler/iop_lightrec.c']
flags=['-O2','-DGEKKO2_IOP_LIGHTREC_PILOT','-DHAVE_MMAP=1','-DLOG_LEVEL=0',*( '-I'+str(p) for p in [r/'include',v/'lightrec',v/'lightrec/tlsf',v/'lightning/include'])]
def build(p):
 o=out/(p.stem+'.o');subprocess.run(['cc',*flags,'-c',str(p),'-o',str(o)],check=True);return str(o)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(build,files))
exe=out/'test-lightrec';subprocess.run(['cc',*flags,str(r/'tests/test_lightrec_iop_r1358.c'),*objects,'-lm','-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
