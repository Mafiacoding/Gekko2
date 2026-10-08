"""Current linked-PPC mapped-memory suite without historical heap-only benchmarks.
Reuses the independent architectural cases, source mutation and IRQ oracles.
Runs on both ordinary and fastmem-enabled builds; no hardware speed claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
ops=[0x20,0x24,0x21,0x25,0x23,0x28,0x29,0x2b]
body=Path(__file__).with_name('verify_ee_mapped_blocks_r1305.py').read_text()
exec(compile(body[body.index('mapped_digest='):],'mapped_oracles_r1331','exec'))
