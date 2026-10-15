#!/usr/bin/env python3
"""Read-only directory inventory of a user-owned raw 2352-byte Mode2 PS2 CD.

Usage: python3 tools/audit_crash_disc_beta12_18.py path/to/crash.bin
Prints file paths only. Does not copy game assets or change the image.
"""
import sys

def inventory(path):
    with open(path, 'rb') as cd:
        def sector(n):
            cd.seek(n * 2352 + 24)
            return cd.read(2048)
        pvd = sector(16)
        if pvd[1:6] != b'CD001':
            raise ValueError('ISO9660 PVD missing at raw Mode2 sector 16')
        def directory(record, base='', depth=0):
            if depth > 10:
                return
            extent = int.from_bytes(record[2:6], 'little')
            size = int.from_bytes(record[10:14], 'little')
            if size > 512 * 1024 * 1024:
                raise ValueError('Unreasonable directory extent')
            data = b''.join(sector(extent + i) for i in range((size + 2047)//2048))[:size]
            pos = 0
            while pos < len(data):
                n = data[pos]
                if not n:
                    pos = (pos // 2048 + 1) * 2048
                    continue
                entry = data[pos:pos+n]
                pos += n
                if len(entry) < 34:
                    continue
                name = entry[33:33+entry[32]].decode('latin1','replace')
                if name in ('\x00','\x01'):
                    continue
                full = base + '/' + name
                if entry[25] & 2:
                    yield from directory(entry,full,depth+1)
                else:
                    yield full
        yield from directory(pvd[156:190])

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Usage: audit_crash_disc_beta12_18.py <user-owned raw Mode2 BIN>')
    names = set(inventory(sys.argv[1]))
    for label in ('/FONTS/ARIEL32.FNT;1','/FONTS/ARIEL32.BMP;1',
                  '/LEVELS/B/TITLES/TITLE.GSC;1','/LEVELS/B/TITLES/TITLE.NUS;1',
                  '/LEVELS/B/TITLES/TITLE.LGT;1'):
        print(('PRESENT' if label in names else 'MISSING')+' '+label)
    print('Total files:',len(names))
