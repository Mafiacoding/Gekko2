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

base=row('sampled_cpu','EE_native_attempt',1,entries=999,work=0)
a=m.analyze('EVENT session=1 active_mask=007075c0 path=PRIVATE_PATH\n'+base)
assert a['cpu_configurations']==[dict(active_mask='007075c0',ee_jit=False,iop_jit=False,vu_jit=False)]
assert any('interpreter run' in w for w in a['warnings']) and 'PRIVATE_PATH' not in str(a)
b=m.analyze('EVENT active_mask=00787dff\n'+base)
assert all(b['cpu_configurations'][0][k] for k in ('ee_jit','iop_jit','vu_jit')) and not b['warnings']
assert m.analyze(base)['warnings']  # zero retired work alone does not prove JIT disabled
mixed=m.analyze('EVENT active_mask=007075c0\nEVENT active_mask=00787dff\n'+base)
assert any('Multiple active' in w for w in mixed['warnings'])
assert not m.analyze('EVENT active_mask=00787dffjunk\n'+base)['cpu_configurations']
assert not m.analyze('EVENT active_mask=00787dff active_mask=007075c0\n'+base)['cpu_configurations']
print('PASS active CPU masks, interpreter warning, mixed-run warning, attempt semantics and configuration privacy')
