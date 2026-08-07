"""Hill-climb the decoder: score variants by how well their CODES agree
with the 2019 decode (which is known to reconstruct correctly).

Ground truth for this test: the 2019 proMap gives, for each projector pixel,
the camera pixel it won. Invert it -> that camera pixel's true code. Compare
each variant's code at those camera pixels.
"""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import numpy as np, cv2, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decode_v2 import load_axis

S = 'scan-1831'
PW = 7680

# --- truth: camera pixel -> projector x, from the 2019 map ---
pm = cv2.imread(f'lan/SharedData/{S}/proMap.png', cv2.IMREAD_UNCHANGED)
ox = pm[..., 2].astype(np.int32); oy = pm[..., 1].astype(np.int32)
ok = (ox > 0) | (oy > 0)
py, px = np.nonzero(ok)
cxr, cyr = ox[ok], oy[ok]

nv, iv = load_axis(f'lan_raw/SharedData/{S}', 'vertical')
n_levels = nv.shape[0]
H, W = nv.shape[1], nv.shape[2]
v = (cxr >= 0) & (cxr < W) & (cyr >= 0) & (cyr < H)
cxr, cyr, px = cxr[v], cyr[v], px[v]
print(f'{n_levels} levels, image {W}x{H}, truth pixels {len(px)}')

# subsample for speed
rng = np.random.default_rng(0)
idx = rng.choice(len(px), 400000, replace=False)
cxr, cyr, px = cxr[idx], cyr[idx], px[idx]


def decode(mode, drop, reverse=False, hp_radius=101):
    normal, inverse = (nv[::-1], iv[::-1]) if reverse else (nv, iv)
    use = max(3, n_levels - drop)
    code = np.zeros((H, W), np.uint32)
    mm = np.full((H, W), np.inf, np.float32)
    for i in range(use):
        d_raw = normal[i] - inverse[i]
        if mode == 'A':
            d = d_raw
        else:
            lp = cv2.blur(d_raw, (hp_radius, hp_radius)); d_hp = d_raw - lp
            if mode == 'B':
                d = np.where(np.abs(d_hp) > np.abs(d_raw), d_hp, d_raw)
            else:
                agree = (d_hp * d_raw) >= 0; weak = np.abs(d_raw) < 2.0
                d = np.where((np.abs(d_hp) > np.abs(d_raw)) & (agree | weak),
                             d_hp, d_raw)
        code = (code << 1) | (d > 0).astype(np.uint32)
        mm = np.minimum(mm, np.abs(d))
    b = code.copy(); sh = 1
    while sh < use:
        b ^= b >> sh; sh <<= 1
    scale = 1 << (n_levels - use)
    return (b.astype(np.float32) + 0.5) * scale - 0.5, mm / 50.0


VARIANTS = [
    ('C drop2 (current)', dict(mode='C', drop=2)),
    ('A drop2',           dict(mode='A', drop=2)),
    ('C drop0',           dict(mode='C', drop=0)),
    ('A drop0',           dict(mode='A', drop=0)),
    ('A drop0 REVERSED',  dict(mode='A', drop=0, reverse=True)),
    ('C drop1',           dict(mode='C', drop=1)),
    ('A drop0 hp301',     dict(mode='A', drop=0, hp_radius=301)),
]
print(f'{"variant":22s}{"agree<8px":>11s}{"agree<32px":>12s}{"conf cov":>10s}')
for name, kw in VARIANTS:
    c, conf = decode(**kw)
    myx = c[cyr, cxr]; cf = conf[cyr, cxr]
    sel = cf > 0.05
    if sel.sum() < 100:
        print(f'{name:22s}  (no confident pixels)'); continue
    d = np.abs(myx[sel] - px[sel])
    print(f'{name:22s}{100*(d<8).mean():10.1f}%{100*(d<32).mean():11.1f}%'
          f'{100*sel.mean():9.1f}%')
