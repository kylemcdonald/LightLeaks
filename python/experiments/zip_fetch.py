"""Extract selected members of a remote zip via GCS Range requests."""
import os
import struct
import sys
import zlib

from zip_list import fetch_range, central_directory, parse_entries, head_size


def extract(obj, names, out_root, listing=None):
    if listing is None:
        cd, _ = central_directory(obj)
        listing = list(parse_entries(cd))
    by_name = {n: (c, u, off) for n, c, u, off in listing}
    for name in names:
        csize, usize, off = by_name[name]
        out_path = os.path.join(out_root, name)
        if os.path.exists(out_path) and \
                os.path.getsize(out_path) == usize:
            print(f"cached {name}")
            continue
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        # local file header: 30 bytes fixed + name + extra.
        # >4GB archives written without zip64 wrap offsets mod 2^32:
        # probe successive 4GB shifts until the signature matches
        size = head_size(obj)
        lh = None
        for k in range(5):
            cand = off + k * 0x100000000
            if cand + 30 > size:
                break
            probe = fetch_range(obj, cand, cand + 29)
            if probe[:4] == b"PK\x03\x04":
                off, lh = cand, probe
                break
        assert lh is not None, f"bad local header for {name}"
        method = struct.unpack("<H", lh[8:10])[0]
        nlen, elen = struct.unpack("<HH", lh[26:30])
        data_start = off + 30 + nlen + elen
        raw = fetch_range(obj, data_start, data_start + csize - 1)
        if method == 8:
            raw = zlib.decompress(raw, -15)
        elif method != 0:
            raise Exception(f"unsupported compression {method}")
        assert len(raw) == usize, f"size mismatch for {name}"
        with open(out_path, "wb") as f:
            f.write(raw)
        print(f"fetched {name} ({usize/1e6:.1f} MB)")
    return listing


if __name__ == "__main__":
    obj, out_root = sys.argv[1], sys.argv[2]
    names = [line.strip() for line in sys.stdin if line.strip()]
    extract(obj, names, out_root)
