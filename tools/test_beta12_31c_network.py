#!/usr/bin/env python3
"""Targeted reproducible network menu/collector and existing profile regression."""
import argparse,json,subprocess,sys,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reports',type=Path,required=True);a=p.parse_args();a.reports.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1]
def run(command,name):
 out=subprocess.run(command,cwd=r,text=True,capture_output=True)
 (a.reports/name).write_text(out.stdout+out.stderr)
 if out.returncode:raise RuntimeError(name+' failed: '+out.stdout+out.stderr)
run([sys.executable,'tools/test_beta12_31a_status.py'],'host-status.json')
run([sys.executable,'tests/test_network_receiver.py'],'host-network.txt')
with tempfile.TemporaryDirectory() as temp:
 exe=str(Path(temp)/'menu')
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Iinclude','tests/test_network_menu.c','source/hw/runtime_telemetry.c','source/core/recompiler/optimization.c','-o',exe],cwd=r,check=True)
 run([exe],'host-menu.txt')
exe=subprocess.check_output([sys.executable,'tools/build_host_core.py','test_runtime_profile_work'],cwd=r,text=True).strip()
run([exe],'host-work.txt')
assert len(json.loads((a.reports/'host-status.json').read_text()))==4
print('PASS four existing CPU/menu/config suites, network protocol/collector, network menu and real EE/VU/GX profile work')
