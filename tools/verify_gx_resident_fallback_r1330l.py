"""Run the existing compact differential oracle with residency enabled.
Fragmented depth masks must reject residency and preserve the compact result.
GPU/cache services remain synthetic; no hardware rendering claim.
"""
from pathlib import Path
template=Path(__file__).with_name('verify_gx_texture_r1299.py').read_text()
template=template.split('# Actual linked shader programs,')[0]
disabled="u.mem_write(syms['surface_enabled'],struct.pack('>I',0)) # test compact TEV/depth capture independently of resident submission"
assert disabled in template
template=template.replace(disabled,'# Residency deliberately enabled for these fallback checks.')
lines=template.splitlines()
prefix='for psm,ztst,zpsm,frag,coeff,linear,pabe in '
for i,line in enumerate(lines):
 if line.startswith(prefix):
  assert line.endswith(']:')
  cases=line[len(prefix):-1]
  lines[i]=prefix+'[case for case in '+cases+' if case[1] in (2,3) and case[3]==128]:'
  break
else:raise AssertionError('compact differential case list missing')
exec(compile('\n'.join(lines),'compact_residency_fallback','exec'))
call('gs_gx_resident_pipeline_count',7)
rejects=(u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_3+1)
assert rejects>=10,('expected fragmented depth rejections',rejects)
print(f'PASS resident-to-compact fallback: {rejects} fragmented depth rejections, exact CT32/CT24 RGB, alpha and Z results; synthetic GX only')
