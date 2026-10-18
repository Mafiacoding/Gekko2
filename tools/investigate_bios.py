#!/usr/bin/env python3
"""Private ROMDIR inventory, optional OSDSYS disassembly and bounded cold boot.
Never publish its outputs: BIOS modules, RAM, frames and logs are proprietary
or private evidence. There is no guest patch, injected graphics or model hack.
"""
import argparse,hashlib,json,os,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def inventory(data):
    if len(data)!=4*1024*1024:raise ValueError('expected a 4 MiB BIOS image')
    for at in range(0,len(data)-16,16):
        if data[at:at+10].rstrip(b'\0')!=b'RESET':continue
        rows=[];offset=0
        for pos in range(at,min(at+16384,len(data)-15),16):
            name,ext,size=struct.unpack_from('<10sHI',data,pos);name=name.rstrip(b'\0')
            if not name:break
            if any(x<32 or x>126 for x in name) or size>len(data)-offset:break
            rows.append(dict(name=name.decode(),offset=offset,size=size,ext=ext));offset+=(size+15)&~15
        if any(x['name']=='ROMDIR' and x['offset']==at for x in rows):return rows
    raise ValueError('no valid bounded ROMDIR')
def disassemble_elf(data,destination):
    try:from capstone import Cs,CS_ARCH_MIPS,CS_MODE_MIPS64,CS_MODE_LITTLE_ENDIAN
    except ImportError:return {'status':'not_run_capstone_missing'}
    if len(data)<52 or data[:6]!=b'\x7fELF\1\1':return {'status':'not_ELF32_LE'}
    shoff=struct.unpack_from('<I',data,32)[0];stride,count=struct.unpack_from('<HH',data,46)
    if stride<40 or shoff+stride*count>len(data):raise ValueError('bad ELF section table')
    md=Cs(CS_ARCH_MIPS,CS_MODE_MIPS64|CS_MODE_LITTLE_ENDIAN);records=unknown=0
    with destination.open('w') as output:
        for i in range(count):
            _,typ,flags,address,off,size,*_=struct.unpack_from('<10I',data,shoff+i*stride)
            if not(flags&4) or typ!=1:continue
            if off+size>len(data) or size%4:raise ValueError('bad executable section')
            for n in range(0,size,4):
                chunk=data[off+n:off+n+4];decoded=list(md.disasm(chunk,address+n,count=1));records+=1
                if decoded:output.write(f'{address+n:08x} {decoded[0].mnemonic} {decoded[0].op_str}\n')
                else:unknown+=1;output.write(f'{address+n:08x} .word 0x{int.from_bytes(chunk,"little"):08x}\n')
    return {'status':'partial_generic_MIPS_not_R5900_MMI','records':records,'unknown_words':unknown}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bios',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--slices',type=int,default=0);p.add_argument('--resume',type=Path);p.add_argument('--input-plan',type=Path);a=p.parse_args()
    out=a.output.resolve()
    if out==root or root in out.parents:p.error('output must be outside the repository')
    if not 0<=a.slices<=1000:p.error('slices must be 0..1000')
    out.mkdir(parents=True,exist_ok=True);data=a.bios.read_bytes();rows=inventory(data)
    report={'sha256':hashlib.sha256(data).hexdigest(),'modules':rows,'private':True,'guest_patched':False,'physical_wii_verified':False}
    for row in rows:
        if row['name']=='OSDSYS':report['disassembly']=disassemble_elf(data[row['offset']:row['offset']+row['size']],out/'osdsys-disassembly.private.txt')
        if row['name']=='ROMVER':report['romver']=data[row['offset']:row['offset']+row['size']].rstrip(b'\0').decode('ascii',errors='replace')
    if a.slices:
        env=dict(os.environ);env['HOSTEXTRA']='-DGEKKO2_IDLE_SKIP -DGEKKO2_SPEED_STUB -DGEKKO2_SPEED_RETIRE -DGEKKO2_GAME_TRACE'
        built=subprocess.check_output([sys.executable,'tools/build_host_core.py','bios_private_capture'],cwd=root,env=env,text=True).strip()
        frames=out/'frames';frames.mkdir(exist_ok=True)
        env.update(GEKKO2_CAPTURE=str(frames),GEKKO2_OPTIONS='7ffd7fff',GEKKO2_PRIVATE_RAM=str(out/'ee-ram.private'),GEKKO2_PRIVATE_CHECKPOINT=str(out/'checkpoint.private'))
        for k in ('GEKKO2_RESUME','GEKKO2_INPUT','GEKKO2_PAD_PLAN'):env.pop(k,None)
        if a.resume:env['GEKKO2_RESUME']=str(a.resume.resolve())
        if a.input_plan:env['GEKKO2_PAD_PLAN']=str(a.input_plan.resolve())
        with (out/'run.private.log').open('w') as log:
            # build_host_core does not add time wrapping; explicitly relink below.
            cache=Path(built).parent;objects=sorted(cache.glob('*.o'));exe=out/'bios-capture'
            subprocess.run(['gcc','-O3','-w','-Iinclude','-Isource',*env['HOSTEXTRA'].split(),'-ffunction-sections','-fdata-sections','tools/bios_private_capture.c',*[str(x) for x in objects],'-Wl,--gc-sections','-Wl,--wrap=time','-lm','-o',str(exe)],cwd=root,check=True)
            run=subprocess.run([str(exe),str(a.bios.resolve()),'-',str(a.slices)],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
        report.update(run_exit=run.returncode,cold_boot=not bool(a.resume),slices=a.slices,frames=len(list(frames.glob('*.ppm'))))
    (out/'report.private.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Private BIOS experiment written outside repository; no hardware or tile-fix claim.')
