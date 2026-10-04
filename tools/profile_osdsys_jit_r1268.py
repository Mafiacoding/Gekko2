"""Record dynamic COP2/MMI compilation candidates during real PAD-only BIOS navigation.
Usage: profile_osdsys_jit_r1268.py BIOS checkpoint-in checkpoint-out config-file [hex-buttons million-EE-budget]...
Run verify_regressions.py first to build native objects. This executes the native
interpreter with a read-only instruction observer, not the PPC JIT or a FPS test.
"""
from pathlib import Path
import tempfile,subprocess,sys
root=Path(__file__).resolve().parents[1]
observer='#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include "core/recompiler/ppc_dynarec.h"\ntypedef struct {uint32_t iw,pc;uint64_t visits;int valid,rejected;} item;\nstatic item entries[4096];static uint64_t all,missed;static unsigned overflow;\nvoid r1267_profile(uint32_t pc,uint32_t iw){all++;unsigned h=(iw*2654435761u)&4095;for(unsigned i=0;i<4096;i++){item*p=&entries[(h+i)&4095];if(!p->valid){ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,1))abort();p->iw=iw;p->pc=pc;p->rejected=ppc_dynarec_translate_one(&c,iw)!=0;ppc_dynarec_free(&c);p->valid=1;}if(p->iw==iw){p->visits++;if(p->rejected)missed++;return;}}overflow++;}\nstatic void dump(void){fprintf(stderr,"CANDIDATE_PROFILE family_visits=%llu unsupported_visits=%llu overflow=%u\\n",(unsigned long long)all,(unsigned long long)missed,overflow);for(unsigned i=0;i<4096;i++)if(entries[i].valid&&entries[i].rejected)fprintf(stderr,"UNSUPPORTED iw=%08x first_pc=%08x visits=%llu\\n",entries[i].iw,entries[i].pc,(unsigned long long)entries[i].visits);}\n__attribute__((constructor)) static void setup(void){atexit(dump);}\n'
with tempfile.TemporaryDirectory() as folder:
 d=Path(folder);ee=d/'ee_profile.c';obj=d/'ee_profile.o';trace=d/'trace.c';driver=d/'driver.c';exe=d/'profile'
 source=(root/'source/core/ee/ee_core.c').read_text().replace('uint32_t instr = ee_fetch32(st, pc);','uint32_t instr = ee_fetch32(st, pc);\n    if ((instr >> 26)==0x12u || (instr >> 26)==0x1cu) r1267_profile(pc,instr);')
 ee.write_text('#include <stdint.h>\nvoid r1267_profile(uint32_t,uint32_t);\n'+source);trace.write_text(observer)
 driver.write_text((root/'tools/verify_osdsys_navigation_r1266.c').read_text().replace('work/osd-r1266-browser-input-','work/osd-r1268-profile-input-'))
 flags=['gcc','-O2','-w','-I'+str(root/'include'),'-I'+str(root/'source')]
 subprocess.run(flags+['-c',str(ee),'-o',str(obj)],check=True)
 objects=[str(p) for p in (root/'outputs/verification').glob('source*.o') if p.name!='source_core_ee_ee_core.c.o']
 subprocess.run(flags+[str(driver),str(trace),str(obj),str(root/'source/core/recompiler/ppc_dynarec.c'),*objects,'-lm','-o',str(exe)],check=True)
 raise SystemExit(subprocess.run([str(exe),*sys.argv[1:]]).returncode)
