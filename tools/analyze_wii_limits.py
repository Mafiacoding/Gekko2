#!/usr/bin/env python3
"""Summarize private benchmark CSVs; rates are workload-specific, never Wii limits."""
import argparse,csv,io,json,re,statistics
from pathlib import Path

def analyze(text):
    lines=text.splitlines(); data=[x for x in lines if x.strip() and not x.startswith('#')]
    if not data:raise ValueError('empty CSV')
    rows=list(csv.DictReader(io.StringIO('\n'.join(data))))
    if not rows:raise ValueError('no samples')
    output={'hardware_execution_confirmed':False,'warnings':[], 'groups':[]}
    if list(rows[0])==['test','bytes','ms','verified']:
        output['schema']='legacy_unknown';output['warnings']=[
            'No source/build, timer frequency, repetitions, resolution or workload parameters.',
            'bytes on stride rows may mean address span; GX bytes has no documented meaning.',
            'verified is a producer flag, not independently established hardware evidence.',
            'Do not derive peak bandwidth, CPU utilization, overlap efficiency or game FPS.']
        for row in rows:
            size=int(row['bytes']);ms=float(row['ms']);verified=int(row['verified'])
            if size<0 or not 0<=ms<float('inf') or verified not in (0,1):raise ValueError('invalid legacy row')
            output['groups'].append(dict(test=row['test'],reported_bytes=size,ms=ms,reported_verified=bool(verified),
                nominal_MiB_s=(size/(1024**2)/(ms/1000) if ms and size and not row['test'].startswith('gx_') else None),rate_meaning='ambiguous_workload'))
        return output
    required={'test','sample','pool','size_bytes','iterations','work_units','unit','ticks','submit_ticks','wait_ticks','verified','status','checksum'}
    if set(rows[0])!=required:raise ValueError('unsupported columns')
    header='\n'.join(x for x in lines if x.startswith('#'))
    match=re.search(r'\btb_hz=(\d+)\b',header)
    if not match or int(match[1])<=0:raise ValueError('missing positive tb_hz')
    hz=int(match[1]);output.update(schema=1,tb_hz=hz,metadata=header,complete='# end=finished' in header)
    if not output['complete']:output['warnings'].append('Missing finished footer: run cancelled, truncated or interrupted.')
    footer=re.search(r'# end=\w+ check_failures=(\d+) gx_timeout=(\d+)',header)
    if footer and (int(footer[1]) or int(footer[2])):output['warnings'].append('Run reports output-check failures or GX timeout; inspect affected samples.')
    output['warnings'].extend(['Microbenchmark rates are not theoretical hardware maxima or emulator FPS.',
        'submit_ticks includes CPU FIFO submission; total includes GPU completion and optional CPU work.',
        'verified is an output smoke check; it does not establish full GX conformance.'])
    groups={};seen=set()
    for row in rows:
        for key in ('sample','size_bytes','iterations','work_units','ticks','submit_ticks','wait_ticks','verified'):
            row[key]=int(row[key]);
            if row[key]<0:raise ValueError('negative '+key)
        if row['verified'] not in (0,1):raise ValueError('invalid verified flag')
        key=(row['test'],row['pool'],row['size_bytes'],row['unit']);identity=key+(row['sample'],)
        if identity in seen:raise ValueError('duplicate sample')
        seen.add(identity);groups.setdefault(key,[]).append(row)
        if row['submit_ticks']>row['ticks'] or row['wait_ticks']>row['ticks']:raise ValueError('component exceeds elapsed ticks')
    for (test,pool,size,unit),samples in groups.items():
        valid=[x for x in samples if x['ticks']>0 and ((x['status']=='ok' and x['verified']) or x['status']=='timing_only')]
        item=dict(test=test,pool=pool,size_bytes=size,unit=unit,samples=len(samples),valid_timings=len(valid),
                  verified_samples=sum(x['verified'] for x in samples),statuses=sorted(set(x['status'] for x in samples)))
        if valid:
            durations=[x['ticks']/hz for x in valid];rates=[x['work_units']/(x['ticks']/hz) for x in valid]
            item.update(median_ms=statistics.median(durations)*1000,min_ms=min(durations)*1000,max_ms=max(durations)*1000,
                        median_units_s=statistics.median(rates),median_submit_ms=statistics.median(x['submit_ticks']/hz*1000 for x in valid),
                        median_wait_ms=statistics.median(x['wait_ticks']/hz*1000 for x in valid))
            if unit=='payload_bytes':item['median_payload_MiB_s']=statistics.median(rates)/(1024**2)
            if unit=='loads':item['median_ns_load']=statistics.median(x['ticks']/hz/x['work_units']*1e9 for x in valid if x['work_units'])
        if len(samples)!=5:output['warnings'].append(f'{test}/{pool}/{size}: {len(samples)} of 5 samples.')
        if len(valid)!=len(samples):output['warnings'].append(f'{test}/{pool}/{size}: failures, timeouts or zero-duration results excluded.')
        output['groups'].append(item)
    return output

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('csv',type=Path);parser.add_argument('--output-json',type=Path);a=parser.parse_args()
    try:report=analyze(a.csv.read_text(encoding='utf-8-sig'));content=json.dumps(report,indent=2,allow_nan=False)+'\n'
    except (ValueError,KeyError,OSError,ZeroDivisionError) as e:parser.exit(1,str(e)+'\n')
    if a.output_json:a.output_json.write_text(content)
    print(content,end='')
