"""Intel HEX parser -> flat byte image + segment map."""
import sys, os, json

def parse(path):
    segs = []          # list of (base, bytearray)
    cur = None
    upper = 0
    eof = False
    with open(path, 'r', encoding='ascii', errors='replace') as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            if not line.startswith(':'):
                raise ValueError(f"line {ln}: not a record: {line!r}")
            raw = bytes.fromhex(line[1:])
            cnt, addr, typ = raw[0], (raw[1] << 8) | raw[2], raw[3]
            data = raw[4:4 + cnt]
            csum = raw[4 + cnt]
            if (sum(raw[:4 + cnt]) + csum) & 0xFF:
                raise ValueError(f"line {ln}: bad checksum")
            if typ == 0x00:
                base = upper + addr
                if cur is not None and cur[0] + len(cur[1]) == base:
                    cur[1].extend(data)
                else:
                    cur = (base, bytearray(data))
                    segs.append(cur)
            elif typ == 0x01:
                eof = True
                break
            elif typ == 0x02:
                upper = ((data[0] << 8) | data[1]) << 4
                cur = None
            elif typ == 0x04:
                upper = ((data[0] << 8) | data[1]) << 16
                cur = None
            elif typ == 0x03 or typ == 0x05:
                pass
            else:
                raise ValueError(f"line {ln}: unknown record type {typ}")
    return segs, eof

if __name__ == '__main__':
    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else None
    segs, eof = parse(path)
    total = sum(len(d) for _, d in segs)
    print(f"file={path} eof_record={eof} segments={len(segs)} bytes={total} (0x{total:X})")
    for base, data in segs:
        print(f"  {base:#010x} - {base+len(data)-1:#010x}  {len(data):6d} bytes")
    if out:
        lo = min(b for b, _ in segs)
        hi = max(b + len(d) for b, d in segs)
        img = bytearray(b'\xff' * (hi - lo))
        for b, d in segs:
            img[b - lo:b - lo + len(d)] = d
        with open(out, 'wb') as f:
            f.write(img)
        meta = {"base": lo, "size": len(img), "segments": [[b, len(d)] for b, d in segs]}
        with open(out + '.json', 'w') as f:
            json.dump(meta, f, indent=2)
        print(f"wrote {out}: base={lo:#x} size={len(img)} (0x{len(img):X})")
