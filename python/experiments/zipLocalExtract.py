"""Extract members from a >4GB zip whose writer wrapped offsets
modulo 2^32 (python zipfile chokes). Reuses the probing logic from
zip_list/zip_fetch but on a local file."""
import os, struct, sys, zlib

def fetch(f, start, n):
    f.seek(start)
    return f.read(n)

def central_directory(f, size):
    tail_len = min(size, 22 + 65536 + 76)
    tail = fetch(f, size - tail_len, tail_len)
    eocd = tail.rfind(b"PK\x05\x06")
    n_total, cd_size, cd_offset = struct.unpack(
        "<HII", tail[eocd+10:eocd+12] + tail[eocd+12:eocd+20])
    eocd_abs = size - tail_len + eocd
    if 0xFFFFFFFF in (cd_offset, cd_size) or n_total == 0xFFFF:
        loc = tail.rfind(b"PK\x06\x07")
        z64o = struct.unpack("<Q", tail[loc+8:loc+16])[0]
        z64 = fetch(f, z64o, 56)
        n_total = struct.unpack("<Q", z64[32:40])[0]
        cd_size = struct.unpack("<Q", z64[40:48])[0]
        cd_offset = struct.unpack("<Q", z64[48:56])[0]
    for off in (cd_offset, eocd_abs - cd_size,
                *[cd_offset + k*0x100000000 for k in range(1,5)]):
        if 0 <= off and off + cd_size <= size:
            cd = fetch(f, off, cd_size)
            if cd[:4] == b"PK\x01\x02":
                return cd
    raise AssertionError("cd not found")

def entries(cd):
    pos = 0
    while pos + 46 <= len(cd) and cd[pos:pos+4] == b"PK\x01\x02":
        csize, usize, nlen, elen, clen = struct.unpack(
            "<IIHHH", cd[pos+20:pos+34])
        off = struct.unpack("<I", cd[pos+42:pos+46])[0]
        name = cd[pos+46:pos+46+nlen].decode("utf-8","replace")
        extra = cd[pos+46+nlen:pos+46+nlen+elen]
        e = 0
        while e + 4 <= len(extra):
            eid, esz = struct.unpack("<HH", extra[e:e+4])
            if eid == 1:
                data = extra[e+4:e+4+esz]; d2 = 0
                if usize == 0xFFFFFFFF:
                    usize = struct.unpack("<Q", data[d2:d2+8])[0]; d2 += 8
                if csize == 0xFFFFFFFF:
                    csize = struct.unpack("<Q", data[d2:d2+8])[0]; d2 += 8
                if off == 0xFFFFFFFF:
                    off = struct.unpack("<Q", data[d2:d2+8])[0]
            e += 4 + esz
        yield name, csize, usize, off
        pos += 46 + nlen + elen + clen

def extract(zpath, match, out_root):
    size = os.path.getsize(zpath)
    f = open(zpath, "rb")
    cd = central_directory(f, size)
    n = 0
    for name, csize, usize, off in entries(cd):
        if not match(name):
            continue
        lh = None
        for k in range(5):
            cand = off + k*0x100000000
            if cand + 30 > size:
                break
            probe = fetch(f, cand, 30)
            if probe[:4] == b"PK\x03\x04":
                off, lh = cand, probe
                break
        if lh is None:
            print("MISS", name); continue
        method = struct.unpack("<H", lh[8:10])[0]
        nlen, elen = struct.unpack("<HH", lh[26:30])
        raw = fetch(f, off + 30 + nlen + elen, csize)
        data = zlib.decompress(raw, -15) if method == 8 else raw
        outp = os.path.join(out_root, name)
        os.makedirs(os.path.dirname(outp), exist_ok=True)
        open(outp, "wb").write(data)
        n += 1
    print("extracted", n)

if __name__ == "__main__":
    zpath, out = sys.argv[1], sys.argv[2]
    extract(zpath, lambda n: "cameraImages" in n and
            n.lower().endswith(".jpg") and "__MACOSX" not in n, out)

