#!/usr/bin/env python3
"""Compare actual cold-BIOS frames at identical bounded guest work and RTC.
Outputs are private guest-derived evidence, never part of the source archive.
"""
import argparse,subprocess,hashlib,os,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--bios',required=True);p.add_argument('--before-source',required=True)
p.add_argument('--output',required=True);p.add_argument('--slices',type=int,default=65);args=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(root/'tools/build_host_core.py'),'game_check_r1386'],cwd=root,check=True)
cache=root/'validation/r1386-host';gif=root/'source/hw/gif.c'
gif_obj=hashlib.sha256(str(gif).encode()).hexdigest()[:16]+'.o'
before=out/'before-gif.o'
flags=['-O3','-w','-ffunction-sections','-fdata-sections','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'source')]
subprocess.run(['gcc',*flags,'-c',str(Path(args.before_source).resolve()),'-o',str(before)],check=True)
summary={}
for label in ['before','after']:
 folder=out/label;folder.mkdir(exist_ok=True);exe=out/('bios-'+label)
 objects=[str(x) for x in cache.glob('*.o') if label=='after' or x.name!=gif_obj]
 if label=='before':objects.append(str(before))
 subprocess.run(['gcc',*flags,str(root/'tools/bios_visual_driver_r1387.c'),*objects,'-Wl,--gc-sections','-Wl,--wrap=time','-o',str(exe),'-lm'],check=True)
 env=dict(os.environ,GEKKO2_OPTIONS='7ffd7fff',GEKKO2_CAPTURE=str(folder))
 for name in ['GEKKO2_RESUME','GEKKO2_INPUT','GEKKO2_PAD_PLAN']:env.pop(name,None)
 with (folder/'run.log').open('w') as log:
  subprocess.run([str(exe),str(Path(args.bios).resolve()),'-',str(args.slices)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 summary[label]={x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in folder.glob('frame-*.ppm')}
common=sorted(set(summary['before'])&set(summary['after']))
result={'common_frames':len(common),'changed_frames':[x for x in common if summary['before'][x]!=summary['after'][x]],'scope':'Actual BIOS cold boot, fixed RTC, same guest slice budget; no injected geometry or logo','physical_wii_verified':False}
(out/'comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
