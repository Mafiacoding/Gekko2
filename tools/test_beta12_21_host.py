#!/usr/bin/env python3
"""Build and execute the BETA12.21 host gates. No private assets required."""
import argparse,json,subprocess,sys,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path);a=p.parse_args()
names=['test_gs_dither_admission','test_gs_blend_formats','test_vif_direct_streaming',
 'test_chain_residency_cache','test_gs_psm_convert_beta12_11','test_beta12_20_gx_alpha',
 'test_gx_pipeline_r1298','test_gs_alias_sync_beta12_13','test_scanout_grid_hash',
 'test_scanout_readback_failure','test_dma_ipu_stall','test_ipu_idec_fields',
 'test_hardware_accuracy_r1384','test_gif_signal_backpressure','test_vif_transport_r1379',
 'test_vif_split_irq_beta11','test_dma_vif_tag_transfer','test_xgkick_timing_aot3',
 'test_xgkick_boundary_r1379','test_gs_dither','test_gs_colclamp','test_beta12_19_framebuffer16']
built=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=r,text=True)
executables=[Path(line) for line in built.splitlines() if Path(line).is_file()]
assert len(executables)==len(names),built
results=[]
def run(name,path):
    out=subprocess.run([str(path)],cwd=r,capture_output=True,text=True)
    results.append(dict(test=name,exit_code=out.returncode,output=out.stdout+out.stderr))
    print(('PASS ' if out.returncode==0 else 'FAIL ')+name,flush=True)
    if out.returncode:print(out.stdout+out.stderr,flush=True)
for name,exe in zip(names,executables):run(name,exe)
with tempfile.TemporaryDirectory(prefix='gekko2-beta12-21-') as directory:
    d=Path(directory)
    for mode,flags in [('control',[]),('chain',['-DGEKKO2_EE_CHAIN_RESIDENCY']),
                       ('interpreter',['-DGEKKO2_EE_CHAIN_RESIDENCY','-DPCSX2WII_JIT_DISABLE'])]:
        exe=d/('options-'+mode)
        subprocess.run(['gcc','-O2','-Iinclude',*flags,'tests/test_experimental_options.c',
                        'source/core/recompiler/experimental_options.c',
                        'source/core/recompiler/optimization.c','-o',str(exe)],cwd=r,check=True)
        run('options-'+mode,exe)
    exe=d/'chain-map'
    subprocess.run(['gcc','-O2','-Iinclude','-Isource','tests/test_chain_map_selection.c',
                    'source/core/recompiler/ppc_dynarec.c',
                    'source/core/recompiler/ppc_code_cache.c',
                    'source/core/recompiler/optimization.c','-lm','-o',str(exe)],cwd=r,check=True)
    run('chain-map-RAM-control',exe)
    exe=d/'psm-modes'
    subprocess.run(['gcc','-O2','-DGEKKO2_EE_CHAIN_RESIDENCY','-ffunction-sections',
                    '-fdata-sections','-Iinclude','-Isource','tests/test_beta12_21_psm_modes.c',
                    'source/core/recompiler/experimental_options.c','-Wl,--gc-sections',
                    '-lm','-o',str(exe)],cwd=r,check=True)
    run('PSM-four-modes',exe)
if a.output_json:
    a.output_json.parent.mkdir(parents=True,exist_ok=True)
    a.output_json.write_text(json.dumps(results,indent=2)+'\n')
assert all(x['exit_code']==0 for x in results),'host regression failure'
print(f'PASS {len(results)} BETA12.21 host suites; host checks do not establish Wii gameplay')
