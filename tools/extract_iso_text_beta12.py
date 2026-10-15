#!/usr/bin/env python3
"""Extract text (.ATS and SYSTEM.CNF) from a user-owned PS2 ISO/BIN.

Reads logical ISO9660 2048-byte sectors from raw Mode 2 2352-byte CD images
or ordinary ISO images. No game bytes are embedded in this script or repo.
"""
import argparse
from pathlib import Path


def le32(data, pos):
    return int.from_bytes(data[pos:pos+4], "little")


class IsoReader:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.f.seek(0, 2)
        self.length = self.f.tell()
        self.stride = None
        self.offset = None
        for stride, off in ((2048, 0), (2352, 24), (2352, 16)):
            try:
                self.stride, self.offset = stride, off
                pvd = self.sector(16)
                if pvd[:7] == b"\x01CD001\x01":
                    self.root = (le32(pvd, 158), le32(pvd, 166))
                    return
            except ValueError:
                pass
        self.f.close()
        raise ValueError("not an ISO9660 image with supported sector layout")

    def sector(self, lba):
        start = lba * self.stride + self.offset
        if lba < 0 or start + 2048 > self.length:
            raise ValueError(f"LBA outside image: {lba}")
        self.f.seek(start)
        b = self.f.read(2048)
        if len(b) != 2048:
            raise ValueError("short sector")
        return b

    def read(self, lba, size, cap=65536):
        if size < 0 or size > cap:
            raise ValueError("oversized text payload")
        return b"".join(self.sector(lba+n) for n in range((size+2047)//2048))[:size]

    def entries(self, lba, size, prefix="", depth=0):
        if depth >= 12 or size > 16*1024*1024:
            raise ValueError("unexpected directory depth/size")
        for n in range((size+2047)//2048):
            sector = self.sector(lba+n)
            pos = 0
            while pos < 2048:
                record_size = sector[pos]
                if not record_size:
                    break
                if record_size < 34 or pos + record_size > 2048:
                    raise ValueError("invalid ISO9660 directory record")
                d = sector[pos:pos+record_size]
                pos += record_size
                name_len = d[32]
                if 33+name_len > record_size:
                    raise ValueError("invalid ISO9660 name length")
                name = d[33:33+name_len].decode("ascii", "replace")
                if name in ("\x00", "\x01"):
                    continue
                target = prefix + name
                extent = le32(d, 2)
                length = le32(d, 10)
                if d[25] & 2:
                    yield from self.entries(extent, length, target + "/", depth+1)
                else:
                    yield target, extent, length


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", help="Your own PS2 ISO or raw BIN image")
    parser.add_argument("--out", required=True, help="Output text report path")
    args = parser.parse_args()
    disc = IsoReader(args.image)
    entries = list(disc.entries(*disc.root))
    lines = ["PS2 Disc – Textauszug aus SYSTEM.CNF und .ATS-Skripten",
             "Extrahiert aus einer vom Nutzer bereitgestellten Disc-Datei.",
             f"Physische Sektoren: {disc.stride} Bytes, Nutzdaten-Offset: {disc.offset}",
             f"ISO9660-Dateien: {len(entries)}", ""]
    names = []
    for name, lba, size in entries:
        if name.upper().split(";")[0].endswith((".ATS", ".CNF")):
            try:
                data = disc.read(lba, size)
            except ValueError as exc:
                lines.append(f"[SKIP] {name}: {exc}")
                continue
            text = data.decode("cp1252", "replace")
            lines += ["="*64, f"{name} – {size} Bytes", "="*64, text.rstrip(), ""]
            names.append(name)
    lines += ["-- Textdateien: " + str(len(names)) + " --"]
    Path(args.out).write_text("\n".join(lines), encoding="utf-8")
    print(f"PASS: extrahiert {len(names)} Textdateien aus {len(entries)} Disc-Dateien nach {args.out}")
    disc.f.close()


if __name__ == "__main__":
    main()
