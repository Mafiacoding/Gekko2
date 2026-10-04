"""Focused warm PPC instruction counts; no hardware FPS/cycle inference."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
ops=[0x22,0x26,0x2a,0x2e,0x1a,0x1b,0x2c,0x2d]
bench='bench={}'+Path(__file__).with_name('verify_ee_merge_internal.py').read_text().split('bench={}')[1]
exec(compile(bench,'merge_benchmark','exec'))
