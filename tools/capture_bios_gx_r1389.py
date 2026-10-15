#!/usr/bin/env python3
"""Capture private GS packets from the user's BIOS, with native eligibility enabled.
The host capture returns to software; this tool does not simulate GPU pixels.
The output directory is a generated workspace; existing case-*.bin are replaced.
"""
from pathlib import Path
import subprocess,hashlib,os,argparse
parser=argparse.ArgumentParser();parser.add_argument('--bios',required=True);parser.add_argument('--output',required=True);parser.add_argument('--slices',type=int,default=30);args=parser.parse_args()
r=Path(__file__).resolve().parents[1];out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True)
for old in out.glob('case-*.bin'):old.unlink()

s=(r/'source/hw/gif.c').read_text();a=s.index('#ifdef GEKKO2_GX_NATIVE_BIOS_TRI\n    uint32_t native_z_min');b=s.index('\n#endif',a)
block=s[a:b].replace('gekko2_opt_enabled(GEKKO2_OPT_GX_GOURAUD)','1').replace('gekko2_opt_enabled(GEKKO2_OPT_GX_RESIDENT)','1').replace('gs_gx_render_active()','1');s=s[:a]+block+s[b:]
s='#define gs_gx_draw_native_stq_triangle capture_native\n'+s
s+='''
#include <stdio.h>
#include <stdlib.h>
int capture_native(uint32_t psm,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
 const int32_t *xy,const float *uv,const float *q,const uint32_t *rgba,uint32_t tw,uint32_t th,
 uint32_t ws,uint32_t wt,uint32_t linear,uint32_t scanmsk,gs_gx_texel_fn sample,
 const gs_gx_pipeline *p,uint32_t conservative_z,const gs_gx_native_color *color)
{
 static unsigned count=0;if(count>=6)return 0;
 if(count>=4&&(uint64_t)(maxx-minx+1)*(uint32_t)(maxy-miny+1)<4096u)return 0;
 uint32_t pixels[16384],amin=255,amax=0;
 for(uint32_t y=0;y<th;y++)for(uint32_t x=0;x<tw;x++){
  uint32_t v=sample(x,y);pixels[y*tw+x]=v;
  if((v>>24)<amin)amin=v>>24;if((v>>24)>amax)amax=v>>24;
 }
 if(amin==amax)return 0;
 char name[512];snprintf(name,sizeof name,"%s/case-%u.bin",getenv("GX_CASE_DIR"),count++);
 FILE *f=fopen(name,"wb");if(!f)return 0;
 uint32_t header[16]={psm,bp,bw,(uint32_t)minx,(uint32_t)miny,(uint32_t)maxx,(uint32_t)maxy,tw,th,ws,wt,linear,scanmsk,conservative_z,rgba!=NULL,q!=NULL};
 fwrite(header,sizeof header,1,f);fwrite(xy,24,1,f);fwrite(uv,24,1,f);
 float qs[3]={1,1,1};fwrite(q?q:qs,12,1,f);uint32_t cs[3]={0,0,0};fwrite(rgba?rgba:cs,12,1,f);
 fwrite(p,sizeof(*p),1,f);fwrite(color,sizeof(*color),1,f);fwrite(pixels,tw*th*4,1,f);
 fwrite(gs_mem_read_range(0,GS_MEM_SIZE),GS_MEM_SIZE,1,f);fclose(f);
 fprintf(stderr,"CAPTURE_GX case=%u tex=%ux%u alpha=%u..%u blend=%u/%u/%u/%u fix=%u Z=%u bound=%u\\n",count,tw,th,amin,amax,p->a,p->b,p->c,p->d,p->fix,p->z,conservative_z);
 return 0;
}
'''
(out/'gif-capture.c').write_text(s)
subprocess.run(['python','tools/build_host_core.py','game_check_r1386'],cwd=r,check=True,stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT)
flags=['-fsanitize=address','-g','-O1','-w','-ffunction-sections','-fdata-sections','-ffp-contract=off','-I'+str(r/'include'),'-I'+str(r/'source')]
subprocess.run(['gcc',*flags,'-DGEKKO2_GX_NATIVE_BIOS_TRI','-c',str(out/'gif-capture.c'),'-o',str(out/'gif-capture.o')],check=True)
old=hashlib.sha256(str(r/'source/hw/gif.c').encode()).hexdigest()[:16]+'.o'
objects=[str(x) for x in (r/'validation/r1386-host').glob('*.o') if x.name!=old]+[str(out/'gif-capture.o')]
exe=out/'bios-capture';subprocess.run(['gcc',*flags,str(r/'tools/bios_visual_driver_r1387.c'),*objects,'-Wl,--gc-sections','-Wl,--wrap=time','-o',str(exe),'-lm'],check=True)
env=dict(os.environ,GEKKO2_OPTIONS='7ffd7fff',GX_CASE_DIR=str(out),ASAN_OPTIONS='detect_leaks=0')
for n in ['GEKKO2_RESUME','GEKKO2_INPUT','GEKKO2_PAD_PLAN','GEKKO2_CAPTURE']:env.pop(n,None)
with (out/'capture.log').open('w') as f:subprocess.run([str(exe),str(Path(args.bios).resolve()),'-',str(args.slices)],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
print([x.name for x in out.glob('case-*.bin')])
