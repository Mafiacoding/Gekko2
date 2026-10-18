#!/usr/bin/env python3
"""Summarize exclusive runtime PROFILE_COST rows without exposing raw guest logs.
Sampled CPU and exact presentation have separate denominators. Work counters
use category-specific units and must not be added across categories.
"""
import argparse,json,re
from pathlib import Path
FIELDS={'schema','scope','category','ticks','entries','work','unit','tb_hz'}
def analyze(text):
    groups={};rows=0;frequencies=set();configurations=set()
    for line in text.splitlines():
        # Whitelist numeric configuration fields only; never copy guest records.
        if line.startswith('EVENT '):
            masks=re.findall(r'(?:^| )active_mask=([0-9a-fA-F]{8})(?= |$)',line)
            if len(masks)==1:configurations.add(int(masks[0],16))
        if not line.startswith('PROFILE_COST '):continue
        parts=line.split()[1:]
        if any('=' not in x for x in parts):raise ValueError('malformed PROFILE_COST')
        pairs=[x.split('=',1) for x in parts];row=dict(pairs)
        if len(pairs)!=len(row) or set(row)!=FIELDS or row['schema']!='1':raise ValueError('unsupported or duplicate profile fields')
        if row['scope'] not in ('sampled_cpu','exact_present'):raise ValueError('unknown timing scope')
        if not re.fullmatch(r'[A-Za-z0-9_]+',row['category']) or not re.fullmatch(r'[A-Za-z0-9_]+',row['unit']):raise ValueError('invalid category/unit')
        for key in ('ticks','entries','work','tb_hz'):
            row[key]=int(row[key])
            if row[key]<0 or (key=='tb_hz' and not row[key]):raise ValueError('invalid '+key)
        frequencies.add(row['tb_hz']);rows+=1
        key=(row['scope'],row['category']);g=groups.setdefault(key,dict(unit=row['unit'],ticks=0,entries=0,work=0,interval_rows=0))
        if g['unit']!=row['unit']:raise ValueError('work unit changed')
        for name in ('ticks','entries','work'):g[name]+=row[name]
        g['interval_rows']+=1
    if not rows:raise ValueError('no PROFILE_COST records; use the profiling build')
    if len(frequencies)!=1:raise ValueError('mixed timer frequencies')
    hz=frequencies.pop();out={'schema':1,'rows':rows,'tb_hz':hz,'scopes':{},'hardware_execution_confirmed':False,
      'notes':['CPU values are sampled; presentation values cover measured host calls.',
               'Percentages are within each scope; do not combine their denominators.',
               'EE native time includes declined attempts; entries are scope entries, not instruction counts.',
               'EE_step_retirements includes native work; work counters are not additive.',
               'Profiler overhead is included in instrumented timing; no FPS gain is inferred.']}
    out['cpu_configurations']=[dict(active_mask=f'{mask:08x}',
        ee_jit=bool(mask&1),iop_jit=bool(mask&2),vu_jit=bool(mask&4))
        for mask in sorted(configurations)]
    warnings=[]
    if not configurations:warnings.append('No active CPU mask found; native execution cannot be inferred from attempt entries.')
    if any(not (mask&7) for mask in configurations):
        warnings.append('EE, IOP and VU JIT are all disabled in at least one active configuration; this is an interpreter run.')
    if len(configurations)>1:
        warnings.append('Multiple active CPU masks: timing totals combine configurations and are not a controlled comparison.')
    out['warnings']=warnings
    for scope in ('sampled_cpu','exact_present'):
        selected=[(c,g) for (s,c),g in groups.items() if s==scope]
        if not selected:continue
        total=sum(g['ticks'] for _,g in selected);cats=[]
        for category,g in selected:
            cats.append(dict(category=category,**g,ms=g['ticks']/hz*1000,share_percent=(100*g['ticks']/total if total else 0)))
        out['scopes'][scope]=dict(ticks=total,ms=total/hz*1000,categories=sorted(cats,key=lambda x:x['ticks'],reverse=True))
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('--output-json',type=Path);a=p.parse_args()
    try:out=analyze(a.log.read_text(errors='replace'));content=json.dumps(out,indent=2,allow_nan=False)+'\n'
    except (ValueError,OSError) as e:p.exit(1,str(e)+'\n')
    if a.output_json:a.output_json.write_text(content)
    print(content,end='')
