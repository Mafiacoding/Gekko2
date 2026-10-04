"""Run the COP1/link oracle independently of historical loader gates.
Both Interpreter and JIT ELFs reach all cases, even when an older nested
regression script deliberately exits after its interpreter-only checks.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
body=Path(__file__).with_name('verify_ee_fpu_links_internal.py').read_text().split('fpu_sig=',1)[1]
exec(compile('fpu_sig='+body,'verify_ee_fpu_links_internal.py','exec'))
