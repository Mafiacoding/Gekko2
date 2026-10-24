#!/usr/bin/env python3
"""Convert a Wii/GameCube DOL to a big-endian PowerPC ELF32 (for Dolphin)."""
import struct, sys

def main(src, dst):
    d = open(src, 'rb').read()
    h = struct.unpack('>54I', d[:216])
    offs, addrs, sizes = h[0:18], h[18:36], h[36:54]
    bss_addr, bss_size, entry = struct.unpack('>3I', d[0xd8:0xe4])
    segs = [(offs[i], addrs[i], sizes[i]) for i in range(18) if sizes[i]]
    phnum = len(segs) + (1 if bss_size else 0)
    data_off = 52 + 32 * phnum
    ph, blob, pos = b'', b'', data_off
    for o, a, s in segs:
        ph += struct.pack('>8I', 1, pos, a, a, s, s, 7, 4)
        blob += d[o:o + s]
        pos += s
    if bss_size:
        ph += struct.pack('>8I', 1, pos, bss_addr, bss_addr, 0, bss_size, 6, 4)
    eh = b'\x7fELF\x01\x02\x01' + b'\0' * 9
    eh += struct.pack('>HHIIIIIHHHHHH', 2, 20, 1, entry, 52, 0, 0, 52, 32, phnum, 40, 0, 0)
    open(dst, 'wb').write(eh + ph + blob)

if __name__ == '__main__':
    main(*sys.argv[1:3])
