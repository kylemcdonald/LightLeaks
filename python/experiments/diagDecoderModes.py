"""Which per-bit decode rule agrees best with the 2019 production
decode? Variants:
  A raw-only            d = d_raw
  B dual max-margin     d = hp if |hp|>|raw| (current v2)
  C sign-guarded dual   hp only if it agrees in sign with raw, or raw
                        is too weak to carry a sign (<2 gray levels)
Compared per projector pixel against the archived 2019 proMap winner
(hand-masked production decode) for scan-1831."""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys
import numpy as np
import cv2
import OpenEXR
import Imath

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decode_v2 import load_axis, subpixel_refine, build_promap

HERE = os.path.dirname(os.path.abspath(__file__))
FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)
SCAN = 'scan-1831'
RAW = os.path.join(HERE, 'lan_raw', 'SharedData', SCAN)
OLD = os.path.join(HERE, 'lan', SCAN)
PW, PH = 7680, 1080


def read_exr(fn, channels):
    f = OpenEXR.InputFile(fn)
    dw = f.header()['dataWindow']
    w, hh = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
    out = [np.frombuffer(f.channel(c, FLOAT), np.float32).reshape(hh, w)
           for c in channels]
    return np.stack(out, axis=-1) if len(out) > 1 else out[0]


def decode_axis_variant(normal, inverse, mode, hp_radius=101,
                        drop_finest=2):
    n_levels, h, w = normal.shape
    use = max(3, n_levels - drop_finest)
    code = np.zeros((h, w), np.uint32)
    min_margin = np.full((h, w), np.inf, np.float32)
    for i in range(use):
        d_raw = normal[i] - inverse[i]
        lp = cv2.blur(d_raw, (hp_radius, hp_radius))
        d_hp = d_raw - lp
        if mode == 'A':
            d = d_raw
        elif mode == 'B':
            d = np.where(np.abs(d_hp) > np.abs(d_raw), d_hp, d_raw)
        else:  # C: sign-guarded
            agree = (d_hp * d_raw) >= 0
            weak = np.abs(d_raw) < 2.0
            use_hp = (np.abs(d_hp) > np.abs(d_raw)) & (agree | weak)
            d = np.where(use_hp, d_hp, d_raw)
        bit = (d > 0).astype(np.uint32)
        code = (code << 1) | bit
        min_margin = np.minimum(min_margin, np.abs(d))
    b = code.copy()
    shift = 1
    while shift < use:
        b ^= b >> shift
        shift <<= 1
    scale = 1 << (n_levels - use)
    return (b.astype(np.float32) + 0.5) * scale - 0.5, min_margin


OLD = os.path.join(HERE, 'lan', 'SharedData', SCAN)
pm_png = cv2.imread(os.path.join(OLD, 'proMap.png'),
                    cv2.IMREAD_UNCHANGED)
old_pm = np.dstack([pm_png[..., 2], pm_png[..., 1]]).astype(np.float32)
old_conf = read_exr(os.path.join(OLD, 'proConfidence.exr'), ['Y'])
print('old proMap', old_pm.shape, 'conf', old_conf.shape)

nv, iv = load_axis(RAW, 'vertical')
nh, ih = load_axis(RAW, 'horizontal')
for mode in ['A', 'B', 'C']:
    cx, mx = decode_axis_variant(nv, iv, mode)
    cy, my = decode_axis_variant(nh, ih, mode)
    conf = np.minimum(mx, my) / 50.0
    cx = np.minimum(cx, PW - 1)
    cy = np.minimum(cy, PH - 1)
    sx, sy = subpixel_refine(cx, cy, conf, 0.05)
    pro_cam, pro_conf = build_promap(sx, sy, conf, PW, PH)
    ok_new = pro_conf > 0.05
    ok_old = (np.abs(old_pm).sum(-1) > 1e-6)
    if old_conf is not None:
        ok_old &= old_conf > 0.1
    both = ok_new & ok_old
    # old proMap stores camera coords normalized? assume pixels
    d = np.linalg.norm(pro_cam - old_pm[..., :2], axis=-1)[both]
    print(f"mode {mode}: confident {100*(conf>0.05).mean():.1f}% cam, "
          f"pro cov {100*ok_new.mean():.1f}%, both {both.sum()}, "
          f"agree median {np.median(d):.2f}px, within3 "
          f"{100*(d<3).mean():.0f}%, within10 {100*(d<10).mean():.0f}%")
