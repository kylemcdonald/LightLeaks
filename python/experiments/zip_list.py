"""List a remote zip's members with two bounded GCS Range requests.

Parses the end-of-central-directory (zip64-aware) and the central
directory manually - no zipfile, so no risk of unbounded reads.
"""
import os
import ssl
import struct
import sys
import urllib.request

TOKEN_PATH = os.path.join(os.path.dirname(__file__), "gcs_token")
CA_BUNDLE = "/root/.ccr/ca-bundle.crt"
BUCKET = "lightleaks-data"
_ctx = ssl.create_default_context(cafile=CA_BUNDLE)


def fetch_range(obj, start, end):
    req = urllib.request.Request(
        f"https://storage.googleapis.com/{BUCKET}/{obj}")
    with open(TOKEN_PATH) as f:
        req.add_header("Authorization", f"Bearer {f.read().strip()}")
    req.add_header("Range", f"bytes={start}-{end}")
    with urllib.request.urlopen(req, context=_ctx) as r:
        return r.read()


def head_size(obj):
    req = urllib.request.Request(
        f"https://storage.googleapis.com/{BUCKET}/{obj}", method="HEAD")
    with open(TOKEN_PATH) as f:
        req.add_header("Authorization", f"Bearer {f.read().strip()}")
    with urllib.request.urlopen(req, context=_ctx) as r:
        return int(r.headers["Content-Length"])


def central_directory(obj):
    size = head_size(obj)
    tail_len = min(size, 22 + 65536 + 76)  # EOCD + max comment + zip64 recs
    tail = fetch_range(obj, size - tail_len, size - 1)

    eocd_pos = tail.rfind(b"PK\x05\x06")
    assert eocd_pos >= 0, "no end-of-central-directory found"
    n_total, cd_size, cd_offset = struct.unpack(
        "<HII", tail[eocd_pos + 10:eocd_pos + 12] +
        tail[eocd_pos + 12:eocd_pos + 20])

    eocd_abs = size - tail_len + eocd_pos
    if cd_offset == 0xFFFFFFFF or n_total == 0xFFFF or \
            cd_size == 0xFFFFFFFF:
        loc_pos = tail.rfind(b"PK\x06\x07")
        assert loc_pos >= 0, "zip64 locator not found"
        z64_eocd_offset = struct.unpack(
            "<Q", tail[loc_pos + 8:loc_pos + 16])[0]
        z64 = fetch_range(obj, z64_eocd_offset, z64_eocd_offset + 55)
        assert z64[:4] == b"PK\x06\x06"
        n_total = struct.unpack("<Q", z64[32:40])[0]
        cd_size = struct.unpack("<Q", z64[40:48])[0]
        cd_offset = struct.unpack("<Q", z64[48:56])[0]

    # some writers of >4GB non-zip64 archives store the offset wrapped
    # modulo 2^32; validate and fall back to deriving the offset from
    # the EOCD's own position
    def try_cd(off):
        if off < 0 or off + cd_size > size:
            return None
        cd = fetch_range(obj, off, off + cd_size - 1)
        return cd if cd[:4] == b"PK\x01\x02" else None

    for off in (cd_offset, eocd_abs - cd_size,
                *[cd_offset + k * 0x100000000 for k in range(1, 5)]):
        cd = try_cd(off)
        if cd is not None:
            return cd, n_total
    raise AssertionError("central directory not found at any "
                         "candidate offset")


def parse_entries(cd):
    """Yield (name, compressed_size, uncompressed_size, header_offset)."""
    pos = 0
    while pos + 46 <= len(cd) and cd[pos:pos + 4] == b"PK\x01\x02":
        (csize, usize, nlen, elen, clen) = struct.unpack(
            "<IIHHH", cd[pos + 20:pos + 34])
        header_offset = struct.unpack("<I", cd[pos + 42:pos + 46])[0]
        name = cd[pos + 46:pos + 46 + nlen].decode("utf-8", "replace")
        extra = cd[pos + 46 + nlen:pos + 46 + nlen + elen]
        # zip64 extra field overrides 0xFFFFFFFF placeholders
        e = 0
        while e + 4 <= len(extra):
            eid, esz = struct.unpack("<HH", extra[e:e + 4])
            if eid == 1:
                data = extra[e + 4:e + 4 + esz]
                d = 0
                if usize == 0xFFFFFFFF:
                    usize = struct.unpack("<Q", data[d:d + 8])[0]
                    d += 8
                if csize == 0xFFFFFFFF:
                    csize = struct.unpack("<Q", data[d:d + 8])[0]
                    d += 8
                if header_offset == 0xFFFFFFFF:
                    header_offset = struct.unpack("<Q", data[d:d + 8])[0]
            e += 4 + esz
        yield name, csize, usize, header_offset
        pos += 46 + nlen + elen + clen


if __name__ == "__main__":
    obj = sys.argv[1]
    cd, n = central_directory(obj)
    print(f"# central directory {len(cd)/1e6:.1f} MB, {n} entries",
          file=sys.stderr)
    for name, csize, usize, off in parse_entries(cd):
        print(f"{usize:>14,}  {off:>14}  {name}")
