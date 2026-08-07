"""Re-decode all LAN raw scans with mode A (raw-only differencing)
and mode C (sign-guarded dual path) in one pass over the images.
The shipped dual max-margin rule (mode B) confidently flips bits
where the high-pass difference opposes the raw difference (bright
surround pushes lp past d_raw), teleporting codes: median 47px
disagreement with the 2019 production decode vs 4.2px for raw-only.

Outputs per scan: camCode{A,C}.npy, camConf{A,C}.npy,
proMap{A,C}.npy, proConf{A,C}.npy
"""
import os
import sys
import json
import numpy as np
import cv2

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decode_v2 import load_axis, subpixel_refine, build_promap

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, 'lan_raw', 'SharedData')


def decode_axis_ac(normal, inverse, hp_radius=101, drop_finest=2):
    n_levels, h, w = normal.shape
    use = max(3, n_levels - drop_finest)
    codes = {m: np.zeros((h, w), np.uint32) for m in 'AC'}
    margins = {m: np.full((h, w), np.inf, np.float32) for m in 'AC'}
    for i in range(use):
        d_raw = normal[i] - inverse[i]
        lp = cv2.blur(d_raw, (hp_radius, hp_radius))
        d_hp = d_raw - lp
        agree = (d_hp * d_raw) >= 0
        weak = np.abs(d_raw) < 2.0
        use_hp = (np.abs(d_hp) > np.abs(d_raw)) & (agree | weak)
        for m, d in (('A', d_raw),
                     ('C', np.where(use_hp, d_hp, d_raw))):
            bit = (d > 0).astype(np.uint32)
            codes[m] = (codes[m] << 1) | bit
            margins[m] = np.minimum(margins[m], np.abs(d))
    out = {}
    scale = 1 << (n_levels - use)
    for m in 'AC':
        b = codes[m].copy()
        shift = 1
        while shift < use:
            b ^= b >> shift
            shift <<= 1
        out[m] = ((b.astype(np.float32) + 0.5) * scale - 0.5,
                  margins[m])
    return out


def main():
    with open(os.path.join(ROOT, 'settings.json')) as f:
        settings = json.load(f)
    pw = settings['projectors'][0]['width']
    ph = settings['projectors'][0]['height']
    scans = sorted(d for d in os.listdir(ROOT)
                   if os.path.isdir(os.path.join(ROOT, d,
                                                 'cameraImages')))
    print(f"{len(scans)} scans", flush=True)
    for s in scans:
        d = os.path.join(ROOT, s)
        if os.path.exists(os.path.join(d, 'proMapC.npy')):
            print(f"{s}: done already", flush=True)
            continue
        nv, iv = load_axis(d, 'vertical')
        vx = decode_axis_ac(nv, iv)
        del nv, iv
        nh, ih = load_axis(d, 'horizontal')
        vy = decode_axis_ac(nh, ih)
        del nh, ih
        for m in 'AC':
            cx, mx = vx[m]
            cy, my = vy[m]
            conf = np.minimum(mx, my) / 50.0
            cx = np.minimum(cx, pw - 1)
            cy = np.minimum(cy, ph - 1)
            sx, sy = subpixel_refine(cx, cy, conf, 0.05)
            np.save(os.path.join(d, f'camCode{m}.npy'),
                    np.dstack([sx, sy]).astype(np.float32))
            np.save(os.path.join(d, f'camConf{m}.npy'),
                    conf.astype(np.float32))
            pro_cam, pro_conf = build_promap(sx, sy, conf, pw, ph)
            np.save(os.path.join(d, f'proMap{m}.npy'),
                    pro_cam.astype(np.float32))
            np.save(os.path.join(d, f'proConf{m}.npy'),
                    pro_conf.astype(np.float32))
            print(f"{s} mode {m}: {100*(conf>0.05).mean():.1f}% cam, "
                  f"{100*(pro_conf>0.05).mean():.1f}% pro", flush=True)


if __name__ == '__main__':
    main()
