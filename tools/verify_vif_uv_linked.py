#!/usr/bin/env python3
"""Current VIF/UV PPC gates without the retired automatic 4:3 postprocess.
The BETA12.23 helper test applies only to the historical BETA12.23 image.
BETA12.24a and later intentionally stopped using it after user feedback.
"""
from pathlib import Path
source=Path(__file__).with_name('verify_beta12_23_linked.py').read_text()
exec(compile(source.split('for wide in (0,1):')[0],'verify_beta12_23_linked.py','exec'))
assert 'gs_xfb_fit_4_3' not in syms,'current image unexpectedly retains historical 4:3 postprocessor'
print('PASS current-image policy: historical automatic 4:3 postprocessor is not linked; no pillarbox assertion applied')
