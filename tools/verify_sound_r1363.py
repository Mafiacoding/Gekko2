"""Linked PPC sound pipeline: actual registers, DMA, clocks and queued PCM.
Platform output hardware is not started; no Wii audible playback claimed.
"""
from pathlib import Path
text=Path(__file__).with_name('verify_scheduler_window_r1349.py').read_text()
exec(compile(text.split('for grant in [')[0], 'scheduler_fixture','exec'))
extension_setup()
u.mem_write(ist,bytes(isize));word(ist+io['ram'],iram);word(ist+io['ram_size'],0x200000)
call('iop_dma_bind_iop_ram',iram,0x200000)
call('iop_intc_init');call('iop_spu2_init');call('iop_dma_init')
mask=syms['gekko2_optimization_mask'];word(mask,int.from_bytes(u.mem_read(mask,4),'big')|(1<<31))
def wr(core,reg,value):assert call('iop_spu2_mmio_write16',0x1f900000+core*0x400+reg,value)==1
def dma(core,madr,words):
 b=0x1f801500 if core else 0x1f8010c0
 for off,val in [(0,madr),(4,0x10000|words),(8,0x01000201)]:assert call('iop_dma_mmio_write32',b+off,val)==1
spu=call('spu2_mixer_get_ram')
u.mem_write(iram+0x100,bytes(range(16)));wr(0,0x1aa,0x4000);dma(0,0x100,4)
assert bytes(u.mem_read(spu+0x8000,16))==bytes(range(16))
assert call('iop_dma_get_spu2_transfer_count')==1
# Three split-stereo chunks cross both ring halves and refill the first.
call('iop_spu2_init');call('iop_dma_init')
wr(1,0x1b0,2);wr(1,0x198,0xc0)
for off,val in [(0x788,0x3fff),(0x78a,0x3fff),(0x794,0x7fff),(0x796,0x7fff)]:assert call('iop_spu2_mmio_write16',0x1f900000+off,val)==1
for chunk in range(3):u.mem_write(iram+0x1000+chunk*1024,struct.pack('<256h',*([1000*(chunk+1)]*256))+struct.pack('<256h',*([-1000*(chunk+1)]*256)))
dma(1,0x1000,768)
for frames in [767,768]:
 ticks=frames*768;u.mem_write(ist+io['sched_ticks'],struct.pack('>Q',ticks));call('iop_spu2_sync',0,ticks)
 assert call('iop_dma_get_spu2_transfer_count')==(frames==768)
call('audio_output_get_info',0x817b0000)
queued,submitted,played,dropped,underrun,nonzero,running=struct.unpack('>7I',u.mem_read(0x817b0000,28))
assert queued==submitted==nonzero==768 and not played and not dropped and not running
assert int.from_bytes(u.mem_read(syms['read_pos'],4),'big')==0
out=struct.unpack('>1536h',u.mem_read(syms['queue'],3072))
for i in range(768):
 expected=(i//256+1)*1000;assert abs(out[i*2]-expected)<4 and abs(out[i*2+1]+expected)<4,(i,out[i*2:i*2+2])
assert call('iop_spu2_audio_stat',0)==768
print('PASS actual PPC DMA4 word upload, ADMA7 split stereo, 768 guest-clock samples, final-frame completion and PCM queue')

# Silent output-buffer write still requests the guest's programmed IRQ.
u.mem_write(ist+io['sched_ticks'],bytes(8));call('iop_spu2_init');call('iop_intc_init')
wr(0,0x19e,0x800);wr(0,0x19a,0x40)
intc=call('iop_intc_get_state')
for ticks in [767,768]:
 u.mem_write(ist+io['sched_ticks'],struct.pack('>Q',ticks));call('iop_spu2_sync',0,ticks)
 assert bool(int.from_bytes(u.mem_read(intc,4),'big')&(1<<9))==(ticks==768)
print('PASS actual PPC silent mixer-output capture IRQ at 768 guest IOP cycles')
