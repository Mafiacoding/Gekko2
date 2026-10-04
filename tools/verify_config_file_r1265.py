"""Read-only check of the file produced by the controller-only Full-screen test.
Not a BIOS/firmware dump and never sets the initialized flag.
"""
import argparse,json,zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('file');p.add_argument('--screen',choices=['4:3','Full','16:9'],default='Full');a=p.parse_args();data=Path(a.file).read_bytes()
assert len(data)==344 and data[:4]==b'WNV1','invalid config format'
assert int.from_bytes(data[340:344],'little')==zlib.crc32(data[:340]),'invalid CRC'
# Shared storage has three banks of seven slots; bank 1 block 1 is the OSD config.
block=data[4+7*16+16:4+7*16+32]
assert sum(block[:15])&255==block[15],'invalid physical block checksum'
assert (block[0]>>1)&3==['4:3','Full','16:9'].index(a.screen),'OSDSYS screen setting was not written'
assert block[2]&0x80,'OSDSYS did not write its initialized flag'
print(json.dumps({'format':'WNV1','bytes':len(data),'file_crc_valid':True,'hardware_block_checksum_valid':True,'screen_size':a.screen,'initialized_flag_written_by_OSDSYS':True},indent=2))
