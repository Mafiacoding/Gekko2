#!/usr/bin/env python3
"""Independent ring/recurrence and CSV accounting oracles; no hardware timing."""
from pathlib import Path
import ctypes as C,importlib.util,random,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('limits',root/'tools/analyze_wii_limits.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
checks=0
with tempfile.TemporaryDirectory() as temp:
    lib=Path(temp)/'kernels.so';subprocess.run(['cc','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-shared','-fPIC',str(root/'apps/wii_limits/kernels.c'),'-o',str(lib)],check=True)
    k=C.CDLL(str(lib));k.limits_integer.argtypes=[C.c_uint32,C.c_uint32];k.limits_integer.restype=C.c_uint32
    k.limits_float.argtypes=[C.c_float,C.c_uint32];k.limits_float.restype=C.c_float
    k.limits_chase.argtypes=[C.POINTER(C.c_uint32),C.c_uint32,C.c_uint32];k.limits_chase.restype=C.c_uint32
    rng=random.Random(20261018)
    for n in [0,1,2,31,256,4096]+[rng.randrange(10000) for _ in range(24)]:
        seed=rng.getrandbits(32);expected=seed
        for _ in range(n):
            expected=(expected*1664525+1013904223)&0xffffffff;expected^=expected>>13
            expected=(((expected<<7)|(expected>>25))+0x9e3779b9)&0xffffffff
        assert k.limits_integer(seed,n)==expected;checks+=1
        f=C.c_float(0.75).value
        for _ in range(n):
            f=C.c_float(C.c_float(f*C.c_float(0.99991).value).value+C.c_float(0.00013).value).value
            f=C.c_float(C.c_float(f*C.c_float(0.99993).value).value+C.c_float(0.00011).value).value
        assert k.limits_float(0.75,n)==f;checks+=1
    for size in [8192,32768,262144,2097152]:
        a=(C.c_uint32*(size//4))();k.limits_fill(a,size);assert k.limits_verify(a,size)==1
        a[size//8]^=1;assert not k.limits_verify(a,size);checks+=2
        k.limits_ring(a,size);index=0;seen=set()
        for _ in range(size//32):
            assert index not in seen and index%8==0;seen.add(index);index=a[index]
        assert index==0 and len(seen)==size//32
        assert k.limits_chase(a,0,size//32)==0;assert k.limits_chase(a,0,size//32-1)!=0;checks+=3
header='# schema=1 tb_hz=1000000\ntest,sample,pool,size_bytes,iterations,work_units,unit,ticks,submit_ticks,wait_ticks,verified,status,checksum\n'
rows=[f'copy,{i},MEM1_cached,1048576,1,1048576,payload_bytes,10000,0,0,1,ok,00000000' for i in range(5)]
report=m.analyze(header+'\n'.join(rows)+'\n# end=finished\n');assert report['complete'];assert report['groups'][0]['median_payload_MiB_s']==100;checks+=2
for bad in [header+'\n'.join(rows+[rows[0]]),header+rows[0].replace('10000,0,0','10000,20000,0'),header.replace('tb_hz=1000000','tb_hz=0')+rows[0]]:
    try:m.analyze(bad)
    except ValueError:checks+=1
    else:raise AssertionError('invalid input accepted')
failure=m.analyze(header+rows[0].replace(',1,ok,',',0,timeout,'));assert failure['groups'][0]['valid_timings']==0 and not failure['complete'];checks+=1
legacy=m.analyze('test,bytes,ms,verified\nmemory_copy,1048576,10,1\nstarlet_device_missing,0,0,0\n');assert legacy['schema']=='legacy_unknown' and legacy['groups'][0]['nominal_MiB_s']==100 and not legacy['hardware_execution_confirmed'];checks+=1
print(f'PASS {checks} independent host kernel, corruption, full-period ring and CSV accounting/rejection checks; no Wii measurements')
