#!/usr/bin/env python3
from pathlib import Path
import importlib.util
root=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('profile',root/'tools/analyze_runtime_profile.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def row(scope,category,ticks,entries=1,work=0,unit='none'):
 return f'PROFILE_COST schema=1 scope={scope} category={category} ticks={ticks} entries={entries} work={work} unit={unit} tb_hz=1000\n'
text='PRIVATE_GUEST_STATE must_not_appear\n'+row('sampled_cpu','EE',30)+row('sampled_cpu','GS',10)+row('sampled_cpu','EE',30)+row('sampled_cpu','GS',10)+row('exact_present','GX_wait',900)+row('exact_present','present',100)
r=m.analyze(text);assert r['rows']==6 and r['scopes']['sampled_cpu']['ticks']==80
assert r['scopes']['sampled_cpu']['categories'][0]['share_percent']==75
assert r['scopes']['exact_present']['categories'][0]['share_percent']==90
assert 'PRIVATE_GUEST_STATE' not in str(r)
checks=5
for bad in ('',row('sampled_cpu','EE',1).replace('ticks=1','ticks=-1'),row('sampled_cpu','EE',1).replace('tb_hz=1000','tb_hz=0'),row('sampled_cpu','EE',1).replace('schema=1','schema=2'),row('sampled_cpu','EE',1)+row('exact_present','EE',2).replace('tb_hz=1000','tb_hz=2000'),row('sampled_cpu','EE',1)+row('sampled_cpu','EE',2,unit='bytes'),row('sampled_cpu','EE',1).replace('ticks=1','ticks=1 ticks=2')):
 try:m.analyze(bad)
 except ValueError:checks+=1
 else:raise AssertionError('bad input accepted')
print(f'PASS {checks} runtime profile aggregation, separate denominators, private-data exclusion and malformed-record checks')
